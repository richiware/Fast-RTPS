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
 * @file RTPSParticipant.cpp
 *
 */

#include "RTPSParticipantImpl.h"
#include <fastrtps/rtps/Endpoint.h>
#include "../reader/RTPSReaderImpl.h"
#include "../writer/RTPSWriterImpl.h"
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class RTPSParticipant::ListenerLink : public RTPSParticipant::impl::Listener
{
    public:

        ListenerLink(RTPSParticipant& participant, RTPSParticipantListener* listener) :
            participant_(participant), listener_(listener)
        {
        }

        RTPSParticipant::impl::Listener* rtps_listener()
        {
            if(listener_)
            {
                return this;
            }

            return nullptr;
        }

        void onRTPSParticipantDiscovery(RTPSParticipant::impl& participant,
                const RTPSParticipantDiscoveryInfo& info) override
        {
            assert(listener_);
            assert(participant_.impl_.get() == &participant);
            listener_->onRTPSParticipantDiscovery(participant_, info);
        }

#if HAVE_SECURITY
        void onRTPSParticipantAuthentication(RTPSParticipant::impl& participant,
                const RTPSParticipantAuthenticationInfo& info) override
        {
            assert(listener_);
            assert(participant_.impl_.get() == &participant);
            listener_->onRTPSParticipantAuthentication(participant_, info);
        }
#endif

    private:

        RTPSParticipant& participant_;

        RTPSParticipantListener* listener_;
};

}
}
}

using namespace eprosima::fastrtps::rtps;

RTPSParticipant::RTPSParticipant(const RTPSParticipantAttributes& param, const GuidPrefix_t& guid,
                RTPSParticipantListener* listener) : listener_link_(new ListenerLink(*this, listener)),
    impl_(new impl(param, guid, listener_link_->rtps_listener())) //C++14 std::make_unique
{
}

RTPSParticipant::~RTPSParticipant()
{
}

const GUID_t& RTPSParticipant::guid() const
{
    return impl_->guid();
}

void RTPSParticipant::announceRTPSParticipantState()
{
    return impl_->announceRTPSParticipantState();
}

void RTPSParticipant::stopRTPSParticipantAnnouncement()
{
    return impl_->stopRTPSParticipantAnnouncement();
}

void RTPSParticipant::resetRTPSParticipantAnnouncement()
{
    return impl_->resetRTPSParticipantAnnouncement();
}

bool RTPSParticipant::newRemoteWriterDiscovered(const GUID_t& pguid, int16_t userDefinedId)
{
    return impl_->newRemoteEndpointDiscovered(pguid,userDefinedId, WRITER);
}
bool RTPSParticipant::newRemoteReaderDiscovered(const GUID_t& pguid, int16_t userDefinedId)
{
    return impl_->newRemoteEndpointDiscovered(pguid,userDefinedId, READER);
}
uint32_t RTPSParticipant::getRTPSParticipantID() const
{
    return impl_->getRTPSParticipantID();
}

bool RTPSParticipant::register_writer(RTPSWriter& writer, TopicAttributes& topicAtt, WriterQos& wqos)
{
    return impl_->register_writer(get_implementation(writer), topicAtt, wqos);
}

bool RTPSParticipant::register_reader(RTPSReader& reader,TopicAttributes& topicAtt, ReaderQos& rqos)
{
    return impl_->register_reader(get_implementation(reader), topicAtt, rqos);
}

bool RTPSParticipant::update_writer(RTPSWriter& writer, WriterQos& wqos)
{
    return impl_->update_local_writer(get_implementation(writer), wqos);
}

bool RTPSParticipant::update_reader(RTPSReader& reader, ReaderQos& rqos)
{
    return impl_->update_local_reader(get_implementation(reader), rqos);
}

std::pair<StatefulReader*,StatefulReader*> RTPSParticipant::getEDPReaders(){	
    return impl_->getEDPReaders();
}

std::vector<std::string> RTPSParticipant::getParticipantNames() const {
    return impl_->getParticipantNames();
}

RTPSParticipantAttributes RTPSParticipant::getRTPSParticipantAttributes() const {
    return impl_->getRTPSParticipantAttributes();
}

bool RTPSParticipant::get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo)
{
    return impl_->get_remote_writer_info(writerGuid, returnedInfo);
}

bool RTPSParticipant::get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo)
{
    return impl_->get_remote_reader_info(readerGuid, returnedInfo);
}
