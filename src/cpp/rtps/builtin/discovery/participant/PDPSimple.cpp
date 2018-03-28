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
 * @file PDPSimple.cpp
 *
 */

#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include "PDPSimpleListener.h"
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <fastrtps/rtps/builtin/liveliness/WLP.h>
#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>
#include <fastrtps/rtps/builtin/discovery/participant/timedevent/RemoteParticipantLeaseDuration.h>
#include <fastrtps/rtps/builtin/data/ReaderProxyData.h>
#include <fastrtps/rtps/builtin/data/WriterProxyData.h>
#include <fastrtps/rtps/builtin/discovery/endpoint/EDPSimple.h>
#include <fastrtps/rtps/builtin/discovery/endpoint/EDPStatic.h>
#include <fastrtps/rtps/resources/AsyncWriterThread.h>
#include <fastrtps/rtps/builtin/discovery/participant/timedevent/ResendParticipantProxyDataPeriod.h>
#include "../../../participant/RTPSParticipantImpl.h"
#include "../../../reader/StatefulReaderImpl.h"
#include "../../../writer/StatelessWriterImpl.h"
#include <fastrtps/rtps/reader/WriterProxy.h>
#include "../../../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/history/CacheChangePool.h>
#include <fastrtps/utils/TimeConversion.h>
#include <fastrtps/log/Log.h>

#include <mutex>

using namespace eprosima::fastrtps::rtps;

PDPSimple::PDPSimple(BuiltinProtocols* built, RTPSParticipant::impl& participant) :
    mp_builtin(built),
    participant_(participant),
    mp_WLP(nullptr),
    mp_EDP(nullptr),
    m_hasChangedLocalPDP(true),
    mp_resendParticipantTimer(nullptr),
    mp_listener(nullptr),
    spdp_writer_history_(nullptr),
    spdp_reader_history_(nullptr),
    mp_mutex(new std::recursive_mutex())
    {

    }

PDPSimple::~PDPSimple()
{
    if(mp_resendParticipantTimer != nullptr)
        delete(mp_resendParticipantTimer);

    participant_.remove_writer(spdp_writer_);
    participant_.remove_reader(spdp_reader_);
    delete(spdp_writer_history_);
    delete(spdp_reader_history_);

    if(mp_EDP!=nullptr)
        delete(mp_EDP);

    if(mp_WLP != nullptr)
    {
        delete(mp_WLP);
        mp_WLP = nullptr;
    }

    delete(mp_listener);
    for(auto it = this->m_participantProxies.begin();
            it!=this->m_participantProxies.end();++it)
    {
        delete(*it);
    }

    delete(mp_mutex);
}

void PDPSimple::initializeParticipantProxyData(ParticipantProxyData* participant_data)
{
    participant_data->m_leaseDuration = participant_.getAttributes().builtin.leaseDuration;
    set_VendorId_eProsima(participant_data->m_VendorId);

    participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
    participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;
    if(participant_.getAttributes().builtin.use_WriterLivelinessProtocol)
    {
        participant_data->m_availableBuiltinEndpoints |= BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
        participant_data->m_availableBuiltinEndpoints |= BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER;
    }
    if(participant_.getAttributes().builtin.use_SIMPLE_EndpointDiscoveryProtocol)
    {
        if(participant_.getAttributes().builtin.m_simpleEDP.use_PublicationWriterANDSubscriptionReader)
        {
            participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
            participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;
        }
        if(participant_.getAttributes().builtin.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter)
        {
            participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;
            participant_data->m_availableBuiltinEndpoints |= DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
        }
    }

#if HAVE_SECURITY
    participant_data->m_availableBuiltinEndpoints |= participant_.security_manager().builtin_endpoints();
#endif

    participant_data->m_defaultUnicastLocatorList = participant_.getAttributes().defaultUnicastLocatorList;
    participant_data->m_defaultMulticastLocatorList = participant_.getAttributes().defaultMulticastLocatorList;
    participant_data->m_expectsInlineQos = false;
    participant_data->m_guid = participant_.guid();
    for(uint8_t i = 0; i<16; ++i)
    {
        if(i<12)
            participant_data->m_key.value[i] = participant_data->m_guid.guidPrefix.value[i];
        else
            participant_data->m_key.value[i] = participant_data->m_guid.entityId.value[i - 12];
    }


    participant_data->m_metatrafficMulticastLocatorList = this->mp_builtin->m_metatrafficMulticastLocatorList;
    participant_data->m_metatrafficUnicastLocatorList = this->mp_builtin->m_metatrafficUnicastLocatorList;

    participant_data->m_participantName = std::string(participant_.getAttributes().getName());

    participant_data->m_userData = participant_.getAttributes().userData;

#if HAVE_SECURITY
    IdentityToken* identity_token = nullptr;
    if(participant_.security_manager().get_identity_token(&identity_token) && identity_token != nullptr)
    {
        participant_data->identity_token_ = std::move(*identity_token);
        participant_.security_manager().return_identity_token(identity_token);
    }
#endif
}

bool PDPSimple::init()
{
    logInfo(RTPS_PDP,"Beginning");
    m_discovery = participant_.getAttributes().builtin;
    //CREATE ENDPOINTS
    if(!createSPDPEndpoints())
        return false;
    //UPDATE METATRAFFIC.
    mp_builtin->updateMetatrafficLocators(spdp_reader_->getAttributes()->unicastLocatorList);
    m_participantProxies.push_back(new ParticipantProxyData());
    initializeParticipantProxyData(m_participantProxies.front());

    if(mp_builtin->m_att.use_WriterLivelinessProtocol)
    {
        mp_WLP = new WLP(this, participant_); //TODO(Ricardo) Change
        mp_WLP->init();
    }

    //INIT EDP
    if(m_discovery.use_STATIC_EndpointDiscoveryProtocol)
    {
        mp_EDP = (EDP*)(new EDPStatic(*this, participant_));
        if(!mp_EDP->initEDP(m_discovery)){
            logError(RTPS_PDP,"Endpoint discovery configuration failed");
            return false;
        }

    }
    else if(m_discovery.use_SIMPLE_EndpointDiscoveryProtocol)
    {
        mp_EDP = (EDP*)(new EDPSimple(*this, participant_));
        if(!mp_EDP->initEDP(m_discovery)){
            logError(RTPS_PDP,"Endpoint discovery configuration failed");
            return false;
        }
    }
    else
    {
        logWarning(RTPS_PDP,"No EndpointDiscoveryProtocol defined");
        return false;
    }

    if(!participant_.enable_reader(*spdp_reader_))
    {
        return false;
    }

    mp_resendParticipantTimer = new ResendParticipantProxyDataPeriod(*this,
            TimeConv::Time_t2MilliSecondsDouble(m_discovery.leaseDuration_announcementperiod));

    return true;
}

void PDPSimple::stopParticipantAnnouncement()
{
    mp_resendParticipantTimer->cancel_timer();
}

void PDPSimple::resetParticipantAnnouncement()
{
    mp_resendParticipantTimer->restart_timer();
}

void PDPSimple::announceParticipantState(bool new_change, bool dispose)
{
    logInfo(RTPS_PDP,"Announcing RTPSParticipant State (new change: "<< new_change <<")");

    if(!dispose)
    {
        if(new_change || m_hasChangedLocalPDP)
        {
            this->mp_mutex->lock();
            ParticipantProxyData* local_participant_data = getLocalParticipantProxyData();
            local_participant_data->m_manualLivelinessCount++;
            InstanceHandle_t key = local_participant_data->m_key;
            ParameterList_t parameter_list = local_participant_data->AllQostoParameterList();
            this->mp_mutex->unlock();

            // TODO Reuse change returned by this function
            spdp_writer_history_->remove_min_change();
            // TODO(Ricardo) Change DISCOVERY_PARTICIPANT_DATA_MAX_SIZE with getLocalParticipantProxyData()->size().
            CacheChange_ptr change = spdp_writer_->new_change(ALIVE, key);

            if(change)
            {
                CDRMessage_t aux_msg(0);
                aux_msg.wraps = true;
                aux_msg.buffer = change->serialized_payload.data;
                aux_msg.max_size = change->serialized_payload.max_size;

#if EPROSIMA_BIG_ENDIAN
                change->serializedPayload.encapsulation = (uint16_t)PL_CDR_BE;
                aux_msg.msg_endian = BIGEND;
#else
                change->serialized_payload.encapsulation = (uint16_t)PL_CDR_LE;
                aux_msg.msg_endian =  LITTLEEND;
#endif

                if(ParameterList::writeParameterListToCDRMsg(&aux_msg, &parameter_list, true))
                {
                    change->serialized_payload.length = (uint16_t)aux_msg.length;

                    //TODO (Ricardo) Messages error in add_changes used on discovery.
                    spdp_writer_history_->add_change(change);
                }
                else
                {
                    logError(RTPS_PDP, "Cannot serialize ParticipantProxyData.");
                }
            }

            m_hasChangedLocalPDP = false;
        }
        else
        {
            //TODO(Ricardo) Review this function
            spdp_writerless_->resent_changes();
        }
    }
    else
    {
        this->mp_mutex->lock();
        ParameterList_t parameter_list = getLocalParticipantProxyData()->AllQostoParameterList();
        this->mp_mutex->unlock();

        // TODO Reuse change returned by this function
        spdp_writer_history_->remove_min_change();
        CacheChange_ptr change = spdp_writer_->new_change(NOT_ALIVE_DISPOSED_UNREGISTERED,
                getLocalParticipantProxyData()->m_key);

        if(change)
        {
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

            if(ParameterList::writeParameterListToCDRMsg(&aux_msg, &parameter_list, true))
            {
                change->serialized_payload.length = (uint16_t)aux_msg.length;

                spdp_writer_history_->add_change(change);
            }
            else
            {
                logError(RTPS_PDP, "Cannot serialize ParticipantProxyData.");
            }
        }
    }

}

bool PDPSimple::lookupReaderProxyData(const GUID_t& reader, ReaderProxyData& rdata, ParticipantProxyData& pdata)
{
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);
    for (auto pit = m_participantProxies.begin();
            pit != m_participantProxies.end();++pit)
    {
        for (auto rit = (*pit)->m_readers.begin();
                rit != (*pit)->m_readers.end();++rit)
        {
            if((*rit)->guid() == reader)
            {
                rdata.copy(*rit);
                pdata.copy(**pit);
                return true;
            }
        }
    }
    return false;
}

bool PDPSimple::lookupWriterProxyData(const GUID_t& writer, WriterProxyData& wdata, ParticipantProxyData& pdata)
{
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);
    for (auto pit = m_participantProxies.begin();
            pit != m_participantProxies.end(); ++pit)
    {
        for (auto wit = (*pit)->m_writers.begin();
                wit != (*pit)->m_writers.end(); ++wit)
        {
            if((*wit)->guid() == writer)
            {
                wdata.copy(*wit);
                pdata.copy(**pit);
                return true;
            }
        }
    }
    return false;
}

bool PDPSimple::removeReaderProxyData(const GUID_t& spdp_reader_guid)
{
    logInfo(RTPS_PDP, "Removing reader proxy data " << spdp_reader_guid);
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);

    for (auto pit = m_participantProxies.begin();
            pit != m_participantProxies.end(); ++pit)
    {
        for (auto rit = (*pit)->m_readers.begin();
                rit != (*pit)->m_readers.end(); ++rit)
        {
            if((*rit)->guid() == spdp_reader_guid)
            {
                mp_EDP->unpairReaderProxy((*pit)->m_guid, spdp_reader_guid);
                delete *rit;
                (*pit)->m_readers.erase(rit);
                return true;
            }
        }
    }

    return false;
}

bool PDPSimple::removeWriterProxyData(const GUID_t& spdp_writer_guid)
{
    logInfo(RTPS_PDP, "Removing writer proxy data " << spdp_writer_guid);
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);

    for (auto pit = m_participantProxies.begin();
            pit != m_participantProxies.end(); ++pit)
    {
        for (auto wit = (*pit)->m_writers.begin();
                wit != (*pit)->m_writers.end(); ++wit)
        {
            if((*wit)->guid() == spdp_writer_guid)
            {
                mp_EDP->unpairWriterProxy((*pit)->m_guid, spdp_writer_guid);
                delete *wit;
                (*pit)->m_writers.erase(wit);
                return true;
            }
        }
    }

    return false;
}


bool PDPSimple::lookupParticipantProxyData(const GUID_t& pguid, ParticipantProxyData& pdata)
{
    logInfo(RTPS_PDP,pguid);
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);
    for(std::vector<ParticipantProxyData*>::iterator pit = m_participantProxies.begin();
            pit!=m_participantProxies.end();++pit)
    {
        if((*pit)->m_guid == pguid)
        {
            pdata.copy(**pit);
            return true;
        }
    }
    return false;
}

bool PDPSimple::createSPDPEndpoints()
{
    logInfo(RTPS_PDP,"Beginning");
    //SPDP BUILTIN RTPSParticipant WRITER
    HistoryAttributes hatt;
    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = 20;
    hatt.maximumReservedCaches = 100;
    spdp_writer_history_ = new WriterHistory::impl(hatt);
    WriterAttributes watt;
    watt.endpoint.endpointKind = WRITER;
    watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
    watt.endpoint.reliabilityKind = BEST_EFFORT;
    watt.endpoint.topicKind = WITH_KEY;
    if(participant_.getRTPSParticipantAttributes().throughputController.bytesPerPeriod != UINT32_MAX &&
            participant_.getRTPSParticipantAttributes().throughputController.periodMillisecs != 0)
        watt.mode = ASYNCHRONOUS_WRITER;
    spdp_writer_ = participant_.create_writer(watt, *spdp_writer_history_, nullptr, c_EntityId_SPDPWriter, true);

    if(spdp_writer_)
    {
#if HAVE_SECURITY
        participant_.set_endpoint_rtps_protection_supports(spdp_writer_.get(), false);
#endif

        spdp_writerless_ = dynamic_cast<StatelessWriter::impl*>(spdp_writer_.get());

        for(LocatorListIterator lit = mp_builtin->m_initialPeersList.begin();
                lit != mp_builtin->m_initialPeersList.end(); ++lit)
        {
            spdp_writerless_->add_locator(*lit);
        }
    }
    else
    {
        logError(RTPS_PDP,"SimplePDP Writer creation failed");
        delete(spdp_writer_history_);
        spdp_writer_history_ = nullptr;
        return false;
    }

    hatt.payloadMaxSize = DISCOVERY_PARTICIPANT_DATA_MAX_SIZE;
    hatt.initialReservedCaches = 250;
    hatt.maximumReservedCaches = 5000;
    spdp_reader_history_ = new ReaderHistory(hatt);
    ReaderAttributes ratt;
    ratt.endpoint.multicastLocatorList = mp_builtin->m_metatrafficMulticastLocatorList;
    ratt.endpoint.unicastLocatorList = mp_builtin->m_metatrafficUnicastLocatorList;
    ratt.endpoint.topicKind = WITH_KEY;
    ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
    ratt.endpoint.reliabilityKind = BEST_EFFORT;
    mp_listener = new PDPSimpleListener(*this);

    spdp_reader_ = participant_.create_reader(ratt, spdp_reader_history_, mp_listener, c_EntityId_SPDPReader, true, false);

    if(spdp_reader_)
    {
#if HAVE_SECURITY
        participant_.set_endpoint_rtps_protection_supports(spdp_reader_.get(), false);
#endif
    }
    else
    {
        logError(RTPS_PDP,"SimplePDP Reader creation failed");
        delete(spdp_reader_history_);
        spdp_reader_history_ = nullptr;
        delete(mp_listener);
        mp_listener = nullptr;
        return false;
    }

    logInfo(RTPS_PDP,"SPDP Endpoints creation finished");
    return true;
}

bool PDPSimple::addReaderProxyData(ReaderProxyData* rdata, ParticipantProxyData& pdata)
{
    logInfo(RTPS_PDP, "Adding reader proxy data " << rdata->guid());

    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);

    for(std::vector<ParticipantProxyData*>::iterator pit = m_participantProxies.begin();
            pit!=m_participantProxies.end();++pit)
    {
        if((*pit)->m_guid.guidPrefix == rdata->guid().guidPrefix)
        {
            // Set locators information if not defined by ReaderProxyData.
            if(rdata->unicastLocatorList().empty() && rdata->multicastLocatorList().empty())
            {
                rdata->unicastLocatorList((*pit)->m_defaultUnicastLocatorList);
                rdata->multicastLocatorList((*pit)->m_defaultMulticastLocatorList);
            }
            // Set as alive.
            rdata->isAlive(true);

            // Copy participant data to be used outside.
            pdata.copy(**pit);

            // Check that it is not already there:
            for(std::vector<ReaderProxyData*>::iterator rit = (*pit)->m_readers.begin();
                    rit!=(*pit)->m_readers.end();++rit)
            {
                if((*rit)->guid().entityId == rdata->guid().entityId)
                {
                    (*rit)->update(rdata);
                    return true;
                }
            }

            ReaderProxyData* newRPD = new ReaderProxyData(*rdata);
            (*pit)->m_readers.push_back(newRPD);
            return true;
        }
    }

    return false;
}

bool PDPSimple::addWriterProxyData(WriterProxyData* wdata, ParticipantProxyData& pdata)
{
    logInfo(RTPS_PDP, "Adding writer proxy data " << wdata->guid());

    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);

    for(std::vector<ParticipantProxyData*>::iterator pit = m_participantProxies.begin();
            pit!=m_participantProxies.end();++pit)
    {
        if((*pit)->m_guid.guidPrefix == wdata->guid().guidPrefix)
        {
            // Set locators information if not defined by ReaderProxyData.
            if(wdata->unicastLocatorList().empty() && wdata->multicastLocatorList().empty())
            {
                wdata->unicastLocatorList((*pit)->m_defaultUnicastLocatorList);
                wdata->multicastLocatorList((*pit)->m_defaultMulticastLocatorList);
            }
            // Set as alive.
            wdata->isAlive(true);

            // Copy participant data to be used outside.
            pdata.copy(**pit);

            //CHECK THAT IT IS NOT ALREADY THERE:
            for(std::vector<WriterProxyData*>::iterator wit = (*pit)->m_writers.begin();
                    wit!=(*pit)->m_writers.end();++wit)
            {
                if((*wit)->guid().entityId == wdata->guid().entityId)
                {
                    (*wit)->update(wdata);
                    return true;
                }
            }

            WriterProxyData* newWPD = new WriterProxyData(*wdata);
            (*pit)->m_writers.push_back(newWPD);
            return true;
        }
    }
    return false;
}

void PDPSimple::assignRemoteEndpoints(ParticipantProxyData* pdata)
{
    logInfo(RTPS_PDP,"For RTPSParticipant: "<<pdata->m_guid.guidPrefix);
    uint32_t endp = pdata->m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
    if(auxendp!=0)
    {
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SPDPWriter;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = BEST_EFFORT;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        spdp_reader_->matched_writer_add(watt);
    }
    auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;
    if(auxendp!=0)
    {
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SPDPReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.reliabilityKind = BEST_EFFORT;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        spdp_writer_->matched_reader_add(ratt);
    }

#if HAVE_SECURITY
    // Validate remote participant
    participant_.security_manager().discovered_participant(*pdata);
#else
    //Inform EDP of new RTPSParticipant data:
    notifyAboveRemoteEndpoints(*pdata);
#endif
}

void PDPSimple::notifyAboveRemoteEndpoints(const GUID_t& participant_guid)
{
    ParticipantProxyData participant_data;
    bool found_participant = false;

    this->mp_mutex->lock();
    for(std::vector<ParticipantProxyData*>::iterator pit = m_participantProxies.begin();
            pit != m_participantProxies.end(); ++pit)
    {
        if((*pit)->m_guid == participant_guid)
        {
            participant_data.copy(**pit);
            found_participant = true;
            break;
        }
    }
    this->mp_mutex->unlock();

    if(found_participant)
    {
        notifyAboveRemoteEndpoints(participant_data);
    }
}

void PDPSimple::notifyAboveRemoteEndpoints(const ParticipantProxyData& pdata)
{
    //Inform EDP of new RTPSParticipant data:
    if(mp_EDP != nullptr)
    {
        mp_EDP->assignRemoteEndpoints(pdata);
    }

    //TODO(Ricardo) This should be outside pdpsimple.
    if(mp_WLP != nullptr)
    {
        mp_WLP->assignRemoteEndpoints(pdata);
    }
}


void PDPSimple::removeRemoteEndpoints(ParticipantProxyData* pdata)
{
    logInfo(RTPS_PDP,"For RTPSParticipant: "<<pdata->m_guid);
    uint32_t endp = pdata->m_availableBuiltinEndpoints;
    uint32_t auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
    if(auxendp!=0)
    {
        RemoteWriterAttributes watt;
        watt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        watt.guid.entityId = c_EntityId_SPDPWriter;
        watt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        watt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        watt.endpoint.reliabilityKind = BEST_EFFORT;
        watt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        spdp_reader_->matched_writer_remove(watt);
    }
    auxendp = endp;
    auxendp &=DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;
    if(auxendp!=0)
    {
        RemoteReaderAttributes ratt;
        ratt.expectsInlineQos = false;
        ratt.guid.guidPrefix = pdata->m_guid.guidPrefix;
        ratt.guid.entityId = c_EntityId_SPDPReader;
        ratt.endpoint.unicastLocatorList = pdata->m_metatrafficUnicastLocatorList;
        ratt.endpoint.multicastLocatorList = pdata->m_metatrafficMulticastLocatorList;
        ratt.endpoint.reliabilityKind = BEST_EFFORT;
        ratt.endpoint.durabilityKind = TRANSIENT_LOCAL;
        spdp_writer_->matched_reader_remove(ratt);
    }
}

bool PDPSimple::removeRemoteParticipant(GUID_t& partGUID)
{
    logInfo(RTPS_PDP,partGUID );
    ParticipantProxyData* pdata = nullptr;

    //Remove it from our vector or RTPSParticipantProxies:
    this->mp_mutex->lock();
    for(std::vector<ParticipantProxyData*>::iterator pit = m_participantProxies.begin();
            pit!=m_participantProxies.end();++pit)
    {
        if((*pit)->m_guid == partGUID)
        {
            pdata = *pit;
            m_participantProxies.erase(pit);
            break;
        }
    }
    this->mp_mutex->unlock();

    if(pdata !=nullptr)
    {
        if(mp_EDP!=nullptr)
        {
            for(std::vector<ReaderProxyData*>::iterator rit = pdata->m_readers.begin();
                    rit != pdata->m_readers.end();++rit)
            {
                mp_EDP->unpairReaderProxy(partGUID, (*rit)->guid());
            }
            for(std::vector<WriterProxyData*>::iterator wit = pdata->m_writers.begin();
                    wit !=pdata->m_writers.end();++wit)
            {
                mp_EDP->unpairWriterProxy(partGUID, (*wit)->guid());
            }
        }

#if HAVE_SECURITY
        participant_.security_manager().remove_participant(*pdata);
#endif

        if(mp_WLP != nullptr)
        {
            mp_WLP->removeRemoteEndpoints(pdata);
        }
        this->mp_EDP->removeRemoteEndpoints(pdata);
        this->removeRemoteEndpoints(pdata);

        spdp_reader_history_->getMutex()->lock();
        for(auto it = spdp_reader_history_->changesBegin(); it != spdp_reader_history_->changesEnd(); ++it)
        {
            if((*it)->instance_handle == pdata->m_key)
            {
                spdp_reader_history_->remove_change(&**it);
                break;
            }
        }
        spdp_reader_history_->getMutex()->unlock();

        delete(pdata);
        return true;
    }

    return false;
}

void PDPSimple::assertRemoteParticipantLiveliness(const GuidPrefix_t& guidP)
{
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);
    for(std::vector<ParticipantProxyData*>::iterator it = this->m_participantProxies.begin();
            it!=this->m_participantProxies.end();++it)
    {
        if((*it)->m_guid.guidPrefix == guidP)
        {
            logInfo(RTPS_LIVELINESS,"RTPSParticipant "<< (*it)->m_guid << " is Alive");
            // TODO Ricardo: Study if isAlive attribute is necessary.
            (*it)->isAlive = true;
            if((*it)->mp_leaseDurationTimer != nullptr)
            {
                (*it)->mp_leaseDurationTimer->cancel_timer();
                (*it)->mp_leaseDurationTimer->restart_timer();
            }
            break;
        }
    }
}

void PDPSimple::assertLocalWritersLiveliness(LivelinessQosPolicyKind kind)
{
    logInfo(RTPS_LIVELINESS,"of type " << (kind==AUTOMATIC_LIVELINESS_QOS?"AUTOMATIC":"")
            <<(kind==MANUAL_BY_PARTICIPANT_LIVELINESS_QOS?"MANUAL_BY_PARTICIPANT":""));
    std::lock_guard<std::recursive_mutex> guard(*this->mp_mutex);
    for(std::vector<WriterProxyData*>::iterator wit = this->m_participantProxies.front()->m_writers.begin();
            wit!=this->m_participantProxies.front()->m_writers.end();++wit)
    {
        if((*wit)->m_qos.m_liveliness.kind == kind)
        {
            logInfo(RTPS_LIVELINESS,"Local Writer "<< (*wit)->guid().entityId << " marked as ALIVE");
            (*wit)->isAlive(true);
        }
    }
}

void PDPSimple::assertRemoteWritersLiveliness(GuidPrefix_t& guidP,LivelinessQosPolicyKind kind)
{
    std::lock_guard<std::recursive_mutex> guardP(*participant_.getParticipantMutex());
    std::lock_guard<std::recursive_mutex> pguard(*this->mp_mutex);
    logInfo(RTPS_LIVELINESS,"of type " << (kind==AUTOMATIC_LIVELINESS_QOS?"AUTOMATIC":"")
            <<(kind==MANUAL_BY_PARTICIPANT_LIVELINESS_QOS?"MANUAL_BY_PARTICIPANT":""));

    for(std::vector<ParticipantProxyData*>::iterator pit=this->m_participantProxies.begin();
            pit!=this->m_participantProxies.end();++pit)
    {
        if((*pit)->m_guid.guidPrefix == guidP)
        {
            for(std::vector<WriterProxyData*>::iterator wit = (*pit)->m_writers.begin();
                    wit != (*pit)->m_writers.end();++wit)
            {
                if((*wit)->m_qos.m_liveliness.kind == kind)
                {
                    (*wit)->isAlive(true);
                    for(auto rit = participant_.userReadersListBegin(); rit != participant_.userReadersListEnd(); ++rit)
                    {
                        if((*rit)->getAttributes()->reliabilityKind == RELIABLE)
                        {
                            StatefulReader::impl* sfr = dynamic_cast<StatefulReader::impl*>(*rit);
                            WriterProxy* WP;
                            if(sfr->matched_writer_lookup((*wit)->guid(), &WP))
                            {
                                WP->assertLiveliness();
                                continue;
                            }
                        }
                    }
                }
            }
            break;
        }
    }
}

bool PDPSimple::newRemoteEndpointStaticallyDiscovered(const GUID_t& pguid, int16_t userDefinedId,EndpointKind_t kind)
{
    ParticipantProxyData pdata;
    if(lookupParticipantProxyData(pguid, pdata))
    {
        if(kind == WRITER)
        {
            dynamic_cast<EDPStatic*>(mp_EDP)->newRemoteWriter(pdata,userDefinedId);
        }
        else
        {
            dynamic_cast<EDPStatic*>(mp_EDP)->newRemoteReader(pdata,userDefinedId);
        }
    }
    return false;
}

CDRMessage_t PDPSimple::get_participant_proxy_data_serialized(Endianness_t endian)
{
    std::lock_guard<std::recursive_mutex> guardPDP(*this->mp_mutex);
    CDRMessage_t cdr_msg;
    cdr_msg.msg_endian = endian;

    ParameterList_t parameter_list = getLocalParticipantProxyData()->AllQostoParameterList();
    if(!ParameterList::writeParameterListToCDRMsg(&cdr_msg, &parameter_list, true))
    {
        cdr_msg.pos = 0;
        cdr_msg.length = 0;
    }

    return cdr_msg;
}
