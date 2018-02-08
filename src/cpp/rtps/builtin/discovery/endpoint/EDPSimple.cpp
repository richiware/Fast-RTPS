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
 * @file EDPSimple.cpp
 *
 */

#include <fastrtps/rtps/builtin/discovery/endpoint/EDPSimple.h>
#include "EDPSimpleListeners.h"
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include "../../../participant/RTPSParticipantImpl.h"
#include "../../../writer/StatefulWriterImpl.h"
#include "../../../reader/StatefulReaderImpl.h"
#include <fastrtps/rtps/attributes/HistoryAttributes.h>
#include <fastrtps/rtps/attributes/WriterAttributes.h>
#include <fastrtps/rtps/attributes/ReaderAttributes.h>
#include <fastrtps/rtps/history/ReaderHistory.h>
#include "../../../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/builtin/data/WriterProxyData.h>
#include <fastrtps/rtps/builtin/data/ReaderProxyData.h>
#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {

EDPSimple::EDPStatefulReader::EDPStatefulReader(std::shared_ptr<RTPSReader::impl>& impl,
        EDPSimpleListener* listener) : StatefulReader(impl, listener), user_listener_(listener)
{
}

//TODO(Ricardo) This about getListener. necessary? better return copy than pointer.
ReaderListener* EDPSimple::EDPStatefulReader::getListener()
{
    assert(user_listener_);
    return nullptr;
}

/**
 * Switch the ReaderListener kind for the Reader.
 * If the RTPSReader does not belong to the built-in protocols it switches out the old one.
 * If it belongs to the built-in protocols, it sets the new ReaderListener callbacks to be called after the 
 * built-in ReaderListener ones.
 * @param target Pointed to ReaderLister to attach
 * @return True is correctly set.
 */
//TODO(Ricardo) better make copy than store  copy than store pointer. Change all
bool EDPSimple::EDPStatefulReader::setListener(ReaderListener* listener)
{
    assert(user_listener_);
    user_listener_->set_internal_listener(listener);
    return true;
}


EDPSimple::EDPSimple(PDPSimple& pdpsimple, RTPSParticipant::impl& participant):
    EDP(pdpsimple, participant),
    mp_pubListen(nullptr),
    mp_subListen(nullptr)

    {
        // TODO Auto-generated constructor stub

    }

EDPSimple::~EDPSimple()
{
    if(this->mp_PubReader.first != nullptr)
    {
        delete mp_PubReader.first;
        delete(mp_PubReader.second);
    }
    if(this->mp_SubReader.first !=nullptr)
    {
        delete mp_SubReader.first;
        delete(mp_SubReader.second);
    }
    if(this->mp_PubWriter.first !=nullptr)
    {
        participant_.remove_writer(mp_PubWriter.first);
        delete(mp_PubWriter.second);
    }
    if(this->mp_SubWriter.first !=nullptr)
    {
        participant_.remove_writer(mp_SubWriter.first);
        delete(mp_SubWriter.second);
    }
    if(mp_pubListen!=nullptr)
        delete(mp_pubListen);
    if(mp_subListen !=nullptr)
        delete(mp_subListen);
}


bool EDPSimple::initEDP(BuiltinAttributes& attributes)
{
    logInfo(RTPS_EDP,"Beginning Simple Endpoint Discovery Protocol");
    m_discovery = attributes;

    if(!createSEDPEndpoints())
    {
        logError(RTPS_EDP,"Problem creation SimpleEDP endpoints");
        return false;
    }
    return true;
}


bool EDPSimple::createSEDPEndpoints()
{
    logInfo(RTPS_EDP,"Beginning");
    WriterAttributes watt;
    ReaderAttributes ratt;
    HistoryAttributes hatt;
    bool created = true;
    if(m_discovery.m_simpleEDP.use_PublicationWriterANDSubscriptionReader)
    {
        hatt.initialReservedCaches = 100;
        hatt.maximumReservedCaches = 5000;
        hatt.payloadMaxSize = DISCOVERY_PUBLICATION_DATA_MAX_SIZE;
        mp_PubWriter.second = new WriterHistory::impl(hatt);
        //Wparam.pushMode = true;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.topicKind = WITH_KEY;
        watt.endpoint.unicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        watt.times.nackResponseDelay.seconds = 0;
        watt.times.nackResponseDelay.fraction = 0;
        watt.times.initialHeartbeatDelay.seconds = 0;
        watt.times.initialHeartbeatDelay.fraction = 0;
        if(participant_.getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
                participant_.getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
            watt.mode = ASYNCHRONOUS_WRITER;

        mp_PubWriter.first = participant_.create_writer(watt, *mp_PubWriter.second, nullptr,
                c_EntityId_SEDPPubWriter, true);

        if(mp_PubWriter.first)
        {
            logInfo(RTPS_EDP,"SEDP Publication Writer created");
        }
        else
        {
            delete(mp_PubWriter.second);
            mp_PubWriter.second = nullptr;
            //TODO(Ricardo) Return?
        }

        hatt.initialReservedCaches = 100;
        hatt.maximumReservedCaches = 1000000;
        hatt.payloadMaxSize = DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;
        mp_SubReader.second = new ReaderHistory(hatt);
        //Rparam.historyMaxSize = 100;
        ratt.expectsInlineQos = false;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.topicKind = WITH_KEY;
        ratt.endpoint.unicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.times.heartbeatResponseDelay.seconds = 0;
        ratt.times.heartbeatResponseDelay.fraction = 0;
        ratt.times.initialAcknackDelay.seconds = 0;
        ratt.times.initialAcknackDelay.fraction = 0;

        std::shared_ptr<RTPSReader::impl> sub_reader = participant_.create_reader(ratt, mp_SubReader.second,
                nullptr, c_EntityId_SEDPSubReader, true);

        if(sub_reader)
        {
            logInfo(RTPS_EDP,"SEDP Subscription Reader created");
        }
        else
        {
            delete(mp_SubReader.second);
            mp_SubReader.second = nullptr;
        }

        mp_subListen = new EDPSimpleSUBListener(*this);
        mp_SubReader.first = new EDPStatefulReader(sub_reader, mp_subListen);
    }
    if(m_discovery.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter)
    {
        hatt.initialReservedCaches = 100;
        hatt.maximumReservedCaches = 1000000;
        hatt.payloadMaxSize = DISCOVERY_PUBLICATION_DATA_MAX_SIZE;
        mp_PubReader.second = new ReaderHistory(hatt);
        //Rparam.historyMaxSize = 100;
        ratt.expectsInlineQos = false;
        ratt.endpoint.reliabilityKind = RELIABLE;
        ratt.endpoint.topicKind = WITH_KEY;
        ratt.endpoint.unicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.times.heartbeatResponseDelay.seconds = 0;
        ratt.times.heartbeatResponseDelay.fraction = 0;
        ratt.times.initialAcknackDelay.seconds = 0;
        ratt.times.initialAcknackDelay.fraction = 0;

        std::shared_ptr<RTPSReader::impl> pub_reader = participant_.create_reader(ratt, mp_PubReader.second,
                nullptr, c_EntityId_SEDPPubReader, true);

        if(pub_reader)
        {
            logInfo(RTPS_EDP,"SEDP Publication Reader created");
        }
        else
        {
            delete(mp_PubReader.second);
            mp_PubReader.second = nullptr;
        }

        mp_pubListen = new EDPSimplePUBListener(*this);
        mp_PubReader.first = new EDPStatefulReader(pub_reader, mp_pubListen);

        hatt.initialReservedCaches = 100;
        hatt.maximumReservedCaches = 5000;
        hatt.payloadMaxSize = DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;
        mp_SubWriter.second = new WriterHistory::impl(hatt);
        //Wparam.pushMode = true;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.topicKind = WITH_KEY;
        watt.endpoint.unicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdpsimple_.getLocalParticipantProxyData()->m_metatrafficMulticastLocatorList;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        watt.times.nackResponseDelay.seconds = 0;
        watt.times.nackResponseDelay.fraction = 0;
        watt.times.initialHeartbeatDelay.seconds = 0;
        watt.times.initialHeartbeatDelay.fraction = 0;
        if(participant_.getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
                participant_.getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
            watt.mode = ASYNCHRONOUS_WRITER;

        mp_SubWriter.first = participant_.create_writer(watt, *mp_SubWriter.second, nullptr,
                c_EntityId_SEDPSubWriter, true);

        if(mp_SubWriter.first)
        {
            logInfo(RTPS_EDP,"SEDP Subscription Writer created");
        }
        else
        {
            delete(mp_SubWriter.second);
            mp_SubWriter.second = nullptr;
        }
    }
    logInfo(RTPS_EDP,"Creation finished");
    return created;
}


bool EDPSimple::processLocalReaderProxyData(ReaderProxyData* rdata)
{
    logInfo(RTPS_EDP,rdata->guid().entityId);
    if(mp_SubWriter.first !=nullptr)
    {
        // TODO(Ricardo) Write a getCdrSerializedPayload for ReaderProxyData.
        CacheChange_ptr change = mp_SubWriter.first->new_change([]() ->
                uint32_t {return DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;}, ALIVE,rdata->key());

        if(change)
        {
            rdata->toParameterList();

            CDRMessage_t aux_msg(0);
            aux_msg.wraps = true;
            aux_msg.buffer = change->serialized_payload.data;
            aux_msg.max_size = change->serialized_payload.max_size;

#if EPROSIMA_BIG_ENDIAN
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_BE;
            aux_msg.msg_endian = BIGEND;
#else
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_LE;
            aux_msg.msg_endian =  LITTLEEND;
#endif

            ParameterList_t parameter_list = rdata->toParameterList();
            ParameterList::writeParameterListToCDRMsg(&aux_msg, &parameter_list, true);
            change->serialized_payload.length = (uint16_t)aux_msg.length;

            {
                auto lock = mp_SubWriter.second->lock_for_transaction();
                for(auto ch = mp_SubWriter.second->begin_nts(); ch != mp_SubWriter.second->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        mp_SubWriter.second->remove_change_nts(ch->second->sequence_number);
                        break;
                    }
                }
            }

            //TODO (Ricardo) Can change in next get_internal_listener. Change.
            if(mp_subListen->get_internal_listener() != nullptr)
            {
                //TODO(Ricardo) Review this Api.
                //TODO(Ricardo) If change to take method, problem here.
                mp_subListen->get_internal_listener()->onNewCacheChangeAdded(*mp_SubReader.first, &*change);
            }

            mp_SubWriter.second->add_change(change);

            return true;
        }
        return false;
    }
    return true;
}
bool EDPSimple::processLocalWriterProxyData(WriterProxyData* wdata)
{
    logInfo(RTPS_EDP, wdata->guid().entityId);
    if(mp_PubWriter.first !=nullptr)
    {
        CacheChange_ptr change = mp_PubWriter.first->new_change([]() ->
                uint32_t {return DISCOVERY_PUBLICATION_DATA_MAX_SIZE;}, ALIVE, wdata->key());

        if(change)
        {
            wdata->toParameterList();

            CDRMessage_t aux_msg(0);
            aux_msg.wraps = true;
            aux_msg.buffer = change->serialized_payload.data;
            aux_msg.max_size = change->serialized_payload.max_size;

#if EPROSIMA_BIG_ENDIAN
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_BE;
            aux_msg.msg_endian = BIGEND;
#else
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_LE;
            aux_msg.msg_endian =  LITTLEEND;
#endif

            ParameterList_t parameter_list = wdata->toParameterList();
            ParameterList::writeParameterListToCDRMsg(&aux_msg, &parameter_list, true);
            change->serialized_payload.length = (uint16_t)aux_msg.length;

            {
                auto lock = mp_PubWriter.second->lock_for_transaction();
                for(auto ch = mp_PubWriter.second->begin_nts(); ch != mp_PubWriter.second->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        mp_PubWriter.second->remove_change_nts(ch->second->sequence_number);
                        break;
                    }
                }
            }

            if(mp_pubListen->get_internal_listener() != nullptr)
            {
                //TODO(Ricardo) Review this Api.
                mp_pubListen->get_internal_listener()->onNewCacheChangeAdded(*mp_PubReader.first, &*change);
            }

            mp_PubWriter.second->add_change(change);

            return true;
        }
        return false;
    }
    return true;
}

bool EDPSimple::removeLocalWriter(RTPSWriter::impl& writer)
{
    logInfo(RTPS_EDP, writer.getGuid().entityId);

    if(mp_PubWriter.first != nullptr)
    {
        InstanceHandle_t iH;
        iH = writer.getGuid();

        CacheChange_ptr change = mp_PubWriter.first->new_change([]() ->
                uint32_t {return DISCOVERY_PUBLICATION_DATA_MAX_SIZE;}, NOT_ALIVE_DISPOSED_UNREGISTERED,iH);

        if(change)
        {
            {
                auto lock = mp_PubWriter.second->lock_for_transaction();
                for(auto ch = mp_PubWriter.second->begin_nts(); ch != mp_PubWriter.second->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        mp_PubWriter.second->remove_change_nts(ch->second->sequence_number);
                        break;
                    }
                }

            }

            if(mp_pubListen->get_internal_listener() != nullptr)
            {
                //TODO(Ricardo) Review this Api.
                mp_pubListen->get_internal_listener()->onNewCacheChangeAdded(*mp_PubReader.first, &*change);
            }

            mp_PubWriter.second->add_change(change);
        }
    }

    return pdpsimple_.removeWriterProxyData(writer.getGuid());
}

bool EDPSimple::removeLocalReader(RTPSReader::impl& reader)
{
    logInfo(RTPS_EDP, reader.getGuid().entityId);

    if(mp_SubWriter.first != nullptr)
    {
        InstanceHandle_t iH;
        iH = (reader.getGuid());
        CacheChange_ptr change = mp_SubWriter.first->new_change([]() ->
                uint32_t {return DISCOVERY_SUBSCRIPTION_DATA_MAX_SIZE;}, NOT_ALIVE_DISPOSED_UNREGISTERED,iH);

        if(change)
        {
            {
                auto lock = mp_SubWriter.second->lock_for_transaction();
                // TODO inset logic in WriterHistory.
                for(auto ch = mp_SubWriter.second->begin_nts(); ch != mp_SubWriter.second->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        mp_SubWriter.second->remove_change_nts(ch->second->sequence_number);
                        break;
                    }
                }
            }

            if(mp_subListen->get_internal_listener() != nullptr)
            {
                //TODO(Ricardo) Review this Api.
                mp_subListen->get_internal_listener()->onNewCacheChangeAdded(*mp_SubReader.first, &*change);
            }

            mp_SubWriter.second->add_change(change);
        }
    }

    return pdpsimple_.removeReaderProxyData(reader.getGuid());
}



void EDPSimple::assignRemoteEndpoints(const ParticipantProxyData& pdata)
{
    logInfo(RTPS_EDP,"New DPD received, adding remote endpoints to our SimpleEDP endpoints");
    uint32_t endp = pdata.m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_PubReader.first!=nullptr) //Exist Pub Writer and i have pub reader
    {
        logInfo(RTPS_EDP,"Adding SEDP Pub Writer to my Pub Reader");
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata.m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SEDPPubWriter;
        watt.endpoint.unicastLocatorList = pdata.m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata.m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        mp_PubReader.first->matched_writer_add(watt);
    }
    auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_PubWriter.first!=nullptr) //Exist Pub Detector
    {
        logInfo(RTPS_EDP,"Adding SEDP Pub Reader to my Pub Writer");
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata.m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SEDPPubReader;
        ratt.endpoint.unicastLocatorList = pdata.m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata.m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.endpoint.reliabilityKind = RELIABLE;
        mp_PubWriter.first->matched_reader_add(ratt);
    }
    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_SubReader.first!=nullptr) //Exist Pub Announcer
    {
        logInfo(RTPS_EDP,"Adding SEDP Sub Writer to my Sub Reader");
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata.m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SEDPSubWriter;
        watt.endpoint.unicastLocatorList = pdata.m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata.m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        mp_SubReader.first->matched_writer_add(watt);
    }
    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_SubWriter.first != nullptr) //Exist Pub Announcer
    {
        logInfo(RTPS_EDP,"Adding SEDP Sub Reader to my Sub Writer");
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata.m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SEDPSubReader;
        ratt.endpoint.unicastLocatorList = pdata.m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata.m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.endpoint.reliabilityKind = RELIABLE;
        mp_SubWriter.first->matched_reader_add(ratt);
    }
}


void EDPSimple::removeRemoteEndpoints(ParticipantProxyData* pdata)
{
    logInfo(RTPS_EDP,"For RTPSParticipant: "<<pdata->m_guid);

    uint32_t endp = pdata->m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_PubReader.first!=nullptr) //Exist Pub Writer and i have pub reader
    {
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SEDPPubWriter;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        mp_PubReader.first->matched_writer_remove(watt);
    }
    auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_PubWriter.first!=nullptr) //Exist Pub Detector
    {
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SEDPPubReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.endpoint.reliabilityKind = RELIABLE;
        mp_PubWriter.first->matched_reader_remove(ratt);
    }
    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_SubReader.first!=nullptr) //Exist Pub Announcer
    {
        logInfo(RTPS_EDP,"Adding SEDP Sub Writer to my Sub Reader");
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SEDPSubWriter;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = RELIABLE;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        mp_SubReader.first->matched_writer_remove(watt);
    }
    auxendp = endp;
    auxendp &= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;
    //FIXME: FIX TO NOT FAIL WITH BAD BUILTIN ENDPOINT SET
    //auxendp = 1;
    if(auxendp!=0 && mp_SubWriter.first!=nullptr) //Exist Pub Announcer
    {
        logInfo(RTPS_EDP,"Adding SEDP Sub Reader to my Sub Writer");
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SEDPSubReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        ratt.endpoint.reliabilityKind = RELIABLE;
        mp_SubWriter.first->matched_reader_remove(ratt);
    }
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
