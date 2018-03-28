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
 * @file RTPSParticipantImpl.h
 */

#ifndef __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__
#define __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__

#include <fastrtps/rtps/participant/RTPSParticipant.h>
#include <fastrtps/rtps/attributes/RTPSParticipantAttributes.h>
#include <fastrtps/rtps/attributes/ReaderAttributes.h>
#include <rtps/writer/RTPSWriterImpl.h>
#include <rtps/reader/RTPSReaderImpl.h>
#include <rtps/history/WriterHistoryImpl.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>

#include <memory>
#include <gmock/gmock.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

class Endpoint;
class RTPSParticipant;
class ReaderHistory;
class WriterListener;
struct EntityId_t;

class MockParticipantListener : public RTPSParticipantListener
{
    public:

        MOCK_METHOD2(onRTPSParticipantDiscovery, void (RTPSParticipant::impl&, const RTPSParticipantDiscoveryInfo&));

        MOCK_METHOD2(onRTPSParticipantAuthentication, void (RTPSParticipant::impl&, const RTPSParticipantAuthenticationInfo&));
};

class RTPSParticipant::impl
{
    public:

        impl() : events_(*this)
        {
            events_.init_thread();
        }

        MOCK_CONST_METHOD0(getRTPSParticipantAttributes, const RTPSParticipantAttributes&());

        const GUID_t& guid() { return guid_; }

        void guid(GUID_t& guid) { guid_ = guid; }

        MOCK_METHOD5(create_writer_mock, std::shared_ptr<RTPSWriter::impl> (const WriterAttributes& param,
                    WriterHistory::impl& history, RTPSWriter::impl::Listener* listener, const EntityId_t& entityId,
                    bool isBuiltin));

        MOCK_METHOD6(create_reader_mock, std::shared_ptr<RTPSReader::impl> (const ReaderAttributes& param,
                    ReaderHistory* hist, RTPSReader::impl::Listener* listener, const EntityId_t& entityId, bool isBuiltin, bool enable));

        std::shared_ptr<RTPSWriter::impl> create_writer(const WriterAttributes& param, WriterHistory::impl& history,
                RTPSWriter::impl::Listener* listener, const EntityId_t& entityId, bool isBuiltin)
        {
            std::shared_ptr<RTPSWriter::impl> ret = create_writer_mock(param , history, listener, entityId, isBuiltin);
            if(ret)
            {
                ret->guid().guidPrefix = guid_.guidPrefix;
                ret->guid().entityId = entityId;
                ret->history_ = &history;
            }
            return ret;
        }

        std::shared_ptr<RTPSReader::impl> create_reader(const ReaderAttributes& param, ReaderHistory* history,
                RTPSReader::impl::Listener* listener, const EntityId_t& entityId, bool isBuiltin, bool enable)
        {
            std::shared_ptr<RTPSReader::impl> ret = create_reader_mock(param, history, listener, entityId, isBuiltin, enable);
            if(ret)
            {
                ret->guid().guidPrefix = guid_.guidPrefix;
                ret->guid().entityId = entityId;
                ret->history_ = history;
                ret->listener_ = listener;
            }
            return ret;
        }

        bool remove_writer(std::shared_ptr<RTPSWriter::impl>&) { return true; }

        bool remove_reader(std::shared_ptr<RTPSReader::impl>&) { return true; }

        PDPSimple* pdpsimple() { return &pdpsimple_; }

        MockParticipantListener* listener() { return &listener_; }

        RTPSParticipant* getUserRTPSParticipant() { return nullptr; }

        ResourceEvent& getEventResource() { return events_; }

        void set_endpoint_rtps_protection_supports(Endpoint* /*endpoint*/, bool /*support*/) {}

        void ResourceSemaphoreWait() {}
        void ResourceSemaphorePost() {}

    private:

        GUID_t guid_;

        PDPSimple pdpsimple_;

        MockParticipantListener listener_;

        ResourceEvent events_;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__
