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
 * @file Participant.h
 *
 */

#ifndef __PARTICIPANT_PARTICIPANT_H__
#define __PARTICIPANT_PARTICIPANT_H__

#include "../rtps/common/Guid.h"
#include "../rtps/attributes/RTPSParticipantAttributes.h"
#include "../rtps/reader/StatefulReader.h"
#include "ParticipantListener.h"

#include <memory>
#include <utility>

namespace eprosima {
namespace fastrtps{

class ParticipantAttributes;
class TopicDataType;

namespace rtps
{
    class WriterProxyData;
    class ReaderProxyData;
}

/**
 * Class Participant used to group Publishers and Subscribers into a single working unit.
 * @ingroup FASTRTPS_MODULE
 */
class RTPS_DllAPI Participant
{
    class ListenerLink;

    public:

        class impl;

        Participant(const ParticipantAttributes& attr, ParticipantListener* listener = nullptr);

        virtual ~Participant();

        /**
         *	Get the rtps::GUID_t of the associated RTPSParticipant.
         * @return rtps::GUID_t
         */
        const rtps::GUID_t& getGuid() const;

        bool register_type(TopicDataType* type);

        /**
         * Get the ParticipantAttributes.
         * @return ParticipantAttributes.
         */
        const ParticipantAttributes& getAttributes() const;

        /**
         * Called when using a StaticEndpointDiscovery mechanism different that the one
         * included in FastRTPS, for example when communicating with other implementations.
         * It indicates to the Participant that an Endpoint from the XML has been discovered and
         * should be activated.
         * @param partguid Participant rtps::GUID_t.
         * @param userId User defined ID as shown in the XML file.
         * @param kind EndpointKind (WRITER or READER)
         * @return True if correctly found and activated.
         */
        bool newRemoteEndpointDiscovered(const rtps::GUID_t& partguid, uint16_t userId,
                rtps::EndpointKind_t kind);

        /**
         * This method returns a pointer to the Endpoint Discovery Protocol Readers (when not in Static mode)
         * SimpleEDP creates two readers, one for Publishers and one for Subscribers, and they are both returned
         * as a std::pair of pointers. These readers in particular have modified listeners that allow a slave 
         * listener to attach its callbach to the original one, allowing for the addition of logging elements.
         * 
         * @return std::pair of pointers to the EDP Readers
         * */	
        std::pair<rtps::StatefulReader*, rtps::StatefulReader*> getEDPReaders();

        std::vector<std::string> getParticipantNames() const;

        bool get_remote_writer_info(const rtps::GUID_t& writerGuid, rtps::WriterProxyData& returnedInfo);

        bool get_remote_reader_info(const rtps::GUID_t& readerGuid, rtps::ReaderProxyData& returnedInfo);

    private:

        std::unique_ptr<ListenerLink> listener_link_;

        std::unique_ptr<impl> impl_;

        friend impl& get_implementation(Participant& participant);
};

}
} // namespace eprosima

#endif // __PARTICIPANT_PARTICIPANT_H__
