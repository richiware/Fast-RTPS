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

#include "RTPSWriterImpl.h"
#include "../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/log/Log.h>
#include "../participant/RTPSParticipantImpl.h"
#include "../flowcontrol/FlowController.h"
#include <fastrtps/rtps/exceptions/Error.h>
#include <fastrtps/rtps/writer/WriterListener.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

    class RTPSWriter::ListenerLink : public RTPSWriter::impl::Listener
    {
        public:

            ListenerLink(RTPSWriter& writer, WriterListener* listener) :
                writer_(writer), listener_(listener)
            {
            }

            RTPSWriter::impl::Listener* rtps_listener()
            {
                if(listener_)
                {
                    return this;
                }

                return nullptr;
            }

            virtual void onWriterMatched(RTPSWriter::impl& writer, const MatchingInfo& info) override
            {
                assert(listener_);
                assert(writer_.impl_.get() == &writer);
                listener_->onWriterMatched(writer_, info);
            }

        private:

            RTPSWriter& writer_;

            WriterListener* listener_;
    };
}
}
}

using namespace eprosima::fastrtps::rtps;

RTPSWriter::RTPSWriter(RTPSParticipant& participant, const WriterAttributes& attributes,
        WriterHistory& history, WriterListener* listener) :
    listener_link_(new RTPSWriter::ListenerLink(*this, listener)),
    impl_(get_implementation(participant).create_writer(attributes, //TODO(Ricardo) Remove *this
                get_implementation(history), listener_link_->rtps_listener()))
{
    if(!impl_)
    {
        throw Error("Error creating writer");
    }

    //TODO(Ricardo) Add to RTPSParticipant.
}

RTPSWriter::~RTPSWriter()
{
    if(impl_ && !impl_.unique())
    {
        impl_->participant().remove_writer(impl_);
    }
}

RTPSWriter::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid, const WriterAttributes& att,
        WriterHistory::impl& history, RTPSWriter::impl::Listener* listen) :
    Endpoint(participant, guid, att.endpoint), push_mode_(true),
    m_cdrmessages(participant.getMaxMessageSize() > att.throughputController.bytesPerPeriod ?
            att.throughputController.bytesPerPeriod > participant.getRTPSParticipantAttributes().throughputController.bytesPerPeriod ?
            participant.getRTPSParticipantAttributes().throughputController.bytesPerPeriod :
            att.throughputController.bytesPerPeriod :
            participant.getMaxMessageSize() > participant.getRTPSParticipantAttributes().throughputController.bytesPerPeriod ?
            participant.getRTPSParticipantAttributes().throughputController.bytesPerPeriod :
            participant.getMaxMessageSize(), participant.guid().guidPrefix),
    m_livelinessAsserted(false),
    history_(history),
    listener_(listen),
    is_async_(att.mode == SYNCHRONOUS_WRITER ? false : true),
    cachechange_pool_(history_.attributes().initialReservedCaches, history_.attributes().payloadMaxSize,
            history_.attributes().maximumReservedCaches, history_.attributes().memoryPolicy)
{
    history_.call_after_adding_change(std::bind(&RTPSWriter::impl::unsent_change_added_to_history, this,
            std::placeholders::_1));
    history_.call_after_deleting_change(std::bind(&RTPSWriter::impl::change_removed_by_history, this,
            std::placeholders::_1, std::placeholders::_2));
}

RTPSWriter::impl::~impl()
{
    deinit_();
}

void RTPSWriter::impl::deinit_()
{
    history_.clear();
}

CacheChange_ptr RTPSWriter::new_change(const std::function<uint32_t()>& data_cdr_serialize_size,
        ChangeKind_t change_kind, const InstanceHandle_t& handle)
{
    return impl_->new_change(data_cdr_serialize_size, change_kind, handle);
}

CacheChange_ptr RTPSWriter::impl::new_change(const std::function<uint32_t()>& data_cdr_serialize_size,
        ChangeKind_t change_kind, const InstanceHandle_t& handle)
{
    logInfo(RTPS_WRITER,"Creating new change");
    CacheChange_t* cachechange = nullptr;

    if(!cachechange_pool_.reserve_cache(&cachechange, data_cdr_serialize_size))
    {
        logWarning(RTPS_WRITER,"Problem reserving Cache from the History");
        return CacheChange_ptr();
    }
    assert(cachechange != nullptr);

    cachechange->kind = change_kind;
    if(att_.topicKind == WITH_KEY && !handle.is_unknown())
    {
        logWarning(RTPS_WRITER,"Changes in KEYED Writers need a valid instanceHandle");
    }
    cachechange->instance_handle = handle;
    cachechange->writer_guid = guid_;
    cachechange->write_params = WriteParams();
    return CacheChange_ptr(&cachechange_pool_, cachechange);
}

uint32_t RTPSWriter::impl::getTypeMaxSerialized() const
{
    return cachechange_pool_.get_initial_payload_size();
}

CONSTEXPR uint32_t info_dst_message_length = 16;
CONSTEXPR uint32_t info_ts_message_length = 12;
CONSTEXPR uint32_t data_frag_submessage_header_length = 36;

uint32_t RTPSWriter::impl::getMaxDataSize()
{
    return calculateMaxDataSize(participant_.getMaxMessageSize());
}

uint32_t RTPSWriter::impl::calculateMaxDataSize(uint32_t length)
{
    uint32_t maxDataSize = participant_.calculateMaxDataSize(length);

    maxDataSize -= info_dst_message_length +
        info_ts_message_length +
        data_frag_submessage_header_length;

    //TODO(Ricardo) inlineqos in future.

#if HAVE_SECURITY
    if(is_submessage_protected())
    {
        maxDataSize -= participant_.security_manager().calculate_extra_size_for_rtps_submessage(guid_);
    }

    if(is_payload_protected())
    {
        maxDataSize -= participant_.security_manager().calculate_extra_size_for_encoded_payload(guid_);
    }
#endif

    return maxDataSize;
}

void RTPSWriter::impl::update_cached_info_nts(std::vector<GUID_t>&& allRemoteReaders,
            std::vector<LocatorList_t>& allLocatorLists)
{
    mAllRemoteReaders = std::move(allRemoteReaders);
    mAllShrinkedLocatorList.clear();
    mAllShrinkedLocatorList.push_back(participant_.network_factory().ShrinkLocatorLists(allLocatorLists));
}

EndpointAttributes* RTPSWriter::getAttributes()
{
    return &impl_->att_;
}
