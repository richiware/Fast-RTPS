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
 * @file ParticipantImpl.cpp
 *
 */

#include "ParticipantImpl.h"
#include <fastrtps/participant/ParticipantDiscoveryInfo.h>
#include <fastrtps/participant/ParticipantListener.h>

#include <fastrtps/TopicDataType.h>

#include <fastrtps/rtps/participant/RTPSParticipant.h>

#include <fastrtps/attributes/PublisherAttributes.h>
#include <fastrtps/publisher/Publisher.h>
#include "../publisher/PublisherImpl.h"

#include <fastrtps/attributes/SubscriberAttributes.h>
#include "../subscriber/SubscriberImpl.h"
#include <fastrtps/subscriber/Subscriber.h>

#include <fastrtps/rtps/RTPSDomain.h>

#include <fastrtps/transport/UDPv4Transport.h>
#include <fastrtps/transport/UDPv6Transport.h>
#include <fastrtps/transport/test_UDPv4Transport.h>

#include <fastrtps/log/Log.h>

using namespace eprosima::fastrtps::rtps;

namespace eprosima {
namespace fastrtps {


Participant::impl::impl(Participant& participant, const ParticipantAttributes& patt,
        ParticipantListener* listener) :
    rtps_listener_(participant),
    m_att(patt),
    mp_rtpsParticipant(nullptr),
    mp_listener(listener)
{
}

Participant::impl::~impl()
{
    while(m_publishers.size()>0)
    {
        this->removePublisher(*m_publishers.begin());
    }
    while(m_subscribers.size()>0)
    {
        this->removeSubscriber(m_subscribers.begin()->first);
    }

    if(this->mp_rtpsParticipant != nullptr)
        RTPSDomain::removeRTPSParticipant(this->mp_rtpsParticipant);
}


bool Participant::impl::removePublisher(Publisher* pub)
{
    for(auto pit = this->m_publishers.begin(); pit != m_publishers.end(); ++pit)
    {
        if((*pit)->getGuid() == pub->getGuid())
        {
            delete(*pit);
            m_publishers.erase(pit);
            return true;
        }
    }
    return false;
}

bool Participant::impl::removeSubscriber(Subscriber* sub)
{
    for(auto sit = m_subscribers.begin();sit!= m_subscribers.end();++sit)
    {
        if(sit->second->getGuid() == sub->getGuid())
        {
            delete(sit->second);
            m_subscribers.erase(sit);
            return true;
        }
    }
    return false;
}

const GUID_t& Participant::impl::getGuid() const
{
    return this->mp_rtpsParticipant->getGuid();
}

Publisher* Participant::impl::createPublisher(PublisherAttributes& att,
        PublisherListener* listen)
{
    logInfo(PARTICIPANT,"CREATING PUBLISHER IN TOPIC: "<<att.topic.getTopicName());
    //Look for the correct type registration

    TopicDataType* p_type = nullptr;

    /// Preconditions
    // Check the type was registered.
    if(!getRegisteredType(att.topic.getTopicDataType().c_str(),&p_type))
    {
        logError(PARTICIPANT,"Type : "<< att.topic.getTopicDataType() << " Not Registered");
        return nullptr;
    }
    // Check the type supports keys.
    if(att.topic.topicKind == WITH_KEY && !p_type->m_isGetKeyDefined)
    {
        logError(PARTICIPANT,"Keyed Topic needs getKey function");
        return nullptr;
    }

    if(m_att.rtps.builtin.use_STATIC_EndpointDiscoveryProtocol)
    {
        if(att.getUserDefinedID() <= 0)
        {
            logError(PARTICIPANT,"Static EDP requires user defined Id");
            return nullptr;
        }
    }
    if(!att.unicastLocatorList.isValid())
    {
        logError(PARTICIPANT,"Unicast Locator List for Publisher contains invalid Locator");
        return nullptr;
    }
    if(!att.multicastLocatorList.isValid())
    {
        logError(PARTICIPANT," Multicast Locator List for Publisher contains invalid Locator");
        return nullptr;
    }
    if(!att.outLocatorList.isValid())
    {
        logError(PARTICIPANT,"Output Locator List for Publisher contains invalid Locator");
        return nullptr;
    }
    if(!att.qos.checkQos() || !att.topic.checkQos())
        return nullptr;

    //TODO CONSTRUIR LA IMPLEMENTACION DENTRO DEL OBJETO DEL USUARIO.
    Publisher* publisher = new Publisher(*this, p_type, att, listen);

    WriterAttributes watt;
    watt.throughputController = att.throughputController;
    watt.endpoint.durabilityKind = att.qos.m_durability.kind == VOLATILE_DURABILITY_QOS ? VOLATILE : TRANSIENT_LOCAL;
    watt.endpoint.endpointKind = WRITER;
    watt.endpoint.multicastLocatorList = att.multicastLocatorList;
    watt.endpoint.reliabilityKind = att.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS ? RELIABLE : BEST_EFFORT;
    watt.endpoint.topicKind = att.topic.topicKind;
    watt.endpoint.unicastLocatorList = att.unicastLocatorList;
    watt.endpoint.outLocatorList = att.outLocatorList;
    watt.mode = att.qos.m_publishMode.kind == eprosima::fastrtps::SYNCHRONOUS_PUBLISH_MODE ? SYNCHRONOUS_WRITER : ASYNCHRONOUS_WRITER;
    watt.endpoint.properties = att.properties;
    if(att.getEntityID()>0)
        watt.endpoint.setEntityID((uint8_t)att.getEntityID());
    if(att.getUserDefinedID()>0)
        watt.endpoint.setUserDefinedID((uint8_t)att.getUserDefinedID());
    watt.times = att.times;

    RTPSWriter* writer = RTPSDomain::createRTPSWriter(this->mp_rtpsParticipant,
            watt, get_implementation(*publisher).history_,
            (WriterListener*)&get_implementation(*publisher).writer_listener_);

    if(writer == nullptr)
    {
        logError(PARTICIPANT,"Problem creating associated Writer");
        delete(publisher);
        return nullptr;
    }

    get_implementation(*publisher).writer_ = writer;

    m_publishers.push_back(publisher);

    //REGISTER THE WRITER
    this->mp_rtpsParticipant->registerWriter(writer,att.topic,att.qos);

    return publisher;
}


std::pair<StatefulReader*,StatefulReader*> Participant::impl::getEDPReaders(){

    return mp_rtpsParticipant->getEDPReaders();
}

std::vector<std::string> Participant::impl::getParticipantNames(){
    return mp_rtpsParticipant->getParticipantNames();
}

Subscriber* Participant::impl::createSubscriber(SubscriberAttributes& att,
        SubscriberListener* listen)
{
    logInfo(PARTICIPANT,"CREATING SUBSCRIBER IN TOPIC: "<<att.topic.getTopicName())
        //Look for the correct type registration

        TopicDataType* p_type = nullptr;

    if(!getRegisteredType(att.topic.getTopicDataType().c_str(),&p_type))
    {
        logError(PARTICIPANT,"Type : "<< att.topic.getTopicDataType() << " Not Registered");
        return nullptr;
    }
    if(att.topic.topicKind == WITH_KEY && !p_type->m_isGetKeyDefined)
    {
        logError(PARTICIPANT,"Keyed Topic needs getKey function");
        return nullptr;
    }
    if(m_att.rtps.builtin.use_STATIC_EndpointDiscoveryProtocol)
    {
        if(att.getUserDefinedID() <= 0)
        {
            logError(PARTICIPANT,"Static EDP requires user defined Id");
            return nullptr;
        }
    }
    if(!att.unicastLocatorList.isValid())
    {
        logError(PARTICIPANT,"Unicast Locator List for Subscriber contains invalid Locator");
        return nullptr;
    }
    if(!att.multicastLocatorList.isValid())
    {
        logError(PARTICIPANT," Multicast Locator List for Subscriber contains invalid Locator");
        return nullptr;
    }
    if(!att.outLocatorList.isValid())
    {
        logError(PARTICIPANT,"Output Locator List for Subscriber contains invalid Locator");
        return nullptr;
    }
    if(!att.qos.checkQos() || !att.topic.checkQos())
        return nullptr;

    SubscriberImpl* subimpl = new SubscriberImpl(*this, p_type, att, listen);
    Subscriber* sub = new Subscriber(subimpl);
    subimpl->mp_userSubscriber = sub;
    subimpl->mp_rtpsParticipant = this->mp_rtpsParticipant;

    ReaderAttributes ratt;
    ratt.endpoint.durabilityKind = att.qos.m_durability.kind == VOLATILE_DURABILITY_QOS ? VOLATILE : TRANSIENT_LOCAL;
    ratt.endpoint.endpointKind = READER;
    ratt.endpoint.multicastLocatorList = att.multicastLocatorList;
    ratt.endpoint.reliabilityKind = att.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS ? RELIABLE : BEST_EFFORT;
    ratt.endpoint.topicKind = att.topic.topicKind;
    ratt.endpoint.unicastLocatorList = att.unicastLocatorList;
    ratt.endpoint.outLocatorList = att.outLocatorList;
    ratt.expectsInlineQos = att.expectsInlineQos;
    ratt.endpoint.properties = att.properties;
    if(att.getEntityID()>0)
        ratt.endpoint.setEntityID((uint8_t)att.getEntityID());
    if(att.getUserDefinedID()>0)
        ratt.endpoint.setUserDefinedID((uint8_t)att.getUserDefinedID());
    ratt.times = att.times;

    RTPSReader* reader = RTPSDomain::createRTPSReader(this->mp_rtpsParticipant,
            ratt,
            (ReaderHistory*)&subimpl->m_history,
            (ReaderListener*)&subimpl->m_readerListener);
    if(reader == nullptr)
    {
        logError(PARTICIPANT,"Problem creating associated Reader");
        delete(subimpl);
        return nullptr;
    }
    subimpl->mp_reader = reader;
    //SAVE THE PUBLICHER PAIR
    t_p_SubscriberPair subpair;
    subpair.first = sub;
    subpair.second = subimpl;
    m_subscribers.push_back(subpair);

    //REGISTER THE READER
    this->mp_rtpsParticipant->registerReader(reader,att.topic,att.qos);

    return sub;
}


bool Participant::impl::getRegisteredType(const char* typeName, TopicDataType** type)
{
    for(std::vector<TopicDataType*>::iterator it=m_types.begin();
            it!=m_types.end();++it)
    {
        if(strcmp((*it)->getName(),typeName)==0)
        {
            *type = *it;
            return true;
        }
    }
    return false;
}

bool Participant::impl::registerType(TopicDataType* type)
{
    if (type->m_typeSize <= 0)
    {
        logError(PARTICIPANT, "Registered Type must have maximum byte size > 0");
        return false;
    }
    if (std::string(type->getName()).size() <= 0)
    {
        logError(PARTICIPANT, "Registered Type must have a name");
        return false;
    }
    for (auto ty = m_types.begin(); ty != m_types.end();++ty)
    {
        if (strcmp((*ty)->getName(), type->getName()) == 0)
        {
            logError(PARTICIPANT, "Type with the same name already exists:" << type->getName());
            return false;
        }
    }
    m_types.push_back(type);
    logInfo(PARTICIPANT, "Type " << type->getName() << " registered.");
    return true;
}

bool Participant::impl::unregisterType(const char* typeName)
{
    bool retValue = true;
    std::vector<TopicDataType*>::iterator typeit;

    for (typeit = m_types.begin(); typeit != m_types.end(); ++typeit)
    {
        if(strcmp((*typeit)->getName(), typeName) == 0)
        {
            break;
        }
    }

    if(typeit != m_types.end())
    {
        bool inUse = false;

        for(auto sit = m_subscribers.begin(); !inUse && sit != m_subscribers.end(); ++sit)
        {
            if(strcmp(sit->second->getType()->getName(), typeName) == 0)
            {
                inUse = true;
            }
        }

        for(auto pit = m_publishers.begin(); !inUse && pit != m_publishers.end(); ++pit)
        {
            if(strcmp(get_implementation(**pit).getType()->getName(), typeName) == 0)
            {
                inUse = true;
                break;
            }
        }

        if(!inUse)
        {
            m_types.erase(typeit);
        }
        else
        {
            retValue =  false;
        }
    }

    return retValue;
}



void Participant::impl::MyRTPSParticipantListener::onRTPSParticipantDiscovery(RTPSParticipant*,
        RTPSParticipantDiscoveryInfo rtpsinfo)
{
    if(get_implementation(participant_).mp_listener != nullptr)
    {
        ParticipantDiscoveryInfo info;
        info.rtps = rtpsinfo;
        get_implementation(participant_).mp_listener->onParticipantDiscovery(&participant_, info);
    }
}

#if HAVE_SECURITY
void Participant::impl::MyRTPSParticipantListener::onRTPSParticipantAuthentication(RTPSParticipant*
        , const RTPSParticipantAuthenticationInfo& rtps_info)
{
    if(get_implementation(participant_).mp_listener != nullptr)
    {
        ParticipantAuthenticationInfo info;
        info.rtps = rtps_info;
        get_implementation(participant_).mp_listener->onParticipantAuthentication(&participant_,
                info);
    }
}
#endif

bool Participant::impl::newRemoteEndpointDiscovered(const GUID_t& partguid, uint16_t endpointId,
        EndpointKind_t kind)
{
    if (kind == WRITER)
        return this->mp_rtpsParticipant->newRemoteWriterDiscovered(partguid, endpointId);
    else 
        return this->mp_rtpsParticipant->newRemoteReaderDiscovered(partguid, endpointId);
}

bool Participant::impl::get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo)
{
    return mp_rtpsParticipant->get_remote_writer_info(writerGuid, returnedInfo);
}

bool Participant::impl::get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo)
{
    return mp_rtpsParticipant->get_remote_reader_info(readerGuid, returnedInfo);
}

} /* namespace pubsub */
} /* namespace eprosima */
