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
 * @file SubscriberImpl.cpp
 *
 */

#include "SubscriberImpl.h"
#include "../participant/ParticipantImpl.h"
#include <fastrtps/TopicDataType.h>
#include "../rtps/reader/RTPSReaderImpl.h"
#include "../rtps/reader/StatefulReaderImpl.h"
#include <fastrtps/log/Log.h>

using namespace eprosima::fastrtps;
using namespace ::rtps;

Subscriber::impl::impl(Participant::impl& participant, TopicDataType* type,
        const SubscriberAttributes& att, Subscriber::impl::Listener* listener):
    participant_(participant),
    reader_(nullptr),
    type_(type),
    att_(att),
#pragma warning (disable : 4355 )
    history_(*this, type->m_typeSize  + 3/*Possible alignment*/, att.topic.historyQos, att.topic.resourceLimitsQos,att.historyMemoryPolicy),
    listener_(listener),
    m_readerListener(*this)
{
}

bool Subscriber::impl::init()
{
    ReaderAttributes ratt;
    ratt.endpoint.durabilityKind = att_.qos.m_durability.kind == VOLATILE_DURABILITY_QOS ? VOLATILE : TRANSIENT_LOCAL;
    ratt.endpoint.endpointKind = READER;
    ratt.endpoint.multicastLocatorList = att_.multicastLocatorList;
    ratt.endpoint.reliabilityKind = att_.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS ? RELIABLE : BEST_EFFORT;
    ratt.endpoint.topicKind = att_.topic.topicKind;
    ratt.endpoint.unicastLocatorList = att_.unicastLocatorList;
    ratt.endpoint.outLocatorList = att_.outLocatorList;
    ratt.expectsInlineQos = att_.expectsInlineQos;
    ratt.endpoint.properties = att_.properties;
    if(att_.getEntityID() > 0)
    {
        ratt.endpoint.setEntityID((uint8_t)att_.getEntityID());
    }
    if(att_.getUserDefinedID() > 0)
    {
        ratt.endpoint.setUserDefinedID((uint8_t)att_.getUserDefinedID());
    }
    ratt.times = att_.times;

    reader_ = participant_.rtps_participant().create_reader(ratt, &history_,
            &m_readerListener);

    if(reader_)
    {
        return true;
    }
    else
    {
        logError(PUBLISHER,"Problem creating associated reader");
    }

    return false;
}

Subscriber::impl::~impl()
{
    deinit();
}

void Subscriber::impl::deinit()
{
    if(reader_ != nullptr)
    {
        logInfo(SUBSCRIBER,this->getGuid().entityId << " in topic: "<<this->att_.topic.topicName);
        participant_.rtps_participant().remove_reader(reader_);
    }
}

bool Subscriber::impl::enable()
{
    bool returned_value = false;

    //Register the reader
    if(participant_.rtps_participant().register_reader(*reader_,
                att_.topic, att_.qos))
    {
        returned_value = true;
    }
    else
    {
        logError(PUBLISHER,"Failed registering associated reader");
    }

    return returned_value;
}

void Subscriber::impl::waitForUnreadMessage()
{
    if(history_.getUnreadCount()==0)
    {
        do
        {
            history_.waitSemaphore();
        }
        while(history_.getUnreadCount() == 0);
    }
}



bool Subscriber::impl::readNextData(void* data,SampleInfo_t* info)
{
    return this->history_.readNextData(data,info);
}

bool Subscriber::impl::takeNextData(void* data,SampleInfo_t* info) {
    return this->history_.takeNextData(data,info);
}

const GUID_t& Subscriber::impl::getGuid(){
    return reader_->getGuid();
}

bool Subscriber::impl::updateAttributes(SubscriberAttributes& att)
{
    bool updated = true;
    bool missing = false;
    if(att.unicastLocatorList.size() != this->att_.unicastLocatorList.size() ||
            att.multicastLocatorList.size() != this->att_.multicastLocatorList.size())
    {
        logWarning(RTPS_READER,"Locator Lists cannot be changed or updated in this version");
        updated &= false;
    }
    else
    {
        for(LocatorListIterator lit1 = this->att_.unicastLocatorList.begin();
                lit1!=this->att_.unicastLocatorList.end();++lit1)
        {
            missing = true;
            for(LocatorListIterator lit2 = att.unicastLocatorList.begin();
                    lit2!= att.unicastLocatorList.end();++lit2)
            {
                if(*lit1 == *lit2)
                {
                    missing = false;
                    break;
                }
            }
            if(missing)
            {
                logWarning(RTPS_READER,"Locator: "<< *lit1 << " not present in new list");
                logWarning(RTPS_READER,"Locator Lists cannot be changed or updated in this version");
            }
        }
        for(LocatorListIterator lit1 = this->att_.multicastLocatorList.begin();
                lit1!=this->att_.multicastLocatorList.end();++lit1)
        {
            missing = true;
            for(LocatorListIterator lit2 = att.multicastLocatorList.begin();
                    lit2!= att.multicastLocatorList.end();++lit2)
            {
                if(*lit1 == *lit2)
                {
                    missing = false;
                    break;
                }
            }
            if(missing)
            {
                logWarning(RTPS_READER,"Locator: "<< *lit1<< " not present in new list");
                logWarning(RTPS_READER,"Locator Lists cannot be changed or updated in this version");
            }
        }
    }

    //TOPIC ATTRIBUTES
    if(this->att_.topic != att.topic)
    {
        logWarning(RTPS_READER,"Topic Attributes cannot be updated");
        updated &= false;
    }
    //QOS:
    //CHECK IF THE QOS CAN BE SET
    if(!this->att_.qos.canQosBeUpdated(att.qos))
    {
        updated &=false;
    }
    if(updated)
    {
        this->att_.expectsInlineQos = att.expectsInlineQos;
        if(this->att_.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS)
        {
            //UPDATE TIMES:
            StatefulReader::impl* sfr = dynamic_cast<StatefulReader::impl*>(reader_.get());
            sfr->updateTimes(att.times);
        }
        this->att_.qos.setQos(att.qos,false);
        //NOTIFY THE BUILTIN PROTOCOLS THAT THE READER HAS CHANGED
        participant_.rtps_participant().update_local_reader(*reader_, att_.qos);
    }
    return updated;
}

void Subscriber::impl::SubscriberReaderListener::onNewCacheChangeAdded(RTPSReader::impl& reader, const CacheChange_t* const /*change*/)
{
    (void)reader;
    assert(subscriber_.reader_.get() == &reader);

    if(subscriber_.listener_ != nullptr)
    {
        subscriber_.listener_->onNewDataMessage(subscriber_);
    }
}

void Subscriber::impl::SubscriberReaderListener::onReaderMatched(RTPSReader::impl& reader, const MatchingInfo& info)
{
    (void)reader;
    assert(subscriber_.reader_.get() == &reader);

    if (subscriber_.listener_ != nullptr)
    {
        subscriber_.listener_->onSubscriptionMatched(subscriber_, info);
    }
}

/*!
 * @brief Returns there is a clean state with all Publishers.
 * It occurs when the Subscriber received all samples sent by Publishers. In other words,
 * its WriterProxies are up to date.
 * @return There is a clean state with all Publishers.
 */
bool Subscriber::impl::isInCleanState() const
{
    return reader_->isInCleanState();
}
