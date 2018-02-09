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
 * @file StatelessReader.cpp
 *             	
 */

#include "StatelessReaderImpl.h"
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/reader/ReaderListener.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/rtps/common/CacheChange.h>
#include "../participant/RTPSParticipantImpl.h"
#include "FragmentedChangePitStop.h"


#include <mutex>
#include <thread>

#include <cassert>

#define IDSTRING "(ID:"<< std::this_thread::get_id() <<") "<<

using namespace eprosima::fastrtps::rtps;

StatelessReader::StatelessReader(RTPSParticipant& participant, const ReaderAttributes& att,
        ReaderHistory* hist, ReaderListener* listen) :
    RTPSReader(participant, att, hist, listen)
{
}

StatelessReader::StatelessReader(std::shared_ptr<RTPSReader::impl>& impl, ReaderListener* listener) :
    RTPSReader(impl, listener)
{
}

StatelessReader::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid,
        const ReaderAttributes& att, ReaderHistory* hist, RTPSReader::impl::Listener* listener) :
    RTPSReader::impl(participant, guid, att, hist, listener)
{
}

StatelessReader::impl::~impl()
{
    deinit_();
}

void StatelessReader::impl::deinit()
{
    deinit_();
}

void StatelessReader::impl::deinit_()
{
    m_matched_writers.clear();
    RTPSReader::impl::deinit_();
}

bool StatelessReader::matched_writer_add(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_add(wdata);
}

bool StatelessReader::impl::matched_writer_add(const RemoteWriterAttributes& wdata)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    for(auto it = m_matched_writers.begin();it!=m_matched_writers.end();++it)
    {
        if((*it).guid == wdata.guid)
            return false;
    }
    logInfo(RTPS_READER,"Writer " << wdata.guid << " added to " << guid_.entityId);
    m_matched_writers.push_back(wdata);
    m_acceptMessagesFromUnkownWriters = false;
    return true;
}

bool StatelessReader::matched_writer_remove(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_remove(wdata);
}

bool StatelessReader::impl::matched_writer_remove(const RemoteWriterAttributes& wdata)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    for(auto it = m_matched_writers.begin();it!=m_matched_writers.end();++it)
    {
        if((*it).guid == wdata.guid)
        {
            logInfo(RTPS_READER,"Writer " <<wdata.guid<< " removed from " << guid_.entityId);
            m_matched_writers.erase(it);
            m_historyRecord.erase(wdata.guid);
            return true;
        }
    }
    return false;
}

bool StatelessReader::matched_writer_is_matched(const RemoteWriterAttributes& wdata)
{
    return impl_->matched_writer_is_matched(wdata);
}

bool StatelessReader::impl::matched_writer_is_matched(const RemoteWriterAttributes& wdata)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    for(auto it = m_matched_writers.begin();it!=m_matched_writers.end();++it)
    {
        if((*it).guid == wdata.guid)
        {
            return true;
        }
    }
    return false;
}

bool StatelessReader::impl::change_received(CacheChange_t* change)
{
    // Only make visible the change if there is not other with bigger sequence number.
    // TODO Revisar si no hay que incluirlo.
    if(!thereIsUpperRecordOf(change->writer_guid, change->sequence_number))
    {
        if(mp_history->received_change(change, 0))
        {
            m_historyRecord[change->writer_guid] = change->sequence_number;

            if(getListener() != nullptr)
            {
                getListener()->onNewCacheChangeAdded(*this, change);
            }

            mp_history->postSemaphore();
            return true;
        }
    }

    return false;
}

bool StatelessReader::nextUntakenCache(CacheChange_t** change, WriterProxy** wpout)
{
    return impl_->nextUntakenCache(change, wpout);
}

bool StatelessReader::impl::nextUntakenCache(CacheChange_t** change, WriterProxy** /*wpout*/)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    return mp_history->get_min_change(change);
}

bool StatelessReader::nextUnreadCache(CacheChange_t** change,WriterProxy** wpout)
{
    return impl_->nextUnreadCache(change, wpout);
}

bool StatelessReader::impl::nextUnreadCache(CacheChange_t** change,WriterProxy** /*wpout*/)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    //m_reader_cache.sortCacheChangesBySeqNum();
    bool found = false;
    std::vector<CacheChange_t*>::iterator it;
    //TODO PROTEGER ACCESO A HISTORIA AQUI??? YO CREO QUE NO, YA ESTA EL READER PROTEGIDO
    for(it = mp_history->changesBegin();
            it!=mp_history->changesEnd();++it)
    {
        if(!(*it)->is_read)
        {
            found = true;
            break;
        }
    }
    if(found)
    {
        *change = *it;
        return true;
    }
    logInfo(RTPS_READER,"No Unread elements left");
    return false;
}


bool StatelessReader::impl::change_removed_by_history(CacheChange_t* /*ch*/, WriterProxy* /*prox*/)
{
    return true;
}

bool StatelessReader::impl::processDataMsg(CacheChange_t *change)
{
    assert(change);

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if(acceptMsgFrom(change->writer_guid))
    {
        logInfo(RTPS_MSG_IN,IDSTRING"Trying to add change " << change->sequence_number <<" TO reader: "<< getGuid().entityId);

        CacheChange_t* change_to_add;
        if(reserveCache(&change_to_add, change->serialized_payload.length)) //Reserve a new cache from the corresponding cache pool
        {
#if HAVE_SECURITY
            if(is_payload_protected())
            {
                change_to_add->copy_not_memcpy(*change);
                if(!participant().security_manager().decode_serialized_payload(change->serialized_payload,
                        change_to_add->serialized_payload, guid_, change->writer_guid))
                {
                    releaseCache(change_to_add);
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
                    releaseCache(change_to_add);
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

        if(!change_received(change_to_add))
        {
            logInfo(RTPS_MSG_IN,IDSTRING"MessageReceiver not add change " << change_to_add->sequence_number);
            releaseCache(change_to_add);

            if(getGuid().entityId == c_EntityId_SPDPReader)
            {
                participant().assertRemoteRTPSParticipantLiveliness(change->writer_guid.guidPrefix);
            }
        }
    }

    return true;
}

bool StatelessReader::impl::processDataFragMsg(CacheChange_t *incomingChange, uint32_t sampleSize, uint32_t fragmentStartingNum)
{
    assert(incomingChange);

    std::unique_lock<std::recursive_mutex> lock(*mp_mutex);

    if (acceptMsgFrom(incomingChange->writer_guid))
    {
        // Check if CacheChange was received.
        if(!thereIsUpperRecordOf(incomingChange->writer_guid, incomingChange->sequence_number))
        {
            logInfo(RTPS_MSG_IN, IDSTRING"Trying to add fragment " << incomingChange->sequence_number.to64long()
                    << " TO reader: " << getGuid().entityId);

            CacheChange_t* change_to_add = incomingChange;

#if HAVE_SECURITY
            if(is_payload_protected())
            {
                if(reserveCache(&change_to_add, incomingChange->serialized_payload.length)) //Reserve a new cache from the corresponding cache pool
                {
                    change_to_add->copy_not_memcpy(*incomingChange);
                    if(!participant().security_manager().decode_serialized_payload(incomingChange->serialized_payload,
                                change_to_add->serialized_payload, guid_, incomingChange->writer_guid))
                    {
                        releaseCache(change_to_add);
                        logWarning(RTPS_MSG_IN, "Cannont decode serialized payload");
                        return false;
                    }
                }
            }
#endif

            // Fragments manager has to process incomming fragments.
            // If CacheChange_t is completed, it will be returned;
            CacheChange_t* change_completed = fragmentedChangePitStop_->process(change_to_add, sampleSize, fragmentStartingNum);

#if HAVE_SECURITY
            if(is_payload_protected())
                releaseCache(change_to_add);
#endif

            // If the change was completed, process it.
            if(change_completed != nullptr)
            {
                // Try to remove previous CacheChange_t from PitStop.
                fragmentedChangePitStop_->try_to_remove_until(incomingChange->sequence_number, incomingChange->writer_guid);

                if (!change_received(change_completed))
                {
                    logInfo(RTPS_MSG_IN, IDSTRING"MessageReceiver not add change " << change_completed->sequence_number.to64long());

                    // Assert liveliness because if it is a participant discovery info.
                    if (getGuid().entityId == c_EntityId_SPDPReader)
                    {
                        participant().assertRemoteRTPSParticipantLiveliness(incomingChange->writer_guid.guidPrefix);
                    }

                    // Release CacheChange_t.
                    releaseCache(change_completed);
                }
            }
        }
    }

    return true;
}

bool StatelessReader::impl::processHeartbeatMsg(GUID_t& /*writerGUID*/, uint32_t /*hbCount*/, SequenceNumber_t& /*firstSN*/,
        SequenceNumber_t& /*lastSN*/, bool /*finalFlag*/, bool /*livelinessFlag*/)
{
    return true;
}

bool StatelessReader::impl::processGapMsg(GUID_t& /*writerGUID*/, SequenceNumber_t& /*gapStart*/, SequenceNumberSet_t& /*gapList*/)
{
    return true;
}

bool StatelessReader::impl::acceptMsgFrom(GUID_t& writerId)
{
    if(this->m_acceptMessagesFromUnkownWriters)
    {
        return true;
    }
    else
    {
        if(writerId.entityId == this->m_trustedWriterEntityId)
            return true;

        for(auto it = m_matched_writers.begin();it!=m_matched_writers.end();++it)
        {
            if((*it).guid == writerId)
            {
                return true;
            }
        }
    }

    return false;
}

bool StatelessReader::impl::thereIsUpperRecordOf(GUID_t& guid, SequenceNumber_t& seq)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    return m_historyRecord.find(guid) != m_historyRecord.end() && m_historyRecord[guid] >= seq;
}
