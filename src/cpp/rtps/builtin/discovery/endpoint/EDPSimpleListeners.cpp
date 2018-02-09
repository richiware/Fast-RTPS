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
 * @file EDPSimpleListener.cpp
 *
 */

#include "EDPSimpleListeners.h"

#include <fastrtps/rtps/builtin/data/WriterProxyData.h>
#include <fastrtps/rtps/builtin/data/ReaderProxyData.h>
#include <fastrtps/rtps/builtin/discovery/endpoint/EDPSimple.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include "../../../participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/reader/StatefulReader.h>

#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/rtps/common/InstanceHandle.h>

#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>

#include <mutex>

#include <fastrtps/log/Log.h>

namespace eprosima {
namespace fastrtps{
namespace rtps {

void EDPSimplePUBListener::onNewCacheChangeAdded(RTPSReader& reader, const CacheChange_t* const change_in)
{
    CacheChange_t* change = (CacheChange_t*)change_in;
    //std::lock_guard<std::recursive_mutex> guard(*this->mp_SEDP->mp_PubReader.first->getMutex());
    logInfo(RTPS_EDP,"");
    if(!computeKey(change))
    {
        logWarning(RTPS_EDP,"Received change with no Key");
    }

    //Call the slave, if it exists
    //TODO(Ricardo) Mutex!!
    if(get_internal_listener() != nullptr)
    {
        get_internal_listener()->onNewCacheChangeAdded(*edpsimple_.mp_PubReader.first, change_in);
    }

    if(change->kind == ALIVE)
    {
        //LOAD INFORMATION IN TEMPORAL WRITER PROXY DATA
        WriterProxyData writerProxyData;
        CDRMessage_t tempMsg(0);
        tempMsg.wraps = true;
        tempMsg.msg_endian = change_in->serialized_payload.encapsulation == PL_CDR_BE ? BIGEND : LITTLEEND;
        tempMsg.length = change_in->serialized_payload.length;
        tempMsg.max_size = change_in->serialized_payload.max_size;
        tempMsg.buffer = change_in->serialized_payload.data;

        if(writerProxyData.readFromCDRMessage(&tempMsg))
        {
            change->instance_handle = writerProxyData.key();
            if(writerProxyData.guid().guidPrefix == edpsimple_.participant_.guid().guidPrefix)
            {
                logInfo(RTPS_EDP,"Message from own RTPSParticipant, ignoring");
                edpsimple_.mp_PubReader.second->remove_change(change);
                return;
            }

            //LOOK IF IS AN UPDATED INFORMATION
            ParticipantProxyData pdata;
            if(edpsimple_.pdpsimple_.addWriterProxyData(&writerProxyData, pdata)) //ADDED NEW DATA
            {
                // At this point we can release reader lock, cause change is not used
                // TODO(Ricardo) Problemo!!!
                //reader.getMutex()->unlock();

                edpsimple_.pairing_writer_proxy_with_any_local_reader(&pdata, &writerProxyData);

                // Take again the reader lock.
                //reader->getMutex()->lock();
            }
            else //NOT ADDED BECAUSE IT WAS ALREADY THERE
            {
                logWarning(RTPS_EDP,"Received message from UNKNOWN RTPSParticipant, removing");
            }
        }
    }
    else
    {
        //REMOVE WRITER FROM OUR READERS:
        logInfo(RTPS_EDP,"Disposed Remote Writer, removing...");

        GUID_t auxGUID = iHandle2GUID(change->instance_handle);
        edpsimple_.pdpsimple_.removeWriterProxyData(auxGUID);
    }

    //Removing change from history
    edpsimple_.mp_PubReader.second->remove_change(change);

    return;
}

static inline bool compute_key(CDRMessage_t* aux_msg,CacheChange_t* change)
{
    if(change->instance_handle == c_InstanceHandle_Unknown)
    {
        SerializedPayload_t* pl = &change->serialized_payload;
        aux_msg->buffer = pl->data;
        aux_msg->length = pl->length;
        aux_msg->max_size = pl->max_size;
        aux_msg->msg_endian = pl->encapsulation == PL_CDR_BE ? BIGEND : LITTLEEND;
        bool valid = false;
        uint16_t pid;
        uint16_t plength;
        while(aux_msg->pos < aux_msg->length)
        {
            valid = true;
            valid&=CDRMessage::readUInt16(aux_msg,(uint16_t*)&pid);
            valid&=CDRMessage::readUInt16(aux_msg,&plength);
            if(pid == PID_SENTINEL)
            {
                break;
            }
            if(pid == PID_KEY_HASH)
            {
                valid &= CDRMessage::readData(aux_msg,change->instance_handle.value,16);
                aux_msg->buffer = nullptr;
                return true;
            }
            if(pid == PID_ENDPOINT_GUID)
            {
                valid &= CDRMessage::readData(aux_msg,change->instance_handle.value,16);
                aux_msg->buffer = nullptr;
                return true;
            }
            aux_msg->pos+=plength;
        }
        aux_msg->buffer = nullptr;
        return false;
    }
    aux_msg->buffer = nullptr;
    return true;
}

bool EDPSimplePUBListener::computeKey(CacheChange_t* change)
{
    CDRMessage_t aux_msg(0);
    return compute_key(&aux_msg,change);
}

bool EDPSimpleSUBListener::computeKey(CacheChange_t* change)
{
    CDRMessage_t aux_msg(0);
    return compute_key(&aux_msg,change);
}

void EDPSimpleSUBListener::onNewCacheChangeAdded(RTPSReader& reader, const CacheChange_t* const change_in)
{
    CacheChange_t* change = (CacheChange_t*)change_in;
    //std::lock_guard<std::recursive_mutex> guard(*this->mp_SEDP->mp_SubReader.first->getMutex());
    logInfo(RTPS_EDP,"");
    if(!computeKey(change))
    {
        logWarning(RTPS_EDP,"Received change with no Key");
    }

    //Call the slave, if it exists
    if(get_internal_listener() != nullptr)
    {
        get_internal_listener()->onNewCacheChangeAdded(*edpsimple_.mp_SubReader.first, change);
    }

    if(change->kind == ALIVE)
    {
        //LOAD INFORMATION IN TEMPORAL WRITER PROXY DATA
        ReaderProxyData readerProxyData;
        CDRMessage_t tempMsg(0);
        tempMsg.wraps = true;
        tempMsg.msg_endian = change_in->serialized_payload.encapsulation == PL_CDR_BE ? BIGEND : LITTLEEND;
        tempMsg.length = change_in->serialized_payload.length;
        tempMsg.max_size = change_in->serialized_payload.max_size;
        tempMsg.buffer = change_in->serialized_payload.data;

        if(readerProxyData.readFromCDRMessage(&tempMsg))
        {
            change->instance_handle = readerProxyData.key();
            if(readerProxyData.guid().guidPrefix == edpsimple_.participant_.guid().guidPrefix)
            {
                logInfo(RTPS_EDP,"From own RTPSParticipant, ignoring");
                edpsimple_.mp_SubReader.second->remove_change(change);
                return;
            }

            //LOOK IF IS AN UPDATED INFORMATION
            ParticipantProxyData pdata;
            if(edpsimple_.pdpsimple_.addReaderProxyData(&readerProxyData, pdata)) //ADDED NEW DATA
            {
                // At this point we can release reader lock, cause change is not used
                //TODO(Ricardo) Maydayyy!!
                //reader->getMutex()->unlock();

                edpsimple_.pairing_reader_proxy_with_any_local_writer(&pdata, &readerProxyData);

                // Take again the reader lock.
                //reader->getMutex()->lock();
            }
            else
            {
                logWarning(RTPS_EDP,"From UNKNOWN RTPSParticipant, removing");
            }
        }
    }
    else
    {
        //REMOVE WRITER FROM OUR READERS:
        logInfo(RTPS_EDP,"Disposed Remote Reader, removing...");

        GUID_t auxGUID = iHandle2GUID(change->instance_handle);
        edpsimple_.pdpsimple_.removeReaderProxyData(auxGUID);
    }

    // Remove change from history.
    edpsimple_.mp_SubReader.second->remove_change(change);

    return;
}

} /* namespace rtps */
}
} /* namespace eprosima */
