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
 * @file RTPSWriter.cpp
 *
 */

#include <fastrtps/rtps/writer/RTPSWriter.h>
#include "../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/log/Log.h>
#include "../participant/RTPSParticipantImpl.h"
#include "../flowcontrol/FlowController.h"

using namespace eprosima::fastrtps::rtps;

RTPSWriter::RTPSWriter(RTPSParticipantImpl* impl, GUID_t& guid, WriterAttributes& att,
        WriterHistory& history, WriterListener* listen) :
    Endpoint(impl,guid,att.endpoint), push_mode_(true),
    m_cdrmessages(impl->getMaxMessageSize() > att.throughputController.bytesPerPeriod ?
            att.throughputController.bytesPerPeriod > impl->getRTPSParticipantAttributes().throughputController.bytesPerPeriod ?
            impl->getRTPSParticipantAttributes().throughputController.bytesPerPeriod :
            att.throughputController.bytesPerPeriod :
            impl->getMaxMessageSize() > impl->getRTPSParticipantAttributes().throughputController.bytesPerPeriod ?
            impl->getRTPSParticipantAttributes().throughputController.bytesPerPeriod :
            impl->getMaxMessageSize(), impl->getGuid().guidPrefix),
    m_livelinessAsserted(false),
    history_(get_implementation(history)),
    listener_(listen),
    is_async_(att.mode == SYNCHRONOUS_WRITER ? false : true),
    cachechange_pool_(history_.attributes().initialReservedCaches, history_.attributes().payloadMaxSize,
            history_.attributes().maximumReservedCaches, history_.attributes().memoryPolicy)
{
    history_.call_after_adding_change(std::bind(&RTPSWriter::unsent_change_added_to_history, this,
            std::placeholders::_1));
    history_.call_after_deleting_change(std::bind(&RTPSWriter::change_removed_by_history, this,
            std::placeholders::_1, std::placeholders::_2));
    logInfo(RTPS_WRITER,"RTPSWriter created");
}

RTPSWriter::~RTPSWriter()
{
    logInfo(RTPS_WRITER,"RTPSWriter destructor");
    history_.clear();
}

SequenceNumber_t RTPSWriter::next_sequence_number_nts() const
{
    return last_cachechange_seqnum_ + 1;
}

CacheChange_ptr RTPSWriter::new_change(const std::function<uint32_t()>& data_cdr_serialize_size,
        ChangeKind_t change_kind, InstanceHandle_t handle)
{
    logInfo(RTPS_WRITER,"Creating new change");
    CacheChange_t* cachechange = nullptr;

    std::lock_guard<std::mutex> guard(mutex_);

    if(!cachechange_pool_.reserve_cache(&cachechange, data_cdr_serialize_size))
    {
        logWarning(RTPS_WRITER,"Problem reserving Cache from the History");
        return CacheChange_ptr();
    }
    assert(cachechange != nullptr);

    cachechange->sequence_number = ++last_cachechange_seqnum_;
    cachechange->kind = change_kind;
    if(m_att.topicKind == WITH_KEY && !handle.is_unknown())
    {
        logWarning(RTPS_WRITER,"Changes in KEYED Writers need a valid instanceHandle");
    }
    cachechange->instance_handle = handle;
    cachechange->writer_guid = m_guid;
    cachechange->write_params = WriteParams();
    return CacheChange_ptr(&cachechange_pool_, cachechange);
}

void RTPSWriter::reuse_change(CacheChange_ptr& change)
{
    std::lock_guard<std::mutex> guard(mutex_);
    // TODO System to verify is my change.
    change->sequence_number = ++last_cachechange_seqnum_;
}

uint32_t RTPSWriter::getTypeMaxSerialized() const
{
    return cachechange_pool_.get_initial_payload_size();
}

CONSTEXPR uint32_t info_dst_message_length = 16;
CONSTEXPR uint32_t info_ts_message_length = 12;
CONSTEXPR uint32_t data_frag_submessage_header_length = 36;

uint32_t RTPSWriter::getMaxDataSize()
{
    return calculateMaxDataSize(mp_RTPSParticipant->getMaxMessageSize());
}

uint32_t RTPSWriter::calculateMaxDataSize(uint32_t length)
{
    uint32_t maxDataSize = mp_RTPSParticipant->calculateMaxDataSize(length);

    maxDataSize -= info_dst_message_length +
        info_ts_message_length +
        data_frag_submessage_header_length;

    //TODO(Ricardo) inlineqos in future.

#if HAVE_SECURITY
    if(is_submessage_protected())
    {
        maxDataSize -= mp_RTPSParticipant->security_manager().calculate_extra_size_for_rtps_submessage(m_guid);
    }

    if(is_payload_protected())
    {
        maxDataSize -= mp_RTPSParticipant->security_manager().calculate_extra_size_for_encoded_payload(m_guid);
    }
#endif

    return maxDataSize;
}

void RTPSWriter::update_cached_info_nts(std::vector<GUID_t>&& allRemoteReaders,
            std::vector<LocatorList_t>& allLocatorLists)
{
    mAllRemoteReaders = std::move(allRemoteReaders);
    mAllShrinkedLocatorList.clear();
    mAllShrinkedLocatorList.push_back(mp_RTPSParticipant->network_factory().ShrinkLocatorLists(allLocatorLists));
}
