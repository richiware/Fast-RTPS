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
 * @file ParticipantImpl.h
 *
 */

#ifndef __PARTICIPANT_PARTICIPANTIMPL_H__
#define __PARTICIPANT_PARTICIPANTIMPL_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps/participant/Participant.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include "../publisher/PublisherImpl.h"
#include "../subscriber/SubscriberImpl.h"
#include <fastrtps/rtps/common/Guid.h>
#include "../rtps/participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>

namespace eprosima {
namespace fastrtps {

namespace rtps {
class WriterProxyData;
class ReaderProxyData;
class StatefulReader;
}

class TopicDataType;
class PublisherAttributes;
class SubscriberAttributes;


/**
 * This is the implementation class of the Participant.
 * @ingroup FASTRTPS_MODULE
 */
class Participant::impl
{
    typedef std::vector<std::shared_ptr<Publisher::impl>> v_publishers;
    typedef std::vector<std::shared_ptr<Subscriber::impl>> v_subscribers;

    class MyRTPSParticipantListener : public rtps::RTPSParticipant::impl::Listener
    {
        public:

            MyRTPSParticipantListener(Participant::impl& participant): participant_(participant) {};

            virtual ~MyRTPSParticipantListener(){};

            void onRTPSParticipantDiscovery(rtps::RTPSParticipant::impl& participant,
                    const rtps::RTPSParticipantDiscoveryInfo& info) override;

#if HAVE_SECURITY
            void onRTPSParticipantAuthentication(rtps::RTPSParticipant::impl& participant,
                    const rtps::RTPSParticipantAuthenticationInfo& info) override;
#endif

            Participant::impl& participant_;

    } rtps_listener_;


    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onParticipantDiscovery(Participant::impl&, const ParticipantDiscoveryInfo&) {}

#if HAVE_SECURITY
                virtual void onParticipantAuthentication(Participant::impl&, const ParticipantAuthenticationInfo&) {}
#endif
        };

        impl(const ParticipantAttributes& attr, Listener* listen = nullptr);

        virtual ~impl();

        /**
         * Register a type in this participant.
         * @param type Pointer to the TopicDatType.
         * @return True if registered.
         */
        bool register_type(TopicDataType* type);

        /**
         * Unregister a type in this participant.
         * @param typeName Name of the type
         * @return True if unregistered.
         */
        bool unregisterType(const char* typeName);

        /**
         * Create a Publisher in this Participant.
         * @param att Attributes of the Publisher.
         * @param listen Pointer to the listener.
         * @return Pointer to the created Publisher.
         */
        std::shared_ptr<Publisher::impl> create_publisher(const PublisherAttributes& att,
                Publisher::impl::Listener* listen = nullptr);

        /**
         * Create a Subscriber in this Participant.
         * @param att Attributes of the Subscriber
         * @param listen Pointer to the listener.
         * @return Pointer to the created Subscriber.
         */
        std::shared_ptr<Subscriber::impl> create_subscriber(const SubscriberAttributes& att,
                Subscriber::impl::Listener* listener = nullptr);

        /**
         * Remove a Publisher from this participant.
         * @param pub Pointer to the Publisher.
         * @return True if correctly removed.
         */
        bool remove_publisher(std::shared_ptr<Publisher::impl>& publisher);

        /**
         * Remove a Subscriber from this participant.
         * @param sub Pointer to the Subscriber.
         * @return True if correctly removed.
         */
        bool remove_subscriber(std::shared_ptr<Subscriber::impl>& subscriber);

        /**
         * Get the GUID_t of the associated RTPSParticipant.
         * @return GUID_t.
         */
        const rtps::GUID_t& getGuid() const;

        /**
         * Get the participant attributes
         * @return Participant attributes
         */
        inline const ParticipantAttributes& getAttributes() const {return m_att;};

        std::pair<rtps::StatefulReader*,rtps::StatefulReader*> getEDPReaders();

        std::vector<std::string> getParticipantNames() const;

        /**
         * This method can be used when using a StaticEndpointDiscovery mechanism differnet that the one
         * included in FastRTPS, for example when communicating with other implementations.
         * It indicates the Participant that an Endpoint from the XML has been discovered and
         * should be activated.
         * @param partguid Participant GUID_t.
         * @param userId User defined ID as shown in the XML file.
         * @param kind EndpointKind (WRITER or READER)
         * @return True if correctly found and activated.
         */
        bool newRemoteEndpointDiscovered(const rtps::GUID_t& partguid, uint16_t userId,
                rtps::EndpointKind_t kind);

        bool get_remote_writer_info(const rtps::GUID_t& writerGuid, rtps::WriterProxyData& returnedInfo);

        bool get_remote_reader_info(const rtps::GUID_t& readerGuid, rtps::ReaderProxyData& returnedInfo);

        rtps::RTPSParticipant::impl& rtps_participant()
        {
            return rtps_participant_;
        }

        bool getRegisteredType(const char* typeName, TopicDataType** type);

    private:

        //!Participant Attributes
        ParticipantAttributes m_att;

        //!RTPSParticipant
        rtps::RTPSParticipant::impl rtps_participant_;

        //!Participant Listener
        Listener* mp_listener;

        //!Publisher Vector
        v_publishers m_publishers;

        //!Subscriber Vector
        v_subscribers m_subscribers;

        //!TOpicDatType vector
        std::vector<TopicDataType*> m_types;
};

inline Participant::impl& get_implementation(Participant& participant)
{
    return *participant.impl_;
}

} //namespace fastrtps
} //namespace eprosima

#endif

#endif /* __PARTICIPANT_PARTICIPANTIMPL_H__ */
