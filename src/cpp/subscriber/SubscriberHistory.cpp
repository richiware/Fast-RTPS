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
 * @file SubscriberHistory.cpp
 *
 */

#include <fastrtps/subscriber/SubscriberHistory.h>
#include "SubscriberImpl.h"
#include "../rtps/reader/RTPSReaderImpl.h"
#include <fastrtps/rtps/reader/WriterProxy.h>

#include <fastrtps/TopicDataType.h>
#include <fastrtps/log/Log.h>

#include <mutex>

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

inline bool sort_ReaderHistoryCache(CacheChange_t*c1,CacheChange_t*c2)
{
    return c1->sequence_number < c2->sequence_number;
}

SubscriberHistory::SubscriberHistory(Subscriber::impl& subscriber, uint32_t payloadMaxSize,
        const HistoryQosPolicy& history,
        const ResourceLimitsQosPolicy& resource,const MemoryManagementPolicy_t mempolicy):
    ReaderHistory(HistoryAttributes(mempolicy, payloadMaxSize,resource.allocated_samples,resource.max_samples + 1)),
    m_unreadCacheCount(0),
    m_historyQos(history),
    m_resourceLimitsQos(resource),
    subscriber_(subscriber),
    mp_getKeyObject(nullptr)
{
    mp_getKeyObject = subscriber_.getType()->createData();
}

SubscriberHistory::~SubscriberHistory() {
    subscriber_.getType()->deleteData(mp_getKeyObject);

}

bool SubscriberHistory::received_change(CacheChange_t* a_change, size_t unknown_missing_changes_up_to)
{

    if(reader_ == nullptr || mp_mutex == nullptr)
    {
        logError(RTPS_HISTORY,"You need to create a Reader with this History before using it");
        return false;
    }

    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    //NO KEY HISTORY
    if(subscriber_.getAttributes().topic.getTopicKind() == NO_KEY)
    {
        bool add = false;
        if(m_historyQos.kind == KEEP_ALL_HISTORY_QOS)
        {
            // TODO(Ricardo) Check
            if(m_resourceLimitsQos.max_samples == 0 ||
                    m_changes.size() + unknown_missing_changes_up_to < (size_t)m_resourceLimitsQos.max_samples)
            {
                add = true;
            }
        }
        else if(m_historyQos.kind == KEEP_LAST_HISTORY_QOS)
        {
            if(m_changes.size() < (size_t)m_historyQos.depth)
            {
                add = true;
            }
            else
            {
                // TODO (Ricardo) Older samples should be selected by sourcetimestamp.

                // Try to substitute a older samples.
                CacheChange_t* older = nullptr;

                for(auto it = m_changes.begin(); it != m_changes.end(); ++it)
                {
                    if((*it)->writer_guid == a_change->writer_guid &&
                            (*it)->sequence_number < a_change->sequence_number)
                    {
                        older = *it;
                        break;
                    }
                }

                if(older != nullptr)
                {
                    bool read = older->is_read;

                    if(this->remove_change_sub(older))
                    {
                        if(!read)
                        {
                            this->decreaseUnreadCount();
                        }
                        add = true;
                    }
                }
            }
        }

        if(add)
        {
            if(m_isHistoryFull)
            {
                // Discarting the sample.
                logWarning(SUBSCRIBER,"Attempting to add Data to Full ReaderHistory: "<<this->subscriber_.getGuid().entityId);
                return false;
            }

            if(this->add_change(a_change))
            {
                increaseUnreadCount();
                if((int32_t)m_changes.size()==m_resourceLimitsQos.max_samples)
                    m_isHistoryFull = true;
                logInfo(SUBSCRIBER,this->subscriber_.getGuid().entityId
                        <<": Change "<< a_change->sequence_number << " added from: "
                        << a_change->writer_guid;);
                //print_changes_seqNum();
                return true;
            }
        }
    }
    //HISTORY WITH KEY
    else if(subscriber_.getAttributes().topic.getTopicKind() == WITH_KEY)
    {
        if(!a_change->instance_handle.is_unknown() && subscriber_.getType() !=nullptr)
        {
            logInfo(RTPS_HISTORY,"Getting Key of change with no Key transmitted")
                subscriber_.getType()->deserialize(&a_change->serialized_payload, mp_getKeyObject);
            if(!subscriber_.getType()->getKey(mp_getKeyObject, &a_change->instance_handle))
                return false;

        }
        else if(!a_change->instance_handle.is_unknown())
        {
            logWarning(RTPS_HISTORY,"NO KEY in topic: "<< this->subscriber_.getAttributes().topic.topicName
                    << " and no method to obtain it";);
            return false;
        }
        t_v_Inst_Caches::iterator vit;
        if(find_Key(a_change,&vit))
        {
            //logInfo(RTPS_EDP,"Trying to add change with KEY: "<< vit->first << endl;);
            bool add = false;
            if(m_historyQos.kind == KEEP_ALL_HISTORY_QOS)
            {
                if((int32_t)vit->second.size() < m_resourceLimitsQos.max_samples_per_instance)
                {
                    add = true;
                }
                else
                {
                    logWarning(SUBSCRIBER,"Change not added due to maximum number of samples per instance";);
                    return false;
                }
            }
            else if (m_historyQos.kind == KEEP_LAST_HISTORY_QOS)
            {
                if(vit->second.size()< (size_t)m_historyQos.depth)
                {
                    add = true;
                }
                else
                {
                    // Try to substitude a older samples.
                    auto older_sample = m_changes.rend();
                    for(auto it = m_changes.rbegin(); it != m_changes.rend(); ++it)
                    {

                        if((*it)->writer_guid == a_change->writer_guid)
                        {
                            if((*it)->sequence_number < a_change->sequence_number)
                                older_sample = it;
                            // Already received
                            else if((*it)->sequence_number == a_change->sequence_number)
                                return false;
                        }
                    }

                    if(older_sample != m_changes.rend())
                    {
                        bool read = (*older_sample)->is_read;

                        if(this->remove_change_sub(*older_sample, &vit))
                        {
                            if(!read)
                            {
                                this->decreaseUnreadCount();
                            }
                            add = true;
                        }
                    }
                }
            }

            if(add)
            {
                if(m_isHistoryFull)
                {
                    // Discarting the sample.
                    logWarning(SUBSCRIBER,"Attempting to add Data to Full ReaderHistory: "<<this->subscriber_.getGuid().entityId);
                    return false;
                }

                if(this->add_change(a_change))
                {
                    increaseUnreadCount();
                    if((int32_t)m_changes.size()==m_resourceLimitsQos.max_samples)
                        m_isHistoryFull = true;
                    //ADD TO KEY VECTOR
                    if(vit->second.size() == 0)
                    {
                        vit->second.push_back(a_change);
                    }
                    else if(vit->second.back()->sequence_number < a_change->sequence_number)
                    {
                        vit->second.push_back(a_change);
                    }
                    else
                    {
                        vit->second.push_back(a_change);
                        std::sort(vit->second.begin(),vit->second.end(),sort_ReaderHistoryCache);
                    }
                    logInfo(SUBSCRIBER,this->reader_->getGuid().entityId
                            <<": Change "<< a_change->sequence_number << " added from: "
                            << a_change->writer_guid<< " with KEY: "<< a_change->instance_handle;);
                    //	print_changes_seqNum();
                    return true;
                }
            }
        }
    }

    return false;
}

bool SubscriberHistory::readNextData(void* data, SampleInfo_t* info)
{

    if(reader_ == nullptr || mp_mutex == nullptr)
    {
        logError(RTPS_HISTORY,"You need to create a Reader with this History before using it");
        return false;
    }

    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    CacheChange_t* change;
    WriterProxy * wp;
    if(this->reader_->nextUnreadCache(&change,&wp))
    {
        change->is_read = true;
        this->decreaseUnreadCount();
        logInfo(SUBSCRIBER,this->reader_->getGuid().entityId <<": reading "<<
                change->sequence_number );
        if(change->kind == ALIVE)
            this->subscriber_.getType()->deserialize(&change->serialized_payload, data);
        if(info!=nullptr)
        {
            info->sampleKind = change->kind;
            info->sample_identity.writer_guid(change->writer_guid);
            info->sample_identity.sequence_number(change->sequence_number);
            info->sourceTimestamp = change->source_timestamp;
            if(this->subscriber_.getAttributes().qos.m_ownership.kind == EXCLUSIVE_OWNERSHIP_QOS)
                info->ownershipStrength = wp->m_att.ownershipStrength;
            if(this->subscriber_.getAttributes().topic.topicKind == WITH_KEY &&
                    change->instance_handle == c_InstanceHandle_Unknown &&
                    change->kind == ALIVE)
            {
                this->subscriber_.getType()->getKey(data, &change->instance_handle);
            }
            info->iHandle = change->instance_handle;
            info->related_sample_identity = change->write_params.sample_identity();
        }
        return true;
    }
    return false;
}


bool SubscriberHistory::takeNextData(void* data, SampleInfo_t* info)
{

    if(reader_ == nullptr || mp_mutex == nullptr)
    {
        logError(RTPS_HISTORY,"You need to create a Reader with this History before using it");
        return false;
    }

    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    CacheChange_t* change;
    WriterProxy * wp;
    if(this->reader_->nextUntakenCache(&change,&wp))
    {
        if(!change->is_read)
            this->decreaseUnreadCount();
        change->is_read = true;
        logInfo(SUBSCRIBER,this->reader_->getGuid().entityId <<": taking seqNum" <<
                change->sequence_number << " from writer: "<< change->writer_guid);
        if(change->kind == ALIVE)
            this->subscriber_.getType()->deserialize(&change->serialized_payload, data);
        if(info!=nullptr)
        {
            info->sampleKind = change->kind;
            info->sample_identity.writer_guid(change->writer_guid);
            info->sample_identity.sequence_number(change->sequence_number);
            info->sourceTimestamp = change->source_timestamp;
            if(this->subscriber_.getAttributes().qos.m_ownership.kind == EXCLUSIVE_OWNERSHIP_QOS)
                info->ownershipStrength = wp->m_att.ownershipStrength;
            if(this->subscriber_.getAttributes().topic.topicKind == WITH_KEY &&
                    change->instance_handle == c_InstanceHandle_Unknown &&
                    change->kind == ALIVE)
            {
                this->subscriber_.getType()->getKey(data, &change->instance_handle);
            }
            info->iHandle = change->instance_handle;
            info->related_sample_identity = change->write_params.sample_identity();
        }
        this->remove_change_sub(change);
        return true;
    }

    return false;
}

bool SubscriberHistory::find_Key(CacheChange_t* a_change, t_v_Inst_Caches::iterator* vit_out)
{
    t_v_Inst_Caches::iterator vit;
    bool found = false;
    for (vit = m_keyedChanges.begin(); vit != m_keyedChanges.end(); ++vit)
    {
        if (a_change->instance_handle == vit->first)
        {
            *vit_out = vit;
            return true;
        }
    }
    if (!found)
    {
        if ((int)m_keyedChanges.size() < m_resourceLimitsQos.max_instances)
        {
            t_p_I_Change newpair;
            newpair.first = a_change->instance_handle;
            m_keyedChanges.push_back(newpair);
            *vit_out = m_keyedChanges.end() - 1;
            return true;
        }
        else
        {
            for (vit = m_keyedChanges.begin(); vit != m_keyedChanges.end(); ++vit)
            {
                if (vit->second.size() == 0)
                {
                    m_keyedChanges.erase(vit);
                    t_p_I_Change newpair;
                    newpair.first = a_change->instance_handle;
                    m_keyedChanges.push_back(newpair);
                    *vit_out = m_keyedChanges.end() - 1;
                    return true;
                }
            }
            logWarning(SUBSCRIBER, "History has reached the maximum number of instances");
        }

    }
    return false;
}


bool SubscriberHistory::remove_change_sub(CacheChange_t* change,t_v_Inst_Caches::iterator* vit_in)
{

    if(reader_ == nullptr || mp_mutex == nullptr)
    {
        logError(RTPS_HISTORY,"You need to create a Reader with this History before using it");
        return false;
    }

    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    if(subscriber_.getAttributes().topic.getTopicKind() == NO_KEY)
    {
        if(this->remove_change(change))
        {
            m_isHistoryFull = false;
            return true;
        }
        return false;
    }
    else
    {
        t_v_Inst_Caches::iterator vit;
        if(vit_in!=nullptr)
            vit = *vit_in;
        else if(this->find_Key(change,&vit))
        {

        }
        else
            return false;
        for(auto chit = vit->second.begin();
                chit!= vit->second.end();++chit)
        {
            if((*chit)->sequence_number == change->sequence_number
                    && (*chit)->writer_guid == change->writer_guid)
            {
                if(remove_change(change))
                {
                    vit->second.erase(chit);
                    m_isHistoryFull = false;
                    return true;
                }
            }
        }
        logError(SUBSCRIBER,"Change not found, something is wrong");
    }
    return false;
}
