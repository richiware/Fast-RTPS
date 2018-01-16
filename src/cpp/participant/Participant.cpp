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
 * @file Participant.cpp
 *
 */

#include "ParticipantImpl.h"

namespace eprosima {
namespace fastrtps {

Participant::Participant(const ParticipantAttributes& attr, ParticipantListener* listener) :
    impl_(new Participant::impl(*this, attr, listener))
{
}

Participant::~Participant()
{
}

const GUID_t& Participant::getGuid() const
{
    return impl_->getGuid();
}

const ParticipantAttributes& Participant::getAttributes()
{
    return impl_->getAttributes();
}

bool Participant::newRemoteEndpointDiscovered(const GUID_t& partguid, uint16_t endpointId,
        EndpointKind_t kind)
{
    return impl_->newRemoteEndpointDiscovered(partguid, endpointId, kind);
}

std::pair<StatefulReader*,StatefulReader*> Participant::getEDPReaders(){
    std::pair<StatefulReader *,StatefulReader*> buffer;

    return impl_->getEDPReaders();
}

std::vector<std::string> Participant::getParticipantNames(){
    return impl_->getParticipantNames();
}

bool Participant::get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo)
{
    return impl_->get_remote_writer_info(writerGuid, returnedInfo);
}

bool Participant::get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo)
{
    return impl_->get_remote_reader_info(readerGuid, returnedInfo);
}

} /* namespace pubsub */
} /* namespace eprosima */
