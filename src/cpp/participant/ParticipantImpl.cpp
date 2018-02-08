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

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

Participant::impl::impl(const ParticipantAttributes& patt,
        Listener* listener) :
    rtps_listener_(*this),
    m_att(patt),
    rtps_participant_(patt.rtps, GuidPrefix_t::unknown(), &rtps_listener_),
    mp_listener(listener)
{
}

Participant::impl::~impl()
{
    while(!m_publishers.empty())
    {
        std::shared_ptr<Publisher::impl> publisher(m_publishers.front());
        remove_publisher(publisher);
    }

    while(!m_subscribers.empty())
    {
        std::shared_ptr<Subscriber::impl> subscriber(m_subscribers.front());
        remove_subscriber(subscriber);
    }
}

bool Participant::impl::remove_publisher(std::shared_ptr<Publisher::impl>& publisher)
{
    bool returned_value = false;

    for(auto pit = m_publishers.begin(); pit != m_publishers.end(); ++pit)
    {
        if(*pit == publisher)
        {
            m_publishers.erase(pit);
            publisher->deinit();
            returned_value =  true;
            break;
        }
    }

    return returned_value;
}

bool Participant::impl::remove_subscriber(std::shared_ptr<Subscriber::impl>& subscriber)
{
    bool returned_value = false;

    for(auto sit = m_subscribers.begin(); sit != m_subscribers.end(); ++sit)
    {
        if(*sit == subscriber)
        {
            m_subscribers.erase(sit);
            subscriber->deinit();
            returned_value =  true;
            break;
        }
    }

    return returned_value;
}

const GUID_t& Participant::impl::getGuid() const
{
    return rtps_participant_.guid();
}

std::shared_ptr<Publisher::impl> Participant::impl::create_publisher(const PublisherAttributes& att,
        Publisher::impl::Listener* listener)
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
    {
        return nullptr;
    }

    std::shared_ptr<Publisher::impl> publisher_impl = std::make_shared<Publisher::impl>(*this, p_type, att, listener);

    if(publisher_impl)
    {
        if(publisher_impl->init())
        {
            m_publishers.push_back(publisher_impl);
            return publisher_impl;
        }
    }

    return nullptr;
}


std::pair<StatefulReader*,StatefulReader*> Participant::impl::getEDPReaders()
{
    return rtps_participant_.getEDPReaders();
}

std::vector<std::string> Participant::impl::getParticipantNames() const
{
    return rtps_participant_.getParticipantNames();
}

std::shared_ptr<Subscriber::impl> Participant::impl::create_subscriber(const SubscriberAttributes& att,
        Subscriber::impl::Listener* listen)
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

    std::shared_ptr<Subscriber::impl> subscriber_impl = std::make_shared<Subscriber::impl>(*this, p_type, att, listen);

    if(subscriber_impl)
    {
        if(subscriber_impl->init())
        {
            m_subscribers.push_back(subscriber_impl);
            return subscriber_impl;
        }
    }

    return nullptr;
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

bool Participant::impl::register_type(TopicDataType* type)
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
            if(strcmp((*sit)->getType()->getName(), typeName) == 0)
            {
                inUse = true;
            }
        }

        for(auto pit = m_publishers.begin(); !inUse && pit != m_publishers.end(); ++pit)
        {
            if(strcmp((**pit).getType()->getName(), typeName) == 0)
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



void Participant::impl::MyRTPSParticipantListener::onRTPSParticipantDiscovery(RTPSParticipant::impl& participant,
        const RTPSParticipantDiscoveryInfo& rtpsinfo)
{
    (void)participant;
    assert(&participant_.rtps_participant_ == &participant);
    if(participant_.mp_listener != nullptr)
    {
        ParticipantDiscoveryInfo info;
        info.rtps = rtpsinfo;
        participant_.mp_listener->onParticipantDiscovery(participant_, info);
    }
}

#if HAVE_SECURITY
void Participant::impl::MyRTPSParticipantListener::onRTPSParticipantAuthentication(RTPSParticipant::impl& participant,
        const RTPSParticipantAuthenticationInfo& rtps_info)
{
    (void)participant;
    assert(&participant_.rtps_participant_ == &participant);
    if(participant_.mp_listener != nullptr)
    {
        ParticipantAuthenticationInfo info;
        info.rtps = rtps_info;
        participant_.mp_listener->onParticipantAuthentication(participant_, info);
    }
}
#endif

bool Participant::impl::newRemoteEndpointDiscovered(const GUID_t& partguid, uint16_t endpointId,
        EndpointKind_t kind)
{
    return rtps_participant_.newRemoteEndpointDiscovered(partguid, endpointId, kind);
}

bool Participant::impl::get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo)
{
    return rtps_participant_.get_remote_writer_info(writerGuid, returnedInfo);
}

bool Participant::impl::get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo)
{
    return rtps_participant_.get_remote_reader_info(readerGuid, returnedInfo);
}
