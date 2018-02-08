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

/*
 * Publisher.cpp
 *
 */

#include "PublisherImpl.h"
#include "../participant/ParticipantImpl.h"
#include <fastrtps/TopicDataType.h>
#include <fastrtps/rtps/attributes/HistoryAttributes.h>
#include "../rtps/writer/RTPSWriterImpl.h"
#include "../rtps/writer/StatefulWriterImpl.h"
#include <fastrtps/rtps/participant/RTPSParticipant.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/utils/TimeConversion.h>

using namespace eprosima::fastrtps;
using namespace ::rtps;

::rtps::WriteParams WRITE_PARAM_DEFAULT;

Publisher::impl::impl(Participant::impl& participant, TopicDataType* type,
        const PublisherAttributes& att, Publisher::impl::Listener* listener) :
    participant_(participant), writer_(nullptr), type_(type), att_(att),
#pragma warning (disable : 4355 )
    history_(HistoryAttributes(att.historyMemoryPolicy, type->m_typeSize,
                att.topic.resourceLimitsQos.allocated_samples, att.topic.resourceLimitsQos.max_samples)),
    listener_(listener),
#pragma warning (disable : 4355 )
    writer_listener_(*this),
    high_mark_for_frag_(0)
{
}

bool Publisher::impl::init()
{
    history_.call_after_adding_change(std::bind(&Publisher::impl::unsent_change_added_to_history, this,
            std::placeholders::_1));
    history_.call_after_deleting_change(std::bind(&Publisher::impl::change_removed_by_history, this,
            std::placeholders::_1, std::placeholders::_2));

    WriterAttributes watt;
    watt.throughputController = att_.throughputController;
    watt.endpoint.durabilityKind = att_.qos.m_durability.kind == VOLATILE_DURABILITY_QOS ? VOLATILE : TRANSIENT_LOCAL;
    watt.endpoint.endpointKind = WRITER;
    watt.endpoint.multicastLocatorList = att_.multicastLocatorList;
    watt.endpoint.reliabilityKind = att_.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS ? RELIABLE : BEST_EFFORT;
    watt.endpoint.topicKind = att_.topic.topicKind;
    watt.endpoint.unicastLocatorList = att_.unicastLocatorList;
    watt.endpoint.outLocatorList = att_.outLocatorList;
    watt.mode = att_.qos.m_publishMode.kind == eprosima::fastrtps::SYNCHRONOUS_PUBLISH_MODE ? SYNCHRONOUS_WRITER : ASYNCHRONOUS_WRITER;
    watt.endpoint.properties = att_.properties;
    if(att_.getEntityID() > 0)
    {
        watt.endpoint.setEntityID((uint8_t)att_.getEntityID());
    }
    if(att_.getUserDefinedID() > 0)
    {
        watt.endpoint.setUserDefinedID((uint8_t)att_.getUserDefinedID());
    }
    watt.times = att_.times;

    writer_ = participant_.rtps_participant().create_writer(watt, history_, &writer_listener_);

    if(writer_)
    {
        // Register the writer
        if(participant_.rtps_participant().register_writer(*writer_, att_.topic, att_.qos))
        {
            return true;
        }
        else
        {
            logError(PUBLISHER,"Failed registering associated writer");
        }
        //TODO (Ricardo) Remove writer
    }
    else
    {
        logError(PUBLISHER,"Problem creating associated writer");
    }

    return false;
}

Publisher::impl::~impl()
{
    deinit();
}

void Publisher::impl::deinit()
{
    if(writer_)
    {
        logInfo(PUBLISHER, this->getGuid().entityId << " in topic: " << att_.topic.topicName);
        participant_.rtps_participant().remove_writer(writer_);
    }
}

bool Publisher::impl::create_new_change(ChangeKind_t change_kind, void* data)
{
    return create_new_change_with_params(change_kind, data, WRITE_PARAM_DEFAULT);
}

bool Publisher::impl::create_new_change_with_params(ChangeKind_t change_kind, void* data, WriteParams &wparams)
{
    /// Preconditions
    if (data == nullptr)
    {
        logError(PUBLISHER, "Data pointer not valid");
        return false;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    if(change_kind == NOT_ALIVE_UNREGISTERED || change_kind == NOT_ALIVE_DISPOSED ||
            change_kind == NOT_ALIVE_DISPOSED_UNREGISTERED)
    {
        if(att_.topic.topicKind == NO_KEY)
        {
            logError(PUBLISHER, "Topic is NO_KEY, operation not permitted");
            return false;
        }
    }

    InstanceHandle_t handle;

    if(att_.topic.topicKind == WITH_KEY)
    {
        type_->getKey(data,&handle);
    }

    map::iterator map_it = changes_by_instance_.find(handle);

    // Currently write fails if exceeded max_instances. Check this value.
    if(att_.topic.resourceLimitsQos.max_instances > 0 &&
            static_cast<int32_t>(changes_by_instance_.size()) == att_.topic.resourceLimitsQos.max_instances &&
            map_it == changes_by_instance_.end())
    {
        logWarning(PUBLISHER, "Exceeded QoS max_instances");
        return false;
    }

    //NOTA Cuando se vaya a eliminar un dato por KEEP_ALL, antes se usaba try_to_remove. Ahora llamar al
    //remove_min_change del historial que devuelve si ha podido o no.

    CacheChange_ptr change = writer_->new_change(type_->getSerializedSizeProvider(data), change_kind, handle);

    if(change)
    {
        if(change_kind == ALIVE)
        {
            //If these two checks are correct, we asume the cachechange is valid and thwn we can write to it.
            if(!type_->serialize(data, &change->serialized_payload))
            {
                logWarning(RTPS_WRITER, "RTPSWriter:Serialization returns false";);
                return false;
            }
        }

        //TODO(Ricardo) This logic in a class. Then a user of rtps layer can use it.
        if(high_mark_for_frag_ == 0)
        {
            uint32_t max_data_size = writer_->getMaxDataSize();
            uint32_t writer_throughput_controller_bytes =
                writer_->calculateMaxDataSize(att_.throughputController.bytesPerPeriod);
            uint32_t participant_throughput_controller_bytes =
                writer_->calculateMaxDataSize(participant_.rtps_participant().getRTPSParticipantAttributes().throughputController.bytesPerPeriod);

            high_mark_for_frag_ =
                max_data_size > writer_throughput_controller_bytes ?
                writer_throughput_controller_bytes :
                (max_data_size > participant_throughput_controller_bytes ?
                 participant_throughput_controller_bytes :
                 max_data_size);
        }

        uint32_t final_high_mark_for_frag = high_mark_for_frag_;

        // If needed inlineqos for related_sample_identity, then remove the inlinqos size from final fragment size.
        if(wparams.related_sample_identity() != SampleIdentity::unknown())
            final_high_mark_for_frag -= 32;

        // If it is big data, fragment it.
        if(change->serialized_payload.length > final_high_mark_for_frag)
        {
            // Check ASYNCHRONOUS_PUBLISH_MODE is being used, but it is an error case.
            if(att_.qos.m_publishMode.kind != ASYNCHRONOUS_PUBLISH_MODE)
            {
                logError(PUBLISHER, "Data cannot be sent. It's serialized size is " <<
                        change->serialized_payload.length << "' which exceeds the maximum payload size of '" <<
                        final_high_mark_for_frag << "' and therefore ASYNCHRONOUS_PUBLISH_MODE must be used.");
                return false;
            }

            /// Fragment the data.
            // Set the fragment size to the cachechange.
            // Note: high_mark will always be a value that can be casted to uint16_t)
            change->setFragmentSize((uint16_t)final_high_mark_for_frag);
        }

        if(&wparams != &WRITE_PARAM_DEFAULT)
        {
            change->write_params = wparams;
        }

        //TODO Falta que el history vuelva a llamar aqui, como hacia con el PublisherHistory.
        if(!history_.add_change(change))
        {
            return false;
        }

        return true;
    }

    return false;
}


bool Publisher::impl::unsent_change_added_to_history(const CacheChange_t& change)
{
    return writer_->unsent_change_added_to_history(change);
}

bool Publisher::impl::change_removed_by_history(const SequenceNumber_t& sequence_number,
                const InstanceHandle_t& handle)
{
    return writer_->change_removed_by_history(sequence_number, handle);
}

//TODO deprecated
bool Publisher::impl::removeMinSeqChange()
{
    CacheChange_ptr change = history_.remove_min_change();

    if(change)
    {
        return true;
    }

    return false;
}

// TODO(Ricardo) deprecated
bool Publisher::impl::removeAllChange(size_t*)
{
    return false; //history_.removeAllChange(removed);
}

const GUID_t& Publisher::impl::getGuid()
{
    return writer_->getGuid();
}
//
bool Publisher::impl::updateAttributes(PublisherAttributes& att)
{
    bool updated = true;
    bool missing = false;
    if(att_.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS)
    {
        if(att.unicastLocatorList.size() != att_.unicastLocatorList.size() ||
                att.multicastLocatorList.size() != att_.multicastLocatorList.size())
        {
            logWarning(PUBLISHER,"Locator Lists cannot be changed or updated in this version");
            updated &= false;
        }
        else
        {
            for(LocatorListIterator lit1 = att_.unicastLocatorList.begin();
                    lit1 != att_.unicastLocatorList.end();++lit1)
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
                    logWarning(PUBLISHER,"Locator: "<< *lit1 << " not present in new list");
                    logWarning(PUBLISHER,"Locator Lists cannot be changed or updated in this version");
                }
            }
            for(LocatorListIterator lit1 = att_.multicastLocatorList.begin();
                    lit1 != att_.multicastLocatorList.end();++lit1)
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
                    logWarning(PUBLISHER,"Locator: "<< *lit1<< " not present in new list");
                    logWarning(PUBLISHER,"Locator Lists cannot be changed or updated in this version");
                }
            }
        }
    }

    //TOPIC ATTRIBUTES
    if(att_.topic != att.topic)
    {
        logWarning(PUBLISHER,"Topic Attributes cannot be updated");
        updated &= false;
    }
    //QOS:
    //CHECK IF THE QOS CAN BE SET
    if(!att_.qos.canQosBeUpdated(att.qos))
    {
        updated &=false;
    }
    if(updated)
    {
        if(att_.qos.m_reliability.kind == RELIABLE_RELIABILITY_QOS)
        {
            //UPDATE TIMES:
            StatefulWriter::impl* sfw = dynamic_cast<StatefulWriter::impl*>(writer_.get());
            sfw->updateTimes(att.times);
        }
        att_.qos.setQos(att.qos,false);
        att_ = att;
        //Notify the participant that a Writer has changed its QOS
        participant_.rtps_participant().update_local_writer(*writer_, att_. qos);
    }


    return updated;
}

void Publisher::impl::PublisherWriterListener::onWriterMatched(RTPSWriter::impl& writer, const MatchingInfo& info)
{
    (void)writer;
    assert(publisher_.writer_.get() == &writer);

    if(publisher_.listener_ != nullptr)
    {
        publisher_.listener_->onPublicationMatched(publisher_, info);
    }
}

bool Publisher::impl::wait_for_all_acked(const Time_t& max_wait)
{
    return writer_->wait_for_all_acked(max_wait);
}
