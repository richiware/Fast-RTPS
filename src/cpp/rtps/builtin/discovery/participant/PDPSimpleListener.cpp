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
 * @file PDPSimpleListener.cpp
 *
 */

#include "PDPSimpleListener.h"
#include <fastrtps/rtps/builtin/discovery/participant/timedevent/RemoteParticipantLeaseDuration.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include "../../../participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/builtin/discovery/endpoint/EDP.h>
#include <fastrtps/rtps/reader/RTPSReader.h>
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/participant/RTPSParticipantDiscoveryInfo.h>
#include <fastrtps/rtps/participant/RTPSParticipantListener.h>
#include <fastrtps/utils/TimeConversion.h>
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {



void PDPSimpleListener::onNewCacheChangeAdded(RTPSReader::impl& reader, const CacheChange_t* const change_in)
{
    CacheChange_t* change = (CacheChange_t*)(change_in);
    logInfo(RTPS_PDP,"SPDP Message received");
    if(change->instance_handle == c_InstanceHandle_Unknown)
    {
        if(!this->getKey(change))
        {
            logWarning(RTPS_PDP,"Problem getting the key of the change, removing");
            pdpsimple_.spdp_reader_history_->remove_change(change);
            return;
        }
    }
    if(change->kind == ALIVE)
    {
        //LOAD INFORMATION IN TEMPORAL RTPSParticipant PROXY DATA
        ParticipantProxyData participant_data;
        CDRMessage_t msg;
        msg.msg_endian = change->serialized_payload.encapsulation == PL_CDR_BE ? BIGEND:LITTLEEND;
        msg.length = change->serialized_payload.length;
        memcpy(msg.buffer,change->serialized_payload.data,msg.length);
        if(participant_data.readFromCDRMessage(&msg))
        {
            //AFTER CORRECTLY READING IT
            //CHECK IF IS THE SAME RTPSParticipant
            change->instance_handle = participant_data.m_key;
            if(participant_data.m_guid == pdpsimple_.participant().guid())
            {
                logInfo(RTPS_PDP,"Message from own RTPSParticipant, removing");
                pdpsimple_.spdp_reader_history_->remove_change(change);
                return;
            }

            // At this point we can release reader lock.
            reader.getMutex()->unlock();

            //LOOK IF IS AN UPDATED INFORMATION
            ParticipantProxyData* pdata = nullptr;
            std::unique_lock<std::recursive_mutex> lock(*pdpsimple_.getMutex());
            for (auto it = pdpsimple_.m_participantProxies.begin(); it != pdpsimple_.m_participantProxies.end();++it)
            {
                if(participant_data.m_key == (*it)->m_key)
                {
                    pdata = (*it);
                    break;
                }
            }

            RTPSParticipantDiscoveryInfo info;
            info.m_guid = participant_data.m_guid;
            info.m_RTPSParticipantName = participant_data.m_participantName;
            info.m_propertyList = participant_data.m_properties.properties;
            info.m_userData = participant_data.m_userData;

            if(pdata == nullptr)
            {
                info.m_status = DISCOVERED_RTPSPARTICIPANT;
                //IF WE DIDNT FOUND IT WE MUST CREATE A NEW ONE
                pdata = new ParticipantProxyData(participant_data);
                pdata->isAlive = true;
                pdata->mp_leaseDurationTimer = new RemoteParticipantLeaseDuration(pdpsimple_,
                        pdata,
                        TimeConv::Time_t2MilliSecondsDouble(pdata->m_leaseDuration));
                pdata->mp_leaseDurationTimer->restart_timer();
                pdpsimple_.m_participantProxies.push_back(pdata);
                lock.unlock();

                pdpsimple_.assignRemoteEndpoints(&participant_data);
                pdpsimple_.announceParticipantState(false);
            }
            else
            {
                info.m_status = CHANGED_QOS_RTPSPARTICIPANT;
                pdata->updateData(participant_data);
                pdata->isAlive = true;
                lock.unlock();

                if(pdpsimple_.m_discovery.use_STATIC_EndpointDiscoveryProtocol)
                    pdpsimple_.mp_EDP->assignRemoteEndpoints(participant_data);
            }

            if(pdpsimple_.participant().listener() != nullptr)
            {
                pdpsimple_.participant().listener()->onRTPSParticipantDiscovery(
                        pdpsimple_.participant(), info);
            }

            // Take again the reader lock
            reader.getMutex()->lock();
        }
    }
    else
    {
        GUID_t guid;
        iHandle2GUID(guid, change->instance_handle);

        if(pdpsimple_.removeRemoteParticipant(guid))
        {
            if(pdpsimple_.participant().listener() != nullptr)
            {
                RTPSParticipantDiscoveryInfo info;
                info.m_status = REMOVED_RTPSPARTICIPANT;
                info.m_guid = guid;
                if(pdpsimple_.participant().listener() != nullptr)
                {
                    pdpsimple_.participant().listener()->onRTPSParticipantDiscovery(
                            pdpsimple_.participant(), info);
                }
            }
        }
    }

    //Remove change form history.
    pdpsimple_.spdp_reader_history_->remove_change(change);

    return;
}

bool PDPSimpleListener::getKey(CacheChange_t* change)
{
    SerializedPayload_t* pl = &change->serialized_payload;
    CDRMessage::initCDRMsg(&aux_msg);
    // TODO CHange because it create a buffer to remove after.
    free(aux_msg.buffer);
    aux_msg.buffer = pl->data;
    aux_msg.length = pl->length;
    aux_msg.max_size = pl->max_size;
    aux_msg.msg_endian = pl->encapsulation == PL_CDR_BE ? BIGEND : LITTLEEND;
    bool valid = false;
    uint16_t pid;
    uint16_t plength;
    while(aux_msg.pos < aux_msg.length)
    {
        valid = true;
        valid&=CDRMessage::readUInt16(&aux_msg,(uint16_t*)&pid);
        valid&=CDRMessage::readUInt16(&aux_msg,&plength);
        if(pid == PID_SENTINEL)
        {
            break;
        }
        if(pid == PID_PARTICIPANT_GUID)
        {
            valid &= CDRMessage::readData(&aux_msg, change->instance_handle.value, 16);
            aux_msg.buffer = nullptr;
            return true;
        }
        if(pid == PID_KEY_HASH)
        {
            valid &= CDRMessage::readData(&aux_msg, change->instance_handle.value, 16);
            aux_msg.buffer = nullptr;
            return true;
        }
        aux_msg.pos+=plength;
    }
    aux_msg.buffer = nullptr;
    return false;
}

}
} /* namespace rtps */
} /* namespace eprosima */
