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
 * @file StatefulReader.cpp
 *
 */

#include "StatefulReaderImpl.h"
#include <fastrtps/rtps/reader/WriterProxy.h>
#include <fastrtps/rtps/reader/ReaderListener.h>
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/reader/timedevent/HeartbeatResponseDelay.h>
#include <fastrtps/rtps/reader/timedevent/InitialAckNack.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include "../participant/RTPSParticipantImpl.h"
#include "FragmentedChangePitStop.h"
#include <fastrtps/utils/TimeConversion.h>

#include <mutex>
#include <thread>
#include <cassert>

#define IDSTRING "(ID:"<< std::this_thread::get_id() <<") "<<

using namespace eprosima::fastrtps::rtps;


StatefulReader::StatefulReader(RTPSParticipant& participant, const ReaderAttributes& att,
        ReaderHistory* hist, ReaderListener* listen):
    RTPSReader(participant, att, hist, listen)
{
}

StatefulReader::StatefulReader(std::shared_ptr<RTPSReader::impl>& impl, ReaderListener* listener) :
    RTPSReader(impl, listener)
{
}

StatefulReader::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid,
        const ReaderAttributes& att, ReaderHistory* hist, RTPSReader::impl::Listener* listener) :
    RTPSReader::impl(participant, guid, att, hist, listener),
    m_acknackCount(0),
    m_nackfragCount(0),
    m_times(att.times)
{
}

StatefulReader::impl::~impl()
{
    deinit_();
}

void StatefulReader::impl::deinit()
{
    deinit_();
}

void StatefulReader::impl::deinit_()
{
    for(std::vector<WriterProxy*>::iterator it = matched_writers.begin();
            it!=matched_writers.end(); ++it)
    {
        delete(*it);
    }
    matched_writers.clear();
    RTPSReader::impl::deinit_();
}

bool StatefulReader::matched_writer_add(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_add(wdata);
}

bool StatefulReader::impl::matched_writer_add(const RemoteWriterAttributes& wdata)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    for(std::vector<WriterProxy*>::iterator it=matched_writers.begin();
            it!=matched_writers.end();++it)
    {
        if((*it)->m_att.guid == wdata.guid)
        {
            logInfo(RTPS_READER,"Attempting to add existing writer");
            return false;
        }
    }
    WriterProxy* wp = new WriterProxy(*this, wdata);

    wp->mp_initialAcknack->restart_timer();

    matched_writers.push_back(wp);
    logInfo(RTPS_READER,"Writer Proxy " <<wp->m_att.guid <<" added to " << guid_.entityId);
    return true;
}

bool StatefulReader::matched_writer_remove(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_remove(wdata);
}

bool StatefulReader::impl::matched_writer_remove(const RemoteWriterAttributes& wdata)
{
    WriterProxy *wproxy = nullptr;
    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    //Remove cachechanges belonging to the unmatched writer
    mp_history->remove_changes_with_guid(wdata.guid);

    for(std::vector<WriterProxy*>::iterator it=matched_writers.begin();it!=matched_writers.end();++it)
    {
        if((*it)->m_att.guid == wdata.guid)
        {
            logInfo(RTPS_READER,"Writer Proxy removed: " <<(*it)->m_att.guid);
            wproxy = *it;
            matched_writers.erase(it);
            break;
        }
    }

    lock.unlock();

    if(wproxy != nullptr)
    {
        delete wproxy;
        return true;
    }

    logInfo(RTPS_READER,"Writer Proxy " << wdata.guid << " doesn't exist in reader "<<this->getGuid().entityId);
    return false;
}

bool StatefulReader::matched_writer_remove(const RemoteWriterAttributes& wdata, bool deleteWP)
{
    return static_cast<StatefulReader::impl*>(impl_.get())->matched_writer_remove(wdata, deleteWP);
}

bool StatefulReader::impl::matched_writer_remove(const RemoteWriterAttributes& wdata, bool deleteWP)
{
    WriterProxy *wproxy = nullptr;
    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    //Remove cachechanges belonging to the unmatched writer
    mp_history->remove_changes_with_guid(wdata.guid);

    for(std::vector<WriterProxy*>::iterator it=matched_writers.begin();it!=matched_writers.end();++it)
    {
        if((*it)->m_att.guid == wdata.guid)
        {
            logInfo(RTPS_READER,"Writer Proxy removed: " <<(*it)->m_att.guid);
            wproxy = *it;
            matched_writers.erase(it);
            break;
        }
    }

    lock.unlock();

    if(wproxy != nullptr && deleteWP)
    {
        delete(wproxy);
        return true;
    }

    logInfo(RTPS_READER,"Writer Proxy " << wdata.guid << " doesn't exist in reader "<<this->getGuid().entityId);
    return false;
}

bool StatefulReader::matched_writer_is_matched(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_is_matched(wdata);
}

bool StatefulReader::impl::matched_writer_is_matched(const RemoteWriterAttributes& wdata)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    for(std::vector<WriterProxy*>::iterator it=matched_writers.begin();it!=matched_writers.end();++it)
    {
        if((*it)->m_att.guid == wdata.guid)
        {
            return true;
        }
    }
    return false;
}

bool StatefulReader::matched_writer_lookup(const GUID_t& writerGUID, WriterProxy** WP)
{
    return static_cast<StatefulReader::impl*>(impl_.get())->matched_writer_lookup(writerGUID, WP);
}

bool StatefulReader::impl::matched_writer_lookup(const GUID_t& writerGUID, WriterProxy** WP)
{
    assert(WP);

    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    bool returnedValue = findWriterProxy(writerGUID, WP);

    if(returnedValue)
    {
        logInfo(RTPS_READER,this->getGuid().entityId<<" FINDS writerProxy "<< writerGUID<<" from "<< matched_writers.size());
    }
    else
    {
        logInfo(RTPS_READER,this->getGuid().entityId<<" NOT FINDS writerProxy "<< writerGUID<<" from "<< matched_writers.size());
    }

    return returnedValue;
}

bool StatefulReader::impl::findWriterProxy(const GUID_t& writerGUID, WriterProxy** WP)
{
    assert(WP);

    for(std::vector<WriterProxy*>::iterator it = matched_writers.begin(); it != matched_writers.end(); ++it)
    {
        if((*it)->m_att.guid == writerGUID)
        {
            *WP = *it;
            return true;
        }
    }
    return false;
}

bool StatefulReader::impl::processDataMsg(CacheChange_t *change)
{
    WriterProxy *pWP = nullptr;

    assert(change);

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if(acceptMsgFrom(change->writer_guid, &pWP))
    {
        // Check if CacheChange was received.
        if(!pWP->change_was_received(change->sequence_number))
        {
            logInfo(RTPS_MSG_IN,IDSTRING"Trying to add change " << change->sequence_number <<" TO reader: "<< getGuid().entityId);

            CacheChange_ptr change_to_add = reserveCache();

            if(change_to_add) //Reserve a new cache from the corresponding cache pool
            {
#if HAVE_SECURITY
                if(is_payload_protected())
                {
                    change_to_add->copy_not_memcpy(*change);
                    if(!participant().security_manager().decode_serialized_payload(change->serialized_payload,
                                change_to_add->serialized_payload, guid_, change->writer_guid))
                    {
                        logWarning(RTPS_MSG_IN, "Cannont decode serialized payload");
                        return false;
                    }
                }
                else
                {
#endif
                    if (!change_to_add->copy(*change))
                    {
                        logWarning(RTPS_MSG_IN,IDSTRING"Problem copying CacheChange, received data is: " << change->serialized_payload.length
                                << " bytes and max size in reader " << getGuid().entityId << " is " << change_to_add->serialized_payload.max_size);
                        return false;
                    }
#if HAVE_SECURITY
                }
#endif
            }
            else
            {
                logError(RTPS_MSG_IN,IDSTRING"Problem reserving CacheChange in reader: " << getGuid().entityId);
                return false;
            }

            // Assertion has to be done before call change_received,
            // because this function can unlock the StatefulReader mutex.
            if(pWP != nullptr)
            {
                pWP->assertLiveliness(); //Asser liveliness since you have received a DATA MESSAGE.
            }

            if(!change_received(change_to_add, pWP))
            {
                logInfo(RTPS_MSG_IN,IDSTRING"MessageReceiver not add change "<<change_to_add->sequence_number);

                if(pWP == nullptr && getGuid().entityId == c_EntityId_SPDPReader)
                {
                    participant().assertRemoteRTPSParticipantLiveliness(change->writer_guid.guidPrefix);
                }
            }
        }
    }

    return true;
}

bool StatefulReader::impl::processDataFragMsg(CacheChange_t *incomingChange, uint32_t sampleSize, uint32_t fragmentStartingNum)
{
    WriterProxy *pWP = nullptr;

    assert(incomingChange);

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if(acceptMsgFrom(incomingChange->writer_guid, &pWP))
    {
        // Check if CacheChange was received.
        if(!pWP->change_was_received(incomingChange->sequence_number))
        {
            logInfo(RTPS_MSG_IN, IDSTRING"Trying to add fragment " << incomingChange->sequence_number.to64long() << " TO reader: " << getGuid().entityId);

            CacheChange_ptr change_to_add(nullptr, incomingChange);

#if HAVE_SECURITY
            if(is_payload_protected())
            {
                change_to_add = reserveCache();

                if(change_to_add) //Reserve a new cache from the corresponding cache pool
                {
                    change_to_add->copy_not_memcpy(*incomingChange);
                    if(!participant().security_manager().decode_serialized_payload(incomingChange->serialized_payload,
                                change_to_add->serialized_payload, guid_, incomingChange->writer_guid))
                    {
                        logWarning(RTPS_MSG_IN, "Cannont decode serialized payload");
                        return false;
                    }
                }
            }
#endif

            // Fragments manager has to process incomming fragments.
            // If CacheChange_t is completed, it will be returned;
            CacheChange_ptr change_completed = fragmentedChangePitStop_->process(change_to_add, sampleSize,
                    fragmentStartingNum);

            // Assertion has to be done before call change_received,
            // because this function can unlock the StatefulReader mutex.
            if(pWP != nullptr)
            {
                pWP->assertLiveliness(); //Asser liveliness since you have received a DATA MESSAGE.
            }

            if(change_completed)
            {
                if(!change_received(change_completed, pWP))
                {
                    logInfo(RTPS_MSG_IN, IDSTRING"MessageReceiver not add change " << change_completed->sequence_number.to64long());

                    // Assert liveliness because it is a participant discovery info.
                    if(pWP == nullptr && getGuid().entityId == c_EntityId_SPDPReader)
                    {
                        participant().assertRemoteRTPSParticipantLiveliness(incomingChange->writer_guid.guidPrefix);
                    }
                }
            }
        }
    }

    return true;
}

bool StatefulReader::impl::processHeartbeatMsg(GUID_t &writerGUID, uint32_t hbCount, SequenceNumber_t &firstSN,
            SequenceNumber_t &lastSN, bool finalFlag, bool livelinessFlag)
{
    WriterProxy *pWP = nullptr;

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if(acceptMsgFrom(writerGUID, &pWP))
    {
        std::unique_lock<std::recursive_mutex> wpLock(*pWP->getMutex());

        if(pWP->m_lastHeartbeatCount < hbCount)
        {
            pWP->m_lastHeartbeatCount = hbCount;
            pWP->lost_changes_update(firstSN);
            fragmentedChangePitStop_->try_to_remove_until(firstSN, pWP->m_att.guid);
            pWP->missing_changes_update(lastSN);
            pWP->m_heartbeatFinalFlag = finalFlag;

            //Analyze wheter a acknack message is needed:
            if(!finalFlag)
            {
                pWP->mp_heartbeatResponse->restart_timer();
            }
            else if(finalFlag && !livelinessFlag)
            {
                if(pWP->areThereMissing())
                    pWP->mp_heartbeatResponse->restart_timer();
            }

            //FIXME: livelinessFlag
            if(livelinessFlag )//TODOG && WP->m_att->m_qos.m_liveliness.kind == MANUAL_BY_TOPIC_LIVELINESS_QOS)
            {
                pWP->assertLiveliness();
            }

            GUID_t proxGUID = pWP->m_att.guid;
            wpLock.unlock();

            // Maybe now we have to notify user from new CacheChanges.
            SequenceNumber_t nextChangeToNotify = pWP->nextCacheChangeToBeNotified();
            while(nextChangeToNotify != SequenceNumber_t::unknown())
            {
                if(getListener()!=nullptr)
                {
                    mp_history->postSemaphore();

                    CacheChange_t* ch_to_give = nullptr;
                    if(mp_history->get_change(nextChangeToNotify, proxGUID, &ch_to_give))
                    {
                        if(!ch_to_give->is_read)
                        {
                            lock.unlock();
                            getListener()->onNewCacheChangeAdded(*this, ch_to_give);
                            lock.lock();
                        }
                    }

                    // Search again the WriterProxy because could be removed after the unlock.
                    if(!findWriterProxy(proxGUID, &pWP))
                        break;
                }

                nextChangeToNotify = pWP->nextCacheChangeToBeNotified();
            }
        }
    }

    return true;
}

bool StatefulReader::impl::processGapMsg(GUID_t &writerGUID, SequenceNumber_t &gapStart, SequenceNumberSet_t &gapList)
{
    WriterProxy *pWP = nullptr;

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if(acceptMsgFrom(writerGUID, &pWP))
    {
        std::lock_guard<std::recursive_mutex> guardWriterProxy(*pWP->getMutex());
        SequenceNumber_t auxSN;
        SequenceNumber_t finalSN = gapList.base -1;
        for(auxSN = gapStart; auxSN<=finalSN;auxSN++)
        {
            if(pWP->irrelevant_change_set(auxSN))
                fragmentedChangePitStop_->try_to_remove(auxSN, pWP->m_att.guid);
        }

        for(auto it = gapList.get_begin(); it != gapList.get_end();++it)
        {
            if(pWP->irrelevant_change_set((*it)))
                fragmentedChangePitStop_->try_to_remove((*it), pWP->m_att.guid);
        }
    }

    return true;
}

bool StatefulReader::impl::acceptMsgFrom(GUID_t &writerId, WriterProxy **wp)
{
    assert(wp != nullptr);

    for(std::vector<WriterProxy*>::iterator it = this->matched_writers.begin();
            it!=matched_writers.end();++it)
    {
        if((*it)->m_att.guid == writerId)
        {
            *wp = *it;
            return true;
        }
    }

    return false;
}

bool StatefulReader::impl::change_removed_by_history(CacheChange_t* a_change, WriterProxy* wp)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    if(wp != nullptr || matched_writer_lookup(a_change->writer_guid,&wp))
    {
        wp->setNotValid(a_change->sequence_number);
        return true;
    }
    else
    {
        logError(RTPS_READER," You should always find the WP associated with a change, something is very wrong");
    }
    return false;
}

bool StatefulReader::impl::change_received(CacheChange_ptr& a_change, WriterProxy* prox)
{
    //First look for WriterProxy in case is not provided
    if(prox == nullptr)
    {
        if(!findWriterProxy(a_change->writer_guid, &prox))
        {
            logInfo(RTPS_READER, "Writer Proxy " << a_change->writer_guid <<" not matched to this Reader "<< guid_.entityId);
            return false;
        }
    }

    SequenceNumber_t sequence_number = a_change->sequence_number;

    std::unique_lock<std::recursive_mutex> writerProxyLock(*prox->getMutex());

    size_t unknown_missing_changes_up_to = prox->unknown_missing_changes_up_to(sequence_number);

    if(this->mp_history->received_change(a_change, unknown_missing_changes_up_to))
    {
        bool ret = prox->received_change_set(sequence_number);

        GUID_t proxGUID = prox->m_att.guid;

        // If KEEP_LAST and history full, make older changes as lost.
        CacheChange_t* aux_change = nullptr;
        if(this->mp_history->isFull() && mp_history->get_min_change_from(&aux_change, proxGUID))
        {
            prox->lost_changes_update(aux_change->sequence_number);
            fragmentedChangePitStop_->try_to_remove_until(aux_change->sequence_number, proxGUID);
        }

        writerProxyLock.unlock();

        SequenceNumber_t nextChangeToNotify = prox->nextCacheChangeToBeNotified();

        while(nextChangeToNotify != SequenceNumber_t::unknown())
        {
            mp_history->postSemaphore();

            if(getListener()!=nullptr)
            {
                CacheChange_t* ch_to_give = nullptr;

                if(mp_history->get_change(nextChangeToNotify, proxGUID, &ch_to_give))
                {
                    if(!ch_to_give->is_read)
                    {
                        getListener()->onNewCacheChangeAdded(*this, ch_to_give);
                    }
                }

                // Search again the WriterProxy because could be removed after the unlock.
                if(!findWriterProxy(proxGUID, &prox))
                    break;
            }

            nextChangeToNotify = prox->nextCacheChangeToBeNotified();
        }

        return ret;
    }

    return false;
}

bool StatefulReader::nextUntakenCache(CacheChange_t** change,WriterProxy** wpout)
{
    return impl_->nextUntakenCache(change, wpout);
}

bool StatefulReader::impl::nextUntakenCache(CacheChange_t** change,WriterProxy** wpout)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    std::vector<CacheChange_t*> toremove;
    bool takeok = false;
    for(auto it = mp_history->changesBegin(); it != mp_history->changesEnd(); ++it)
    {
        WriterProxy* wp;
        if(this->matched_writer_lookup((*it)->writer_guid, &wp))
        {
            // TODO Revisar la comprobacion
            SequenceNumber_t seq = wp->available_changes_max();
            if(seq >= (*it)->sequence_number)
            {
                *change = &**it;
                if(wpout !=nullptr)
                    *wpout = wp;

                takeok = true;
                break;
                //				if((*it)->kind == ALIVE)
                //				{
                //					this->mp_type->deserialize(&(*it)->serializedPayload,data);
                //				}
                //				(*it)->isRead = true;
                //				if(info!=NULL)
                //				{
                //					info->sampleKind = (*it)->kind;
                //					info->writerGUID = (*it)->writerGUID;
                //					info->sourceTimestamp = (*it)->sourceTimestamp;
                //					info->iHandle = (*it)->instanceHandle;
                //					if(this->m_qos.m_ownership.kind == EXCLUSIVE_OWNERSHIP_QOS)
                //						info->ownershipStrength = wp->m_data->m_qos.m_ownershipStrength.value;
                //				}
                //				m_reader_cache.decreaseUnreadCount();
                //				logInfo(RTPS_READER,this->getGuid().entityId<<": reading change "<< (*it)->sequenceNumber.to64long());
                //				readok = true;
                //				break;
            }
        }
        else
        {
            toremove.push_back(&(**it));
        }
    }

    for(std::vector<CacheChange_t*>::iterator it = toremove.begin();
            it!=toremove.end();++it)
    {
        logWarning(RTPS_READER,"Removing change " << (*it)->sequence_number << " from "
                << (*it)->writer_guid << " because is no longer paired");
        mp_history->remove_change(*it);
    }
    return takeok;
}

bool StatefulReader::nextUnreadCache(CacheChange_t** change,WriterProxy** wpout)
{
    return impl_->nextUnreadCache(change, wpout);
}

// TODO Porque elimina aqui y no cuando hay unpairing
bool StatefulReader::impl::nextUnreadCache(CacheChange_t** change,WriterProxy** wpout)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    std::vector<CacheChange_t*> toremove;
    bool readok = false;
    for(auto it = mp_history->changesBegin(); it != mp_history->changesEnd(); ++it)
    {
        if((*it)->is_read)
            continue;
        WriterProxy* wp;
        if(this->matched_writer_lookup((*it)->writer_guid,&wp))
        {
            SequenceNumber_t seq;
            seq = wp->available_changes_max();
            if(seq >= (*it)->sequence_number)
            {
                *change = &**it;
                if(wpout !=nullptr)
                    *wpout = wp;

                readok = true;
                break;
                //				if((*it)->kind == ALIVE)
                //				{
                //					this->mp_type->deserialize(&(*it)->serializedPayload,data);
                //				}
                //				(*it)->isRead = true;
                //				if(info!=NULL)
                //				{
                //					info->sampleKind = (*it)->kind;
                //					info->writerGUID = (*it)->writerGUID;
                //					info->sourceTimestamp = (*it)->sourceTimestamp;
                //					info->iHandle = (*it)->instanceHandle;
                //					if(this->m_qos.m_ownership.kind == EXCLUSIVE_OWNERSHIP_QOS)
                //						info->ownershipStrength = wp->m_data->m_qos.m_ownershipStrength.value;
                //				}
                //				m_reader_cache.decreaseUnreadCount();
                //				logInfo(RTPS_READER,this->getGuid().entityId<<": reading change "<< (*it)->sequenceNumber.to64long());
                //				readok = true;
                //				break;
            }
        }
        else
        {
            toremove.push_back(&(**it));
        }
    }

    for(std::vector<CacheChange_t*>::iterator it = toremove.begin();
            it!=toremove.end();++it)
    {
        logWarning(RTPS_READER,"Removing change "<<(*it)->sequence_number << " from " << (*it)->writer_guid << " because is no longer paired");
        mp_history->remove_change(*it);
    }

    return readok;
}

bool StatefulReader::impl::updateTimes(ReaderTimes& ti)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    if(m_times.heartbeatResponseDelay != ti.heartbeatResponseDelay)
    {
        m_times = ti;
        for(std::vector<WriterProxy*>::iterator wit = this->matched_writers.begin();
                wit!=this->matched_writers.end();++wit)
        {
            (*wit)->mp_heartbeatResponse->update_interval(m_times.heartbeatResponseDelay);
        }
    }
    return true;
}

bool StatefulReader::isInCleanState() const
{
    return impl_->isInCleanState();
}

bool StatefulReader::impl::isInCleanState() const
{
    bool cleanState = true;
    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    for (WriterProxy* wp : matched_writers)
    {
        if (wp->numberOfChangeFromWriter() != 0)
        {
            cleanState = false;
            break;
        }
    }

    return cleanState;
}
