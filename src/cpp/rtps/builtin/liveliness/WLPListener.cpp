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
 * @file WLPListener.cpp
 *
 */

#include "WLPListener.h"
#include <fastrtps/rtps/builtin/liveliness/WLP.h>

#include <fastrtps/rtps/history/ReaderHistory.h>

#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>

#include <fastrtps/rtps/reader/StatefulReader.h>

#include <fastrtps/log/Log.h>

#include <mutex>



namespace eprosima {
namespace fastrtps{
namespace rtps {


WLPListener::WLPListener(WLP& wlp) : wlp_(wlp)
{
    free(aux_msg.buffer);
}

WLPListener::~WLPListener()
{
    aux_msg.buffer = nullptr;
}


typedef std::vector<WriterProxy*>::iterator WPIT;

void WLPListener::onNewCacheChangeAdded(RTPSReader::impl& reader, const CacheChange_t* const changeIN)
{
    std::lock_guard<std::recursive_mutex> guard2(*wlp_.pdpsimple_->getMutex());
    logInfo(RTPS_LIVELINESS,"");
    GuidPrefix_t guidP;
    LivelinessQosPolicyKind livelinessKind;
    CacheChange_t* change = (CacheChange_t*)changeIN;
    if(!computeKey(change))
    {
        logWarning(RTPS_LIVELINESS,"Problem obtaining the Key");
        return;
    }
    //Check the serializedPayload:
    for(auto ch = wlp_.mp_builtinReaderHistory->changesBegin();
            ch != wlp_.mp_builtinReaderHistory->changesEnd(); ++ch)
    {
        if((*ch)->instance_handle == change->instance_handle &&
                (*ch)->sequence_number < change->sequence_number)
        {
            wlp_.mp_builtinReaderHistory->remove_change(&**ch);
            break;
        }
    }
    if(change->serialized_payload.length>0)
    {
        for(uint8_t i =0;i<12;++i)
        {
            guidP.value[i] = change->serialized_payload.data[i];
        }
        livelinessKind = (LivelinessQosPolicyKind)(change->serialized_payload.data[15]-0x01);

    }
    else
    {
        if(!separateKey(change->instance_handle,&guidP,&livelinessKind))
            return;
    }
    logInfo(RTPS_LIVELINESS,"RTPSParticipant "<<guidP<< " assert liveliness of "
            <<((livelinessKind == 0x00)?"AUTOMATIC":"")
            <<((livelinessKind==0x01)?"MANUAL_BY_RTPSParticipant":"")<< " writers");
    if(guidP == reader.getGuid().guidPrefix)
    {
        logInfo(RTPS_LIVELINESS,"Message from own RTPSParticipant, ignoring");
        wlp_.mp_builtinReaderHistory->remove_change(change);
        return;
    }
    wlp_.pdpsimple_->assertRemoteWritersLiveliness(guidP,livelinessKind);

    return;
}


//bool WLPListener::processParameterList(ParameterList_t* param,GuidPrefix_t* guidP,LivelinessQosPolicyKind* liveliness)
//{
//	for(std::vector<Parameter_t*>::iterator it=param->m_parameters.begin();
//			it!=param->m_parameters.end();++it)
//	{
//		switch((*it)->Pid)
//		{
//		case(PID_KEY_HASH):
//						{
//			ParameterKey_t* p = (ParameterKey_t*)(*it);
//			return separateKey(p->key,guidP,liveliness);
//						}
//		default:
//		{
//			logWarning(RTPS_LIVELINESS,"In this ParameterList should not be anything but the Key");
//			break;
//		}
//		}
//	}
//	return false;
//}

bool WLPListener::separateKey(InstanceHandle_t& key,GuidPrefix_t* guidP,LivelinessQosPolicyKind* liveliness)
{
    for(uint8_t i=0;i<12;++i)
    {
        guidP->value[i] = key.value[i];
    }
    *liveliness = (LivelinessQosPolicyKind)key.value[15];
    return true;
}

bool WLPListener::computeKey(CacheChange_t* change)
{
    if(change->instance_handle == c_InstanceHandle_Unknown)
    {
        SerializedPayload_t* pl = &change->serialized_payload;
        if(pl->length > 16)
        {
            CDRMessage::initCDRMsg(&aux_msg);
            aux_msg.buffer = pl->data;
            aux_msg.length = pl->length;
            aux_msg.max_size = pl->max_size;
            aux_msg.msg_endian = pl->encapsulation == PL_CDR_BE ? BIGEND : LITTLEEND;
            for(uint8_t i =0;i<16;++i)
            {
                change->instance_handle.value[i] = aux_msg.buffer[i];
            }
            aux_msg.buffer = nullptr;
            return true;
        }
        return false;
    }
    return true;
}


} /* namespace rtps */
} /* namespace eprosima */
}
