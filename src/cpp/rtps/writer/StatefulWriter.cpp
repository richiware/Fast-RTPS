// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file StatefulWriter.cpp
 *
 */

#include "StatefulWriterImpl.h"
#include <fastrtps/rtps/writer/ReaderProxy.h>
#include <fastrtps/rtps/resources/AsyncWriterThread.h>
#include "../participant/RTPSParticipantImpl.h"
#include "../flowcontrol/FlowController.h"
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/rtps/messages/RTPSMessageGroup.h>
#include <fastrtps/rtps/writer/timedevent/PeriodicHeartbeat.h>
#include "timedevent/NackSupressionDuration.h"
#include <fastrtps/rtps/writer/timedevent/NackResponseDelay.h>
#include <fastrtps/rtps/writer/timedevent/InitialHeartbeat.h>
#include "timedevent/CleanupEvent.h"
#include "../history/WriterHistoryImpl.h"
#include <fastrtps/log/Log.h>
#include <fastrtps/utils/TimeConversion.h>
#include "RTPSWriterCollector.h"
#include "StatefulWriterOrganizer.h"

#include <mutex>
#include <vector>

using namespace eprosima::fastrtps::rtps;

StatefulWriter::StatefulWriter(RTPSParticipant& participant,
        const WriterAttributes& att, WriterHistory& hist, WriterListener* listen) :
    RTPSWriter(participant, att, hist, listen)
{
    //TODO(ricardo) View check attributes are RELIABLE.
}

StatefulWriter::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid,
        const WriterAttributes& att, WriterHistory::impl& hist, RTPSWriter::impl::Listener* listener) :
    RTPSWriter::impl(participant, guid, att, hist, listener),
    m_times(att.times), all_acked_(false), disable_heartbeat_piggyback_(att.disableHeartbeatPiggyback),
    sendBufferSize_(participant.get_min_network_send_buffer_size()),
    currentUsageSendBufferSize_(static_cast<int32_t>(participant.get_min_network_send_buffer_size())),
    periodic_heartbeat_(nullptr), cleanup_event_(nullptr)
{
    m_heartbeatCount = 0;
    if(guid.entityId == c_EntityId_SEDPPubWriter)
    {
        m_HBReaderEntityId = c_EntityId_SEDPPubReader;
    }
    else if(guid.entityId == c_EntityId_SEDPSubWriter)
    {
        m_HBReaderEntityId = c_EntityId_SEDPSubReader;
    }
    else if(guid.entityId == c_EntityId_WriterLiveliness)
    {
        m_HBReaderEntityId= c_EntityId_ReaderLiveliness;
    }
    else
    {
        m_HBReaderEntityId = c_EntityId_Unknown;
    }
}

StatefulWriter::impl::~impl()
{
    deinit_();
}

void StatefulWriter::impl::deinit()
{
    deinit_();
}

bool StatefulWriter::impl::init()
{
    periodic_heartbeat_ = new PeriodicHeartbeat(*this, TimeConv::Time_t2MilliSecondsDouble(m_times.heartbeatPeriod));

    if(att_.durabilityKind == VOLATILE)
    {
        cleanup_event_ = new CleanupEvent(*this, 0);
    }

    return true;
}

void StatefulWriter::impl::deinit_()
{
    for(auto remote_reader : matched_readers_)
    {
        remote_reader->destroy_timers();
    }

    if(periodic_heartbeat_ != nullptr)
    {
        delete(periodic_heartbeat_);
        periodic_heartbeat_ = nullptr;
    }

    for(auto remote_reader : matched_readers_)
    {
        delete(remote_reader);
    }
    matched_readers_.clear();

    RTPSWriter::impl::deinit_();
}


/**
 * This method must be used only by WriterHistory. WriterHistory's mutex should have been taken.
 * On debug this precondition is check.
 */
bool StatefulWriter::impl::unsent_change_added_to_history(const CacheChange_t& change)
{
#if defined(__DEBUG)
    assert(history_.get_mutex_owner() == history_.get_thread_id());
#endif

    std::lock_guard<std::mutex> guard(mutex_);

    //TODO Think about when set liveliness assertion when writer is asynchronous.
    this->setLivelinessAsserted(true);

    if(!matched_readers_.empty())
    {
        if(!isAsync())
        {
            //TODO(Ricardo) Temporal.
            bool expectsInlineQos = false;

            for(auto remote_reader : matched_readers_)
            {
                ChangeForReader_t changeForReader(change);

                // TODO(Ricardo) Study next case: Not push mode, writer reliable and reader besteffort.
                if(push_mode_)
                {
                    if(remote_reader->m_att.endpoint.reliabilityKind == RELIABLE)
                    {
                        changeForReader.set_status(UNDERWAY);
                    }
                    else
                    {
                        changeForReader.set_status(ACKNOWLEDGED);
                    }
                }
                else
                {
                    changeForReader.set_status(UNACKNOWLEDGED);
                }

                remote_reader->mp_mutex->lock();
                changeForReader.set_relevance(remote_reader->rtps_is_relevant(change));
                remote_reader->add_change(changeForReader);
                expectsInlineQos |= remote_reader->m_att.expectsInlineQos;
                remote_reader->mp_mutex->unlock();

                if(remote_reader->mp_nackSupression != nullptr) // It is reliable
                {
                    remote_reader->mp_nackSupression->restart_timer();
                }
            }

            RTPSMessageGroup group(participant_, this,  RTPSMessageGroup::WRITER, m_cdrmessages);
            if(!group.add_data(change, mAllRemoteReaders, mAllShrinkedLocatorList, expectsInlineQos))
            {
                logError(RTPS_WRITER, "Error sending change " << change.sequence_number);
            }

            // Heartbeat piggyback.
            if(!disable_heartbeat_piggyback_)
            {
            /*
                if(mp_history->isFull())
                {
                    send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group);
                }
                else
                {
                    currentUsageSendBufferSize_ -= group.get_current_bytes_processed();

                    if(currentUsageSendBufferSize_ < 0)
                    {
                        send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group);
                    }
                }
            */

                //TODO(Ricardo) Revisar cambio
                currentUsageSendBufferSize_ -= group.get_current_bytes_processed();

                if(currentUsageSendBufferSize_ < 0)
                {
                    send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group,
                            history_.get_min_sequence_number_nts(), history_.get_max_sequence_number_nts());
                }
            }

            periodic_heartbeat_->restart_timer();
        }
        else
        {
            for(auto remote_reader : matched_readers_)
            {
                ChangeForReader_t changeForReader(change);

                if(push_mode_)
                {
                    changeForReader.set_status(UNSENT);
                }
                else
                {
                    changeForReader.set_status(UNACKNOWLEDGED);
                }

                std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);
                changeForReader.set_relevance(remote_reader->rtps_is_relevant(change));
                remote_reader->add_change(changeForReader);
            }
        }
    }
    else
    {
        logInfo(RTPS_WRITER,"No reader proxy to add change.");
    }

    return true;
}


/**
 * This method must be used only by WriterHistory. WriterHistory's mutex should have been taken.
 * On debug this precondition is check.
 */
bool StatefulWriter::impl::change_removed_by_history(const SequenceNumber_t& sequence_number,
        const InstanceHandle_t&)
{
#if defined(__DEBUG)
    assert(history_.get_mutex_owner() == history_.get_thread_id());
#endif

    logInfo(RTPS_WRITER,"CacheChange "<< sequence_number << " to be removed.");

    std::lock_guard<std::mutex> guard(mutex_);

    // Invalidate CacheChange pointer in ReaderProxies.
    for(auto remote_reader : matched_readers_)
    {
        remote_reader->setNotValid(sequence_number);
    }

    return true;
}

/*
 * This function must to be call only from AsyncWriterThread.
 */
void StatefulWriter::impl::send_any_unsent_changes()
{
    std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());

    RTPSWriterCollector<ReaderProxy*> relevantChanges;
    StatefulWriterOrganizer notRelevantChanges;

    std::unique_lock<std::mutex> lock(mutex_);
    for(auto remoteReader : matched_readers_)
    {
        std::lock_guard<std::recursive_mutex> rguard(*remoteReader->mp_mutex);
        std::vector<ChangeForReader_t*> unsent_changes = remoteReader->get_unsent_changes();

        for(auto unsent_change : unsent_changes)
        {
            if(unsent_change->is_relevant())
            {
                if(push_mode_)
                {
                    const CacheChange_t* const cachechange =
                        history_.get_change_nts(unsent_change->get_sequence_number());
                    assert(cachechange != nullptr);
                    relevantChanges.add_change(cachechange, remoteReader, unsent_change->get_unsent_fragments());
                }
                else // Change status to UNACKNOWLEDGED
                {
                    remoteReader->set_change_to_status(unsent_change->get_sequence_number(), UNACKNOWLEDGED);
                }
            }
            else
            {
                notRelevantChanges.add_sequence_number(unsent_change->get_sequence_number(), remoteReader);
                remoteReader->set_change_to_status(unsent_change->get_sequence_number(), UNDERWAY); //TODO(Ricardo) Review
            }
        }
    }



    if(push_mode_)
    {
        // Clear all relevant changes through the local controllers first
        for (auto& controller : m_controllers)
            (*controller)(relevantChanges);

        // Clear all relevant changes through the parent controllers
        for (auto& controller : participant_.getFlowControllers())
            (*controller)(relevantChanges);

        RTPSMessageGroup group(participant_, this,  RTPSMessageGroup::WRITER, m_cdrmessages);
        bool activateHeartbeatPeriod = false;
        uint32_t lastBytesProcessed = 0;

        while(!relevantChanges.empty())
        {
            RTPSWriterCollector<ReaderProxy*>::Item changeToSend = relevantChanges.pop();
            std::vector<GUID_t> remote_readers;
            std::vector<LocatorList_t> locatorLists;
            bool expectsInlineQos = false;

            for(auto remoteReader : changeToSend.remote_readers)
            {
                remote_readers.push_back(remoteReader->m_att.guid);
                LocatorList_t locators(remoteReader->m_att.endpoint.unicastLocatorList);
                locators.push_back(remoteReader->m_att.endpoint.multicastLocatorList);
                locatorLists.push_back(locators);
                expectsInlineQos |= remoteReader->m_att.expectsInlineQos;
            }

            // TODO(Ricardo) Flowcontroller has to be used in RTPSMessageGroup. Study.
            // And controllers are notified about the changes being sent
            FlowController::NotifyControllersChangeSent(changeToSend.cachechange);

            if(changeToSend.fragment_number != 0)
            {
                if(group.add_data_frag(*changeToSend.cachechange, changeToSend.fragment_number, remote_readers,
                            participant_.network_factory().ShrinkLocatorLists(locatorLists),
                            expectsInlineQos))
                {
                    for(auto remoteReader : changeToSend.remote_readers)
                    {
                        std::lock_guard<std::recursive_mutex> rguard(*remoteReader->mp_mutex);
                        bool allFragmentsSent = remoteReader->mark_fragment_as_sent_for_change(changeToSend.sequence_number, changeToSend.fragment_number);

                        if(remoteReader->m_att.endpoint.reliabilityKind == RELIABLE)
                        {
                            activateHeartbeatPeriod = true;
                            assert(remoteReader->mp_nackSupression != nullptr);
                            if(allFragmentsSent)
                                remoteReader->mp_nackSupression->restart_timer();
                        }
                    }
                }
                else
                {
                    logError(RTPS_WRITER, "Error sending fragment (" << changeToSend.sequence_number <<
                            ", " << changeToSend.fragment_number << ")");
                }
            }
            else
            {
                if(group.add_data(*changeToSend.cachechange, remote_readers,
                            participant_.network_factory().ShrinkLocatorLists(locatorLists),
                            expectsInlineQos))
                {
                    for(auto remoteReader : changeToSend.remote_readers)
                    {
                        std::lock_guard<std::recursive_mutex> rguard(*remoteReader->mp_mutex);
                        if(remoteReader->m_att.endpoint.reliabilityKind == RELIABLE)
                        {
                            remoteReader->set_change_to_status(changeToSend.sequence_number, UNDERWAY);
                            activateHeartbeatPeriod = true;
                            assert(remoteReader->mp_nackSupression != nullptr);
                            remoteReader->mp_nackSupression->restart_timer();
                        }
                        else
                        {
                            remoteReader->set_change_to_status(changeToSend.sequence_number, ACKNOWLEDGED);
                        }
                    }
                }
                else
                {
                    logError(RTPS_WRITER, "Error sending change " << changeToSend.sequence_number);
                }
            }

            // Heartbeat piggyback.
            if(!disable_heartbeat_piggyback_)
            {
                /*
                if(mp_history->isFull())
                {
                    send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group);
                }
                else
                {
                    currentUsageSendBufferSize_ -= group.get_current_bytes_processed() - lastBytesProcessed;
                    lastBytesProcessed = group.get_current_bytes_processed();

                    if(currentUsageSendBufferSize_ < 0)
                    {
                        send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group);
                    }
                }
                */
                //TODO(Ricardo) Revisar cambio
                currentUsageSendBufferSize_ -= group.get_current_bytes_processed() - lastBytesProcessed;
                lastBytesProcessed = group.get_current_bytes_processed();

                if(currentUsageSendBufferSize_ < 0)
                {
                    send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group,
                            history_.get_min_sequence_number_nts(), history_.get_max_sequence_number_nts());
                }
            }
        }

        for(auto pair : notRelevantChanges.elements())
        {
            std::vector<GUID_t> remote_readers;
            std::vector<LocatorList_t> locatorLists;

            for(auto remoteReader : pair.first)
            {
                remote_readers.push_back(remoteReader->m_att.guid);
                LocatorList_t locators(remoteReader->m_att.endpoint.unicastLocatorList);
                locators.push_back(remoteReader->m_att.endpoint.multicastLocatorList);
                locatorLists.push_back(locators);
            }
            group.add_gap(pair.second, remote_readers,
                    participant_.network_factory().ShrinkLocatorLists(locatorLists));
        }

        if(activateHeartbeatPeriod)
        {
            this->periodic_heartbeat_->restart_timer();
        }
    }
    else
    {
        RTPSMessageGroup group(participant_, this, RTPSMessageGroup::WRITER, m_cdrmessages);
        send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group, history_.get_min_sequence_number_nts(),
                history_.get_max_sequence_number_nts(), true);
    }

    logInfo(RTPS_WRITER, "Finish sending unsent changes");
}

bool StatefulWriter::matched_reader_add(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_add(rdata);
}

bool StatefulWriter::impl::matched_reader_add(const RemoteReaderAttributes& rdata)
{
    if(rdata.guid == c_Guid_Unknown)
    {
        logError(RTPS_WRITER,"Reliable Writer need GUID_t of matched readers");
        return false;
    }

    std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());

    std::vector<GUID_t> allRemoteReaders;
    std::vector<LocatorList_t> allLocatorLists;

    std::lock_guard<std::mutex> guard(mutex_);

    // Check if it is already matched.
    for(std::vector<ReaderProxy*>::iterator it = matched_readers_.begin(); it != matched_readers_.end(); ++it)
    {
        std::lock_guard<std::recursive_mutex> rguard(*(*it)->mp_mutex);
        if((*it)->m_att.guid == rdata.guid)
        {
            logInfo(RTPS_WRITER, "Attempting to add existing reader" << endl);
            return false;
        }

        allRemoteReaders.push_back((*it)->m_att.guid);
        LocatorList_t locators((*it)->m_att.endpoint.unicastLocatorList);
        locators.push_back((*it)->m_att.endpoint.multicastLocatorList);
        allLocatorLists.push_back(locators);
    }

    // Add info of new datareader.
    allRemoteReaders.push_back(rdata.guid);
    LocatorList_t locators(rdata.endpoint.unicastLocatorList);
    locators.push_back(rdata.endpoint.multicastLocatorList);
    allLocatorLists.push_back(locators);

    update_cached_info_nts(std::move(allRemoteReaders), allLocatorLists);

    ReaderProxy* rp = new ReaderProxy(*this, rdata, m_times);
    std::set<SequenceNumber_t> not_relevant_changes;

    SequenceNumber_t current_seq = history_.get_min_sequence_number_nts();
    SequenceNumber_t max_seq = history_.get_max_sequence_number_nts();

    if(current_seq != SequenceNumber_t::unknown())
    {
        (void)max_seq;
        assert(max_seq != SequenceNumber_t::unknown());

        if(rp->m_att.endpoint.durabilityKind >= TRANSIENT_LOCAL
                && this->getAttributes()->durabilityKind == TRANSIENT_LOCAL)
        {
            for(; current_seq <= max_seq; ++current_seq)
            {
                const CacheChange_t* const change =
                    history_.get_change_nts(current_seq);
                if(change != nullptr)
                {
                    ChangeForReader_t changeForReader(*change);
                    changeForReader.set_relevance(rp->rtps_is_relevant(*change));
                    if(!rp->rtps_is_relevant(*change))
                        not_relevant_changes.insert(changeForReader.get_sequence_number());
                    changeForReader.set_status(UNACKNOWLEDGED);
                    rp->add_change(changeForReader);
                }
                else
                {
                    ChangeForReader_t changeForReader(current_seq);
                    changeForReader.set_relevance(false);
                    not_relevant_changes.insert(current_seq);
                    changeForReader.set_status(UNACKNOWLEDGED);
                    rp->add_change(changeForReader);
                }
            }
        }
        else
        {
            for(; current_seq <= max_seq; ++current_seq)
            {
                ChangeForReader_t changeForReader(current_seq);
                changeForReader.set_relevance(false);
                not_relevant_changes.insert(current_seq);
                changeForReader.set_status(UNACKNOWLEDGED);
                rp->add_change(changeForReader);
            }
        }

        assert(max_seq + 1 == current_seq);
    }

    // Send a initial heartbeat
    if(rp->mp_initialHeartbeat != nullptr) // It is reliable
        rp->mp_initialHeartbeat->restart_timer();

    // TODO(Ricardo) In the heartbeat event?
    // Send Gap
    if(!not_relevant_changes.empty())
    {
        RTPSMessageGroup group(participant_, this, RTPSMessageGroup::WRITER, m_cdrmessages);
        //TODO (Ricardo) Temporal
        LocatorList_t locatorsList(rp->m_att.endpoint.unicastLocatorList);
        locatorsList.push_back(rp->m_att.endpoint.multicastLocatorList);

        group.add_gap(not_relevant_changes, {rp->m_att.guid}, locatorsList);
    }

    // Always activate heartbeat period. We need a confirmation of the reader.
    // The state has to be updated.
    this->periodic_heartbeat_->restart_timer();

    matched_readers_.push_back(rp);

    logInfo(RTPS_WRITER, "Reader Proxy "<< rp->m_att.guid<< " added to " << guid_.entityId << " with "
            <<rp->m_att.endpoint.unicastLocatorList.size()<<"(u)-"
            <<rp->m_att.endpoint.multicastLocatorList.size()<<"(m) locators");

    return true;
}

bool StatefulWriter::matched_reader_remove(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_remove(rdata);
}

bool StatefulWriter::impl::matched_reader_remove(const RemoteReaderAttributes& rdata)
{
    ReaderProxy *rproxy = nullptr;
    std::unique_lock<std::mutex> lock(mutex_);

    std::vector<GUID_t> allRemoteReaders;
    std::vector<LocatorList_t> allLocatorLists;

    auto it = matched_readers_.begin();
    while(it != matched_readers_.end())
    {
        std::lock_guard<std::recursive_mutex> rguard(*(*it)->mp_mutex);

        if((*it)->m_att.guid == rdata.guid)
        {
            logInfo(RTPS_WRITER, "Reader Proxy removed: " << (*it)->m_att.guid);
            assert(rproxy == nullptr);
            rproxy = std::move(*it);
            it = matched_readers_.erase(it);

            continue;
        }

        allRemoteReaders.push_back((*it)->m_att.guid);
        LocatorList_t locators((*it)->m_att.endpoint.unicastLocatorList);
        locators.push_back((*it)->m_att.endpoint.multicastLocatorList);
        allLocatorLists.push_back(locators);
        ++it;
    }

    update_cached_info_nts(std::move(allRemoteReaders), allLocatorLists);

    if(matched_readers_.size() == 0)
    {
        this->periodic_heartbeat_->cancel_timer();
    }

    lock.unlock();

    if(rproxy != nullptr)
    {
        delete rproxy;

        check_acked_status_nts_();

        return true;
    }

    logInfo(RTPS_HISTORY,"Reader Proxy doesn't exist in this writer");
    return false;
}

bool StatefulWriter::matched_reader_is_matched(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_is_matched(rdata);
}

bool StatefulWriter::impl::matched_reader_is_matched(const RemoteReaderAttributes& rdata)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for(auto remote_reader : matched_readers_)
    {
        std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);
        if(remote_reader->m_att.guid == rdata.guid)
        {
            return true;
        }
    }
    return false;
}

bool StatefulWriter::wait_for_all_acked(const Duration_t& max_wait)
{
    return impl_->wait_for_all_acked(max_wait);
}

bool StatefulWriter::impl::wait_for_all_acked(const Duration_t& max_wait)
{
    std::unique_lock<std::mutex> lock(mutex_);
    std::unique_lock<std::mutex> all_acked_lock(all_acked_mutex_);

    all_acked_ = true;

    for(auto remote_reader : matched_readers_)
    {
        std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);
        if(remote_reader->countChangesForReader() > 0)
        {
            all_acked_ = false;
            break;
        }
    }
    lock.unlock();

    if(!all_acked_)
    {
        std::chrono::microseconds max_w(::TimeConv::Time_t2MicroSecondsInt64(max_wait));
        all_acked_cond_.wait_for(all_acked_lock, max_w, [&]() { return all_acked_; });
    }

    return all_acked_;
}

void StatefulWriter::impl::check_acked_status_nts_()
{
    bool all_acked = true;

    for(auto remote_reader : matched_readers_)
    {
        std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);

        if(remote_reader->countChangesForReader() > 0)
        {
            all_acked = false;
            break;
        }
    }

    // If VOLATILE, remove samples acked.
    //TODO If min_low_mark automatically calculate, only call if change value.
    if(att_.durabilityKind == VOLATILE)
    {
        assert(cleanup_event_);
        cleanup_event_->restart_timer();
    }

    if(all_acked)
    {
        std::unique_lock<std::mutex> all_acked_lock(all_acked_mutex_);
        all_acked_ = true;
        all_acked_cond_.notify_all();
    }
}

void StatefulWriter::impl::cleanup_()
{
    assert(att_.durabilityKind == VOLATILE);

    //TODO Instead of calculate min_low_mark, always autocalculate when ack is received.
    SequenceNumber_t min_low_mark, history_min_sequence_number;

    std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());

    {
        std::unique_lock<std::mutex> lock(mutex_);

        for(auto remote_reader : matched_readers_)
        {
            std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);

            if(remote_reader->get_low_mark_nts() < min_low_mark)
            {
                min_low_mark = remote_reader->get_low_mark_nts();
            }
        }
    }

    // Unlock before call history_.remove_change_nts_, because will lock again the mutex.

    while((history_min_sequence_number = history_.get_min_sequence_number_nts()) <= min_low_mark)
    {
        history_.remove_change_nts(history_min_sequence_number);
    }
}

void StatefulWriter::updateAttributes(WriterAttributes& att)
{
    impl_->updateAttributes(att);
}

void StatefulWriter::impl::updateAttributes(WriterAttributes& att)
{
    this->updateTimes(att.times);
}

void StatefulWriter::impl::updateTimes(WriterTimes& times)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if(m_times.heartbeatPeriod != times.heartbeatPeriod)
    {
        this->periodic_heartbeat_->update_interval(times.heartbeatPeriod);
    }

    if(m_times.nackResponseDelay != times.nackResponseDelay)
    {
        for(auto remote_reader : matched_readers_)
        {
            std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);

            if(remote_reader->mp_nackResponse != nullptr) // It is reliable
            {
                remote_reader->mp_nackResponse->update_interval(times.nackResponseDelay);
            }
        }
    }
    if(m_times.heartbeatPeriod != times.heartbeatPeriod)
    {
        this->periodic_heartbeat_->update_interval(times.heartbeatPeriod);
    }
    if(m_times.nackResponseDelay != times.nackResponseDelay)
    {
        for(auto remote_reader : matched_readers_)
        {
            std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);
            remote_reader->mp_nackResponse->update_interval(times.nackResponseDelay);
        }
    }
    if(m_times.nackSupressionDuration != times.nackSupressionDuration)
    {
        for(auto remote_reader : matched_readers_)
        {
            std::lock_guard<std::recursive_mutex> rguard(*remote_reader->mp_mutex);

            if(remote_reader->mp_nackSupression != nullptr) // It is reliable
            {
                remote_reader->mp_nackSupression->update_interval(times.nackSupressionDuration);
            }
        }
    }
    m_times = times;
}

void StatefulWriter::impl::add_flow_controller(std::unique_ptr<FlowController> controller)
{
    m_controllers.push_back(std::move(controller));
}

void StatefulWriter::impl::send_heartbeat(bool final)
{
    SequenceNumber_t first_seq;
    SequenceNumber_t last_seq;
    {
        std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());
        first_seq = history_.get_min_sequence_number_nts();
        last_seq = history_.get_max_sequence_number_nts();
        if(first_seq == c_SequenceNumber_Unknown || last_seq == c_SequenceNumber_Unknown)
        {
            first_seq = history_.next_sequence_number_nts();
            last_seq = SequenceNumber_t(0, 0);
        }
        else
        {
            (void)first_seq;
            assert(first_seq <= last_seq);
        }
    }

    std::lock_guard<std::mutex> guard(mutex_);
    RTPSMessageGroup group(participant_, this, RTPSMessageGroup::WRITER, m_cdrmessages);
    send_heartbeat_nts_(mAllRemoteReaders, mAllShrinkedLocatorList, group, first_seq, last_seq, final);
}

void StatefulWriter::impl::send_heartbeat_to(const ReaderProxy& remote_reader, bool final)
{
    SequenceNumber_t first_seq;
    SequenceNumber_t last_seq;
    {
        std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());
        first_seq = history_.get_min_sequence_number_nts();
        last_seq = history_.get_max_sequence_number_nts();
        if(first_seq == c_SequenceNumber_Unknown || last_seq == c_SequenceNumber_Unknown)
        {
            first_seq = history_.next_sequence_number_nts();
            last_seq = SequenceNumber_t(0, 0);
        }
        else
        {
            (void)first_seq;
            assert(first_seq <= last_seq);
        }
    }

    std::lock_guard<std::mutex> guard(mutex_);
    send_heartbeat_to_nts(remote_reader, first_seq, last_seq, final);
}

void StatefulWriter::impl::send_heartbeat_to_nts(const ReaderProxy& remote_reader, SequenceNumber_t first_seq,
        SequenceNumber_t last_seq, bool final)
{
    RTPSMessageGroup group(participant_, this, RTPSMessageGroup::WRITER, m_cdrmessages);
    LocatorList_t locators(remote_reader.m_att.endpoint.unicastLocatorList);
    locators.push_back(remote_reader.m_att.endpoint.multicastLocatorList);

    send_heartbeat_nts_(std::vector<GUID_t>{remote_reader.m_att.guid}, locators, group, first_seq, last_seq, final);
}

void StatefulWriter::impl::send_heartbeat_nts_(const std::vector<GUID_t>& remote_readers, const LocatorList_t &locators,
        RTPSMessageGroup& message_group, SequenceNumber_t first_seq, SequenceNumber_t last_seq, bool final)
{
    incrementHBCount();

    // FinalFlag is always false because this class is used only by StatefulWriter in Reliable.
    message_group.add_heartbeat(remote_readers,
            first_seq, last_seq, m_heartbeatCount, final, false, locators);

    // Update calculate of heartbeat piggyback.
    currentUsageSendBufferSize_ = static_cast<int32_t>(sendBufferSize_);

    logInfo(RTPS_WRITER, getGuid().entityId << " Sending Heartbeat (" << first_seq << " - " << last_seq <<")" );
}

void StatefulWriter::impl::process_acknack(const GUID_t reader_guid, uint32_t ack_count,
                        const SequenceNumberSet_t& sn_set, bool final_flag)
{
    SequenceNumber_t first_seq;
    SequenceNumber_t last_seq;
    {
        std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());
        first_seq = history_.get_min_sequence_number_nts();
        last_seq = history_.get_max_sequence_number_nts();
    }

    std::unique_lock<std::mutex> lock(mutex_);

    for(auto remote_reader : matched_readers_)
    {
        if(remote_reader->m_att.guid == reader_guid)
        {
            std::lock_guard<std::recursive_mutex> reader_guard(*remote_reader->mp_mutex);

            if(remote_reader->m_lastAcknackCount < ack_count)
            {
                remote_reader->m_lastAcknackCount = ack_count;
                // Sequence numbers before Base are set as Acknowledged.
                remote_reader->acked_changes_set(sn_set.base);
                std::vector<SequenceNumber_t> set_vec = sn_set.get_set();
                if (remote_reader->requested_changes_set(set_vec) && remote_reader->mp_nackResponse != nullptr)
                {
                    remote_reader->mp_nackResponse->restart_timer();
                }
                else if(!final_flag)
                {
                    if(sn_set.base == SequenceNumber_t(0, 0) && sn_set.isSetEmpty())
                    {
                        send_heartbeat_to_nts(*remote_reader, first_seq, last_seq, true);
                    }

                    periodic_heartbeat_->restart_timer();
                }

                // Check if all CacheChange are acknowledge, because a user could be waiting
                // for this, of if VOLATILE should be removed CacheChanges
                check_acked_status_nts_();
            }
            break;
        }
    }
}

void StatefulWriter::impl::process_nackfrag(const GUID_t reader_guid, uint32_t nackfrag_count,
        const SequenceNumber_t& writer_sn, const FragmentNumberSet_t& fn_set)
{
    std::unique_lock<std::mutex> lock(mutex_);

    for(auto remote_reader : matched_readers_)
    {
        if(remote_reader->m_att.guid == reader_guid)
        {
            std::lock_guard<std::recursive_mutex> reader_guard(*remote_reader->mp_mutex);

            if(remote_reader->last_nackfrag_count() < nackfrag_count)
            {
                remote_reader->last_nackfrag_count() = nackfrag_count;
                // TODO Not doing Acknowledged.
                if(remote_reader->requested_fragment_set(writer_sn, fn_set))
                {
                    remote_reader->mp_nackResponse->restart_timer();
                }
            }
            break;
        }
    }
}
