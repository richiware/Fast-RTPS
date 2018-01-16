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
#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/rtps/reader/StatefulReader.h>

namespace eprosima{
namespace fastrtps{

namespace rtps{
class RTPSParticipant;
class WriterProxyData;
class ReaderProxyData;
}

class ParticipantListener;
class TopicDataType;
class Publisher;
class PublisherAttributes;
class PublisherListener;
class Subscriber;
class SubscriberImpl;
class SubscriberAttributes;
class SubscriberListener;


/**
 * This is the implementation class of the Participant.
 * @ingroup FASTRTPS_MODULE
 */
class Participant::impl
{
    typedef std::pair<Subscriber*,SubscriberImpl*> t_p_SubscriberPair;
    typedef std::vector<Publisher*> v_publishers;
    typedef std::vector<t_p_SubscriberPair> t_v_SubscriberPairs;

    class MyRTPSParticipantListener : public rtps::RTPSParticipantListener
    {
        public:

            MyRTPSParticipantListener(Participant& participant): participant_(participant) {};

            virtual ~MyRTPSParticipantListener(){};

            void onRTPSParticipantDiscovery(rtps::RTPSParticipant* part, rtps::RTPSParticipantDiscoveryInfo info);

#if HAVE_SECURITY
            void onRTPSParticipantAuthentication(rtps::RTPSParticipant* part, const rtps::RTPSParticipantAuthenticationInfo& info);
#endif

            Participant& participant_;

    } rtps_listener_;


    public:

    impl(Participant& participant, const ParticipantAttributes& attr,
            ParticipantListener* listen = nullptr);

    virtual ~impl();

    /**
     * Register a type in this participant.
     * @param type Pointer to the TopicDatType.
     * @return True if registered.
     */
    bool registerType(TopicDataType* type);

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
    Publisher* createPublisher(PublisherAttributes& att, PublisherListener* listen=nullptr);

    /**
     * Create a Subscriber in this Participant.
     * @param att Attributes of the Subscriber
     * @param listen Pointer to the listener.
     * @return Pointer to the created Subscriber.
     */
    Subscriber* createSubscriber(SubscriberAttributes& att, SubscriberListener* listen=nullptr);

    /**
     * Remove a Publisher from this participant.
     * @param pub Pointer to the Publisher.
     * @return True if correctly removed.
     */
    bool removePublisher(Publisher* pub);

    /**
     * Remove a Subscriber from this participant.
     * @param sub Pointer to the Subscriber.
     * @return True if correctly removed.
     */
    bool removeSubscriber(Subscriber* sub);

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

    MyRTPSParticipantListener* rtps_listener() { return &rtps_listener_; }

    void rtps_participant(rtps::RTPSParticipant* rtps_participant)
    {
        mp_rtpsParticipant = rtps_participant;
    }

    rtps::RTPSParticipant* rtps_participant()
    {
        return mp_rtpsParticipant;
    }

    bool getRegisteredType(const char* typeName, TopicDataType** type);

    private:

    //!Participant Attributes
    ParticipantAttributes m_att;

    //!RTPSParticipant
    rtps::RTPSParticipant* mp_rtpsParticipant;

    //!Participant Listener
    ParticipantListener* mp_listener;

    //!Publisher Vector
    v_publishers m_publishers;

    //!Subscriber Vector
    t_v_SubscriberPairs m_subscribers;

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
