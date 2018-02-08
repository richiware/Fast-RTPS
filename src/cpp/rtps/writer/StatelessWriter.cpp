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

/*
 * @file StatelessWriter.cpp
 *
 */

#include "StatelessWriterImpl.h"
#include "../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/resources/AsyncWriterThread.h>
#include "../participant/RTPSParticipantImpl.h"
#include "../flowcontrol/FlowController.h"
#include "RTPSWriterCollector.h"

#include <mutex>
#include <vector>
#include <set>

#include <fastrtps/log/Log.h>

namespace eprosima {
namespace fastrtps{
namespace rtps {

StatelessWriter::StatelessWriter(RTPSParticipant& participant,
        const WriterAttributes& att, WriterHistory& history, WriterListener* listener) :
    RTPSWriter(participant, att, history, listener)
{
}

StatelessWriter::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid,
        const WriterAttributes& att, WriterHistory::impl& history, RTPSWriter::impl::Listener* listener) :
    RTPSWriter::impl(participant, guid, att, history, listener)
{
    mAllRemoteReaders = get_builtin_guid();
}

StatelessWriter::impl::~impl()
{
    deinit_();
}

void StatelessWriter::impl::deinit()
{
    deinit_();
}

void StatelessWriter::impl::deinit_()
{
    RTPSWriter::impl::deinit_();
}

std::vector<GUID_t> StatelessWriter::impl::get_builtin_guid()
{
    if(this->guid_.entityId == ENTITYID_SPDP_BUILTIN_RTPSParticipant_WRITER)
    {
        return {{GuidPrefix_t(), c_EntityId_SPDPReader}};
    }
#if HAVE_SECURITY
    else if(this->guid_.entityId == ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_WRITER)
    {
        return {{GuidPrefix_t(), participant_stateless_message_reader_entity_id}};
    }
#endif

    return {};
}


bool StatelessWriter::impl::unsent_change_added_to_history(const CacheChange_t& change)
{
#if defined(__DEBUG)
    assert(history_.get_mutex_owner() == history_.get_thread_id());
#endif

    std::lock_guard<std::mutex> guard(mutex_);

    if(!isAsync())
    {
        this->setLivelinessAsserted(true);

        RTPSMessageGroup group(participant_, this, RTPSMessageGroup::WRITER, m_cdrmessages);

        if(!group.add_data(change, mAllRemoteReaders, mAllShrinkedLocatorList, false))
        {
            logError(RTPS_WRITER, "Error sending change " << change.sequence_number);
        }
    }
    else
    {
        for(auto& reader_locator : reader_locators)
            reader_locator.unsent_changes.push_back(ChangeForReader_t(change));
        AsyncWriterThread::wakeUp(*this);
    }

    return true;
}

bool StatelessWriter::impl::change_removed_by_history(const SequenceNumber_t& sequence_number,
        const InstanceHandle_t&)
{
#if defined(__DEBUG)
    assert(history_.get_mutex_owner() == history_.get_thread_id());
#endif

    std::lock_guard<std::mutex> guard(mutex_);

    for(auto& reader_locator : reader_locators)
    {
        reader_locator.unsent_changes.erase(std::remove_if(
                    reader_locator.unsent_changes.begin(),
                    reader_locator.unsent_changes.end(),
                    [sequence_number](ChangeForReader_t& change)
                    {
                        return change.get_sequence_number() == sequence_number;
                    }),
                    reader_locator.unsent_changes.end());
    }

    return true;
}

void StatelessWriter::impl::update_unsent_changes(ReaderLocator& reader_locator,
        const SequenceNumber_t& sequence_number, const FragmentNumber_t fragment_number)
{
    auto it = std::find_if(reader_locator.unsent_changes.begin(),
            reader_locator.unsent_changes.end(),
            [sequence_number](const ChangeForReader_t& unsent_change)
            {
                return sequence_number == unsent_change.get_sequence_number();
            });

    if(it != reader_locator.unsent_changes.end())
    {
        if(fragment_number != 0)
        {
            it->mark_fragment_as_sent(fragment_number);
            FragmentNumberSet_t fragment_sns = it->get_unsent_fragments();
            if(fragment_sns.isSetEmpty())
                reader_locator.unsent_changes.erase(it);
        }
        else
            reader_locator.unsent_changes.erase(it);
    }
}

void StatelessWriter::impl::send_any_unsent_changes()
{
    std::unique_lock<std::mutex>
        history_lock(history_.lock_for_transaction());

    std::lock_guard<std::mutex> guard(mutex_);

    RTPSWriterCollector<ReaderLocator*> changesToSend;

    for(auto& reader_locator : reader_locators)
    {
        for(auto unsent_change : reader_locator.unsent_changes)
        {
            const CacheChange_t* const cachechange =
                history_.get_change_nts(unsent_change.get_sequence_number());
            assert(cachechange != nullptr);
            changesToSend.add_change(cachechange, &reader_locator, unsent_change.get_unsent_fragments());
        }
    }

    // Clear through local controllers
    for (auto& controller : m_controllers)
        (*controller)(changesToSend);

    // Clear through parent controllers
    for (auto& controller : participant_.getFlowControllers())
        (*controller)(changesToSend);

    RTPSMessageGroup group(participant_, this,  RTPSMessageGroup::WRITER, m_cdrmessages);

    while(!changesToSend.empty())
    {
        RTPSWriterCollector<ReaderLocator*>::Item changeToSend = changesToSend.pop();
        std::vector<GUID_t> remote_readers = get_builtin_guid();
        std::set<GUID_t> remote_readers_aux;
        LocatorList_t locatorList;
        bool expectsInlineQos = false, addGuid = remote_readers.empty();

        for(auto readerLocator : changeToSend.remote_readers)
        {
            // Remove the messages selected for sending from the original list,
            // and update those that were fragmented with the new sent index
            update_unsent_changes(*readerLocator, changeToSend.sequence_number, changeToSend.fragment_number);

            if(addGuid)
                remote_readers_aux.insert(readerLocator->remote_guids.begin(), readerLocator->remote_guids.end());
            locatorList.push_back(readerLocator->locator);
            expectsInlineQos |= readerLocator->expectsInlineQos;
        }

        if(addGuid)
            remote_readers.assign(remote_readers_aux.begin(), remote_readers_aux.end());

        // Notify the controllers
        FlowController::NotifyControllersChangeSent(changeToSend.cachechange);

        if(changeToSend.fragment_number != 0)
        {
            if(!group.add_data_frag(*changeToSend.cachechange, changeToSend.fragment_number, remote_readers,
                        locatorList, expectsInlineQos))
            {
                logError(RTPS_WRITER, "Error sending fragment (" << changeToSend.sequence_number <<
                        ", " << changeToSend.fragment_number << ")");
            }
        }
        else
        {
            if(!group.add_data(*changeToSend.cachechange, remote_readers,
                        locatorList, expectsInlineQos))
            {
                logError(RTPS_WRITER, "Error sending change " << changeToSend.sequence_number);
            }
        }
    }

    logInfo(RTPS_WRITER, "Finish sending unsent changes";);
}


bool StatelessWriter::matched_reader_add(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_add(rdata);
}

bool StatelessWriter::impl::matched_reader_add(const RemoteReaderAttributes& rdata)
{
    std::unique_lock<std::mutex>
        history_lock(history_.lock_for_transaction());

    std::lock_guard<std::mutex> guard(mutex_);

    std::vector<GUID_t> allRemoteReaders = get_builtin_guid();
    std::vector<LocatorList_t> allLocatorLists;
    bool addGuid = allRemoteReaders.empty();

    for(auto it = m_matched_readers.begin(); it != m_matched_readers.end(); ++it)
    {
        if((*it).guid == rdata.guid)
        {
            logWarning(RTPS_WRITER, "Attempting to add existing reader");
            return false;
        }

        if(addGuid)
            allRemoteReaders.push_back((*it).guid);
        LocatorList_t locators((*it).endpoint.unicastLocatorList);
        locators.push_back((*it).endpoint.multicastLocatorList);
        allLocatorLists.push_back(locators);
    }

    // Add info of new datareader.
    if(addGuid)
        allRemoteReaders.push_back(rdata.guid);
    LocatorList_t locators(rdata.endpoint.unicastLocatorList);
    locators.push_back(rdata.endpoint.multicastLocatorList);
    allLocatorLists.push_back(locators);

    update_cached_info_nts(std::move(allRemoteReaders), allLocatorLists);

    this->m_matched_readers.push_back(rdata);

    update_locators_nts_(rdata.endpoint.durabilityKind >= TRANSIENT_LOCAL ? rdata.guid : c_Guid_Unknown);

    logInfo(RTPS_READER,"Reader " << rdata.guid << " added to " << guid_.entityId);
    return true;
}

bool StatelessWriter::impl::add_locator(Locator_t& loc)
{
#if HAVE_SECURITY
    if(!is_submessage_protected() && !is_payload_protected())
#endif
    {
        std::lock_guard<std::mutex> guard(mutex_);

        for(auto readerLocator : fixed_locators)
            if(readerLocator.locator == loc)
            {
                logInfo(RTPS_WRITER, "Already registered locator");
                return false;
            }

        ReaderLocator newLoc;
        newLoc.locator = loc;
        fixed_locators.push_back(newLoc);

        mAllShrinkedLocatorList.push_back(loc);

        for(auto readerLocator : reader_locators)
            if(readerLocator.locator == newLoc.locator)
                return true;

        reader_locators.push_back(newLoc);
        return true;
    }
#if HAVE_SECURITY
    else
    {
        logError(RTPS_WRITER, "A secure besteffort writer cannot add a lonely locator");
        return false;
    }
#endif
}

void StatelessWriter::impl::update_locators_nts_(const GUID_t& optionalGuid)
{
    std::vector<ReaderLocator> backup(std::move(reader_locators));

    // Update mAllShrinkedLocatorList because at this point it was only updated
    // with locators of reader_locators, and not the fixed locators.
    for(auto fixedLocator : fixed_locators)
            mAllShrinkedLocatorList.push_back(fixedLocator.locator);

    for(auto it = mAllShrinkedLocatorList.begin(); it != mAllShrinkedLocatorList.end(); ++it)
    {
        auto readerLocator = std::find_if(backup.begin(), backup.end(),
                [it](const ReaderLocator& reader_locator) {
                    if(reader_locator.locator == *it)
                        return true;

                    return false;
                });

        if(readerLocator != backup.end())
        {
            reader_locators.push_back(std::move(*readerLocator));
            //backup.erase(readerLocator);
        }
        else
        {
            logInfo(RTPS_WRITER, "Adding Locator: " << *it << " to StatelessWriter";);

            ReaderLocator rl;
            rl.locator = *it;
            reader_locators.push_back(std::move(rl));
        }

        reader_locators.back().remote_guids.clear();
        reader_locators.back().expectsInlineQos = false;

        // Find guids
        for(auto remoteReader = m_matched_readers.begin(); remoteReader != m_matched_readers.end(); ++remoteReader)
        {
            bool found = false;

            for(auto loc = remoteReader->endpoint.unicastLocatorList.begin(); loc != remoteReader->endpoint.unicastLocatorList.end(); ++loc)
            {
                if(*loc == reader_locators.back().locator ||
                        (participant_.network_factory().is_local_locator(*loc) &&
                         participant_.network_factory().is_local_locator(reader_locators.back().locator)))
                {
                    found = true;
                    break;
                }
            }

            for(auto loc = remoteReader->endpoint.multicastLocatorList.begin(); !found && loc != remoteReader->endpoint.multicastLocatorList.end(); ++loc)
                if(*loc == reader_locators.back().locator)
                {
                    found = true;
                    break;
                }

            if(found)
            {
                reader_locators.back().remote_guids.push_back(remoteReader->guid);
                reader_locators.back().expectsInlineQos |= remoteReader->expectsInlineQos;

                if(remoteReader->guid == optionalGuid)
                {
                    for(auto history_it = history_.begin_nts(); history_it != history_.end_nts();
                            ++history_it)
                    {
                        reader_locators.back().unsent_changes.push_back(*history_it->second);
                    }
                    AsyncWriterThread::wakeUp(*this);
                }
            }
        }
    }
}

bool StatelessWriter::matched_reader_remove(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_remove(rdata);
}

bool StatelessWriter::impl::matched_reader_remove(const RemoteReaderAttributes& rdata)
{
    std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());

    std::lock_guard<std::mutex> guard(mutex_);

    std::vector<GUID_t> allRemoteReaders = get_builtin_guid();
    std::vector<LocatorList_t> allLocatorLists;
    bool found = false, addGuid = allRemoteReaders.empty();

    auto rit = m_matched_readers.begin();
    while(rit!=m_matched_readers.end())
    {
        if((*rit).guid == rdata.guid)
        {
            rit = m_matched_readers.erase(rit);
            found = true;
            continue;
        }

        if(addGuid)
            allRemoteReaders.push_back((*rit).guid);
        LocatorList_t locators((*rit).endpoint.unicastLocatorList);
        locators.push_back((*rit).endpoint.multicastLocatorList);
        allLocatorLists.push_back(locators);
        ++rit;
    }

    update_cached_info_nts(std::move(allRemoteReaders), allLocatorLists);

    update_locators_nts_(c_Guid_Unknown);

    return found;
}

bool StatelessWriter::matched_reader_is_matched(const RemoteReaderAttributes& rdata)
{
    return impl_->matched_reader_is_matched(rdata);
}

bool StatelessWriter::impl::matched_reader_is_matched(const RemoteReaderAttributes& rdata)
{
    std::lock_guard<std::mutex> guard(mutex_);
    for(auto rit = m_matched_readers.begin();
            rit!=m_matched_readers.end();++rit)
    {
        if((*rit).guid == rdata.guid)
        {
            return true;
        }
    }
    return false;
}

void StatelessWriter::impl::resent_changes()
{
    std::unique_lock<std::mutex> history_lock(history_.lock_for_transaction());

    std::lock_guard<std::mutex> guard(mutex_);

    for(auto history_it = history_.begin_nts(); history_it != history_.end_nts(); ++history_it)
    {
        const CacheChange_t& change = *history_it->second;
        for(auto& reader_locator : reader_locators)
        {
            reader_locator.unsent_changes.push_back(change);
        }
    }

    AsyncWriterThread::wakeUp(*this);
}

void StatelessWriter::impl::add_flow_controller(std::unique_ptr<FlowController> controller)
{
    m_controllers.push_back(std::move(controller));
}


} /* namespace rtps */
} /* namespace eprosima */
}
