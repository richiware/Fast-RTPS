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
 * @file RTPSParticipant.cpp
 *
 */

#include "RTPSParticipantImpl.h"
#include "../flowcontrol/ThroughputController.h"
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/resources/AsyncWriterThread.h>
#include <fastrtps/rtps/messages/MessageReceiver.h>
#include "../history/WriterHistoryImpl.h"
#include "../writer/StatelessWriterImpl.h"
#include "../writer/StatefulWriterImpl.h"
#include "../reader/StatelessReaderImpl.h"
#include "../reader/StatefulReaderImpl.h"
#include <fastrtps/rtps/reader/StatefulReader.h>
#include <fastrtps/transport/UDPv4Transport.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>
#include <fastrtps/utils/IPFinder.h>
#include <fastrtps/utils/eClock.h>
#include <fastrtps/utils/Semaphore.h>
#include <fastrtps/log/Log.h>

#include <mutex>
#include <algorithm>

using namespace eprosima::fastrtps::rtps;

uint32_t RTPSParticipant::impl::m_maxRTPSParticipantID = 0;

static EntityId_t TrustedWriter(const EntityId_t& reader)
{
    if(reader == c_EntityId_SPDPReader) return c_EntityId_SPDPWriter;
    if(reader == c_EntityId_SEDPPubReader) return c_EntityId_SEDPPubWriter;
    if(reader == c_EntityId_SEDPSubReader) return c_EntityId_SEDPSubWriter;
    if(reader == c_EntityId_ReaderLiveliness) return c_EntityId_WriterLiveliness;

    return c_EntityId_Unknown;
}

Locator_t RTPSParticipant::impl::applyLocatorAdaptRule(Locator_t loc)
{
    switch (loc.kind){
        case LOCATOR_KIND_UDPv4:
            //This is a completely made up rule
            loc.port += att_.port.participantIDGain;
            break;
        case LOCATOR_KIND_UDPv6:
            //TODO - Define the rest of rules
            loc.port += att_.port.participantIDGain;
            break;
    }
    return loc;
}

//TODO(Ricardo) Remove GUID. Better be inside attributes.
//TODO(Ricardo) ParticipantID is calculate by networkmanager trying to open ports.
//TODO(Ricardo) InstanceID is diferent that ParticipantID.
RTPSParticipant::impl::impl(const RTPSParticipantAttributes& PParam,
        const GuidPrefix_t& guid, Listener* listener) : att_(PParam),
    event_thread_(nullptr),
    builtin_protocols_(nullptr),
    resources_semaphore_(new Semaphore(0)),
    id_counter_(0),
#if HAVE_SECURITY
    security_manager_(*this),
#endif
    listener_(listener),
    mutex_(new std::recursive_mutex())
#if HAVE_SECURITY
    , is_rtps_protected_(false)
#endif
{
    //TODO(Ricardo) Preconditions.

    if(att_.participantID < 0)
    {
        att_.participantID = ++m_maxRTPSParticipantID;
    }

    // Generate GUID if necessary.
    if(guid == GuidPrefix_t::unknown())
    {
        int pid;
#if defined(__cplusplus_winrt)
        pid = (int)GetCurrentProcessId();
#elif defined(_WIN32)
        pid = (int)_getpid();
#else
        pid = (int)getpid();
#endif

        LocatorList_t loc;
        //TODO(Ricardo) Review What happen if not network found.
        IPFinder::getIP4Address(&loc);
        if(loc.size()>0)
        {
            guid_.guidPrefix.value[0] = c_VendorId_eProsima[0];
            guid_.guidPrefix.value[1] = c_VendorId_eProsima[1];
            guid_.guidPrefix.value[2] = loc.begin()->address[14];
            guid_.guidPrefix.value[3] = loc.begin()->address[15];
        }
        else
        {
            guid_.guidPrefix.value[0] = c_VendorId_eProsima[0];
            guid_.guidPrefix.value[1] = c_VendorId_eProsima[1];
            guid_.guidPrefix.value[2] = 127;
            guid_.guidPrefix.value[3] = 1;
        }
        guid_.guidPrefix.value[4] = ((octet*)&pid)[0];
        guid_.guidPrefix.value[5] = ((octet*)&pid)[1];
        guid_.guidPrefix.value[6] = ((octet*)&pid)[2];
        guid_.guidPrefix.value[7] = ((octet*)&pid)[3];
        guid_.guidPrefix.value[8] = ((octet*)&att_.participantID)[0];
        guid_.guidPrefix.value[9] = ((octet*)&att_.participantID)[1];
        guid_.guidPrefix.value[10] = ((octet*)&att_.participantID)[2];
        guid_.guidPrefix.value[11] = ((octet*)&att_.participantID)[3];
    }
    else
    {
        guid_.guidPrefix = guid;
    }
    guid_.entityId = c_EntityId_RTPSParticipant;

#if HAVE_SECURITY
    // Read participant properties.
    const std::string* property_value = PropertyPolicyHelper::find_property(PParam.properties,
            "rtps.participant.rtps_protection_kind");
    if(property_value != nullptr && property_value->compare("ENCRYPT") == 0)
        is_rtps_protected_ = true;
#endif

    // Builtin transport by default
    if (PParam.useBuiltinTransports)
    {
        UDPv4TransportDescriptor descriptor;
        descriptor.sendBufferSize = att_.sendSocketBufferSize;
        descriptor.receiveBufferSize = att_.listenSocketBufferSize;
        network_factory_.RegisterTransport(&descriptor);
    }

    // User defined transports
    for (const auto& transportDescriptor : PParam.userTransports)
        network_factory_.RegisterTransport(transportDescriptor.get());

    Locator_t loc;
    loc.port = PParam.defaultSendPort;
    event_thread_ = new ResourceEvent(*this);
    event_thread_->init_thread();


    // Throughput controller, if the descriptor has valid values
    if (PParam.throughputController.bytesPerPeriod != UINT32_MAX &&
            PParam.throughputController.periodMillisecs != 0)
    {
        std::unique_ptr<FlowController> controller(new ThroughputController(PParam.throughputController, this));
        controllers_.push_back(std::move(controller));
    }

    /// Creation of metatraffic locator and receiver resources
    uint32_t metatraffic_multicast_port = att_.port.getMulticastPort(att_.builtin.domainId);
    uint32_t metatraffic_unicast_port = att_.port.getUnicastPort(att_.builtin.domainId, att_.participantID);

    /* If metatrafficMulticastLocatorList is empty, add mandatory default Locators
       Else -> Take them */

    /* INSERT DEFAULT MANDATORY MULTICAST LOCATORS HERE */

    if(att_.builtin.metatrafficMulticastLocatorList.empty() && att_.builtin.metatrafficUnicastLocatorList.empty())
    {
        //UDPv4
        Locator_t mandatoryMulticastLocator;
        mandatoryMulticastLocator.kind = LOCATOR_KIND_UDPv4;
        mandatoryMulticastLocator.port = metatraffic_multicast_port;
        mandatoryMulticastLocator.set_IP4_address(239,255,0,1);

        att_.builtin.metatrafficMulticastLocatorList.push_back(mandatoryMulticastLocator);

        Locator_t default_metatraffic_unicast_locator;
        default_metatraffic_unicast_locator.port = metatraffic_unicast_port;
        att_.builtin.metatrafficUnicastLocatorList.push_back(default_metatraffic_unicast_locator);

        network_factory_.NormalizeLocators(att_.builtin.metatrafficUnicastLocatorList);
    }
    else
    {
        std::for_each(att_.builtin.metatrafficMulticastLocatorList.begin(), att_.builtin.metatrafficMulticastLocatorList.end(),
                [&](Locator_t& locator) {
                    if(locator.port == 0)
                        locator.port = metatraffic_multicast_port;
                });
        network_factory_.NormalizeLocators(att_.builtin.metatrafficMulticastLocatorList);

        std::for_each(att_.builtin.metatrafficUnicastLocatorList.begin(), att_.builtin.metatrafficUnicastLocatorList.end(),
                [&](Locator_t& locator) {
                    if(locator.port == 0)
                        locator.port = metatraffic_unicast_port;
                });
        network_factory_.NormalizeLocators(att_.builtin.metatrafficUnicastLocatorList);
    }

    //Create ReceiverResources now and update the list with the REAL used ones
    createReceiverResources(att_.builtin.metatrafficMulticastLocatorList, true);

    //Create ReceiverResources now and update the list with the REAL used ones
    createReceiverResources(att_.builtin.metatrafficUnicastLocatorList, true);

    // Initial peers
    if(att_.builtin.initialPeersList.empty())
    {
        att_.builtin.initialPeersList = att_.builtin.metatrafficMulticastLocatorList;
    }
    else
    {
        LocatorList_t initial_peers;
        initial_peers.swap(att_.builtin.initialPeersList);

        std::for_each(initial_peers.begin(), initial_peers.end(),
                [&](Locator_t& locator) {
                    if(locator.port == 0)
                    {
                        // TODO(Ricardo) Make configurable.
                        for(int32_t i = 0; i < 4; ++i)
                        {
                            Locator_t auxloc(locator);
                            auxloc.port = att_.port.getUnicastPort(att_.builtin.domainId, i);

                            att_.builtin.initialPeersList.push_back(auxloc);
                        }
                    }
                    else
                        att_.builtin.initialPeersList.push_back(locator);
                });

        network_factory_.NormalizeLocators(att_.builtin.initialPeersList);
    }

    /// Creation of user locator and receiver resources

    bool hasLocatorsDefined = true;
    //If no default locators are defined we define some.
    /* The reasoning here is the following.
       If the parameters of the RTPS Participant don't hold default listening locators for the creation
       of Endpoints, we make some for Unicast only.
       If there is at least one listen locator of any kind, we do not create any default ones.
       If there are no sending locators defined, we create default ones for the transports we implement.
       */
    if(att_.defaultUnicastLocatorList.empty() && att_.defaultMulticastLocatorList.empty())
    {
        //Default Unicast Locators in case they have not been provided
        /* INSERT DEFAULT UNICAST LOCATORS FOR THE PARTICIPANT */
        hasLocatorsDefined = false;
        Locator_t loc2;

        LocatorList_t loclist;
        IPFinder::getIP4Address(&loclist);
        for(auto it = loclist.begin(); it != loclist.end(); ++it)
        {
            (*it).port = att_.port.portBase +
                att_.port.domainIDGain*PParam.builtin.domainId+
                att_.port.offsetd3+
                att_.port.participantIDGain * att_.participantID;
            (*it).kind = LOCATOR_KIND_UDPv4;

            att_.defaultUnicastLocatorList.push_back((*it));
        }
    }
    else
    {
        // Locator with port 0, calculate port.
        std::for_each(att_.defaultUnicastLocatorList.begin(),
                att_.defaultUnicastLocatorList.end(),
                [&](Locator_t& loc) {
                    if(loc.port == 0)
                        loc.port = att_.port.portBase +
                            att_.port.domainIDGain * PParam.builtin.domainId+
                            att_.port.offsetd3+
                            att_.port.participantIDGain * att_.participantID;
                });

        // Normalize unicast locators.
        network_factory_.NormalizeLocators(att_.defaultUnicastLocatorList);
    }

    /*
        Since nothing guarantees the correct creation of the Resources on the Locators we have specified, and
        in order to maintain synchrony between the defaultLocator list and the actuar ReceiveResources,
        We create the resources for these Locators now. Furthermore, in case these resources are taken,
        we create them on another Locator and then update de defaultList.
        */
    createReceiverResources(att_.defaultUnicastLocatorList, true);

    if(!hasLocatorsDefined){
        logInfo(RTPS_PARTICIPANT,att_.getName()<<" Created with NO default Unicast Locator List, adding Locators: "<<att_.defaultUnicastLocatorList);
    }
    //Multicast
    createReceiverResources(att_.defaultMulticastLocatorList, true);

    //Check if defaultOutLocatorsExist, create some if they don't
    hasLocatorsDefined = true;
    if (att_.defaultOutLocatorList.empty()){
        hasLocatorsDefined = false;
        Locator_t SendLocator;
        /*TODO - Fill with desired default Send Locators for our transports*/
        //Warning - Mock rule being used (and only for IPv4)!
        SendLocator.kind = LOCATOR_KIND_UDPv4;
        att_.defaultOutLocatorList.push_back(SendLocator);
    }
    //Create the default sendResources - For the same reason as in the ReceiverResources
    std::vector<SenderResource > newSenders;
    std::vector<SenderResource > newSendersBuffer;
    LocatorList_t defcopy = att_.defaultOutLocatorList;
    for (auto it = defcopy.begin(); it != defcopy.end(); ++it){
        /* Try to build resources with that specific Locator*/
        newSendersBuffer = network_factory_.BuildSenderResources((*it));
        uint32_t tries = 100;
        while(newSendersBuffer.empty() && tries != 0)
        {
            //No ReceiverResources have been added, therefore we have to change the Locator
            (*it) = applyLocatorAdaptRule(*it); //Mutate the Locator to find a suitable rule. Overwrite the old one as it is useless now.
            newSendersBuffer = network_factory_.BuildSenderResources((*it));
            --tries;
        }
        //Now we DO have resources, and the new locator is already replacing the old one.
        for(auto mit= newSendersBuffer.begin(); mit!= newSendersBuffer.end(); ++mit){
            newSenders.push_back(std::move(*mit));
        }

        //newSenders.insert(newSenders.end(), newSendersBuffer.begin(), newSendersBuffer.end());
        newSendersBuffer.clear();
    }

    send_resources_mutex_.lock();
    for(auto mit=newSenders.begin(); mit!=newSenders.end();++mit){
        sender_resource_.push_back(std::move(*mit));
    }
    send_resources_mutex_.unlock();
    att_.defaultOutLocatorList = defcopy;

    if (!hasLocatorsDefined){
        logInfo(RTPS_PARTICIPANT, att_.getName() << " Created with NO default Send Locator List, adding Locators: " << att_.defaultOutLocatorList);
    }

#if HAVE_SECURITY
    // Start security
    security_manager_.init();
#endif

    //START BUILTIN PROTOCOLS
    builtin_protocols_ = new BuiltinProtocols(*this, att_.builtin);
    if(!builtin_protocols_->init())
    {
        logError(RTPS_PARTICIPANT, "The builtin protocols were not correctly initialized");
    }
    //eClock::my_sleep(300);

    logInfo(RTPS_PARTICIPANT,"RTPSParticipant \"" <<  att_.getName() << "\" with guidPrefix: " <<guid_.guidPrefix);
}

//TODO(Ricardo) Review if necessary. Currently used by flowcontrollers.
const std::vector<std::shared_ptr<RTPSWriter::impl>>& RTPSParticipant::impl::getAllWriters() const
{
    return all_writer_list_;
}

const std::vector<std::shared_ptr<RTPSReader::impl>>& RTPSParticipant::impl::getAllReaders() const
{
    return all_reader_list_;
}

RTPSParticipant::impl::~impl()
{
    delete(builtin_protocols_);
    builtin_protocols_ = nullptr;

#if HAVE_SECURITY
    security_manager_.destroy();
#endif

    while(!all_reader_list_.empty())
    {
        std::shared_ptr<RTPSReader::impl> reader(all_reader_list_.front());
        remove_reader(reader);
    }

    while(!all_writer_list_.empty())
    {
        std::shared_ptr<RTPSWriter::impl> writer(all_writer_list_.front());
        remove_writer(writer);
    }

    // Safely abort threads.
    for(auto& block : receiver_resource_list_)
    {
        block.resourceAlive = false;
        block.Receiver.Abort();
        block.m_thread->join();
        delete block.m_thread;
    }

    // Destruct message receivers
    for(auto& block : receiver_resource_list_)
    {
        delete block.mp_receiver;
    }
    receiver_resource_list_.clear();

    delete(resources_semaphore_);
    sender_resource_.clear();

    delete(event_thread_);

    delete(mutex_);
}

/*
 *
 * MAIN RTPSParticipant IMPL API
 *
 */


std::shared_ptr<RTPSWriter::impl> RTPSParticipant::impl::create_writer(const WriterAttributes& param,
        WriterHistory::impl& history, RTPSWriter::impl::Listener* listener,
        const EntityId_t& entityId,bool isBuiltin)
{
    std::string type = (param.endpoint.reliabilityKind == RELIABLE) ? "RELIABLE" :"BEST_EFFORT";
    logInfo(RTPS_PARTICIPANT," of type " << type);
    EntityId_t entId;
    if(entityId == c_EntityId_Unknown)
    {
        if(param.endpoint.topicKind == NO_KEY)
            entId.value[3] = 0x03;
        else if(param.endpoint.topicKind == WITH_KEY)
            entId.value[3] = 0x02;
        uint32_t idnum;
        if(param.endpoint.getEntityID()>0)
            idnum = param.endpoint.getEntityID();
        else
        {
            id_counter_++;
            idnum = id_counter_;
        }

        octet* c = (octet*)&idnum;
        entId.value[2] = c[0];
        entId.value[1] = c[1];
        entId.value[0] = c[2];
        if(this->existsEntityId(entId,WRITER))
        {
            logError(RTPS_PARTICIPANT,"A writer with the same entityId already exists in this RTPSParticipant");
            return nullptr;
        }
    }
    else
    {
        entId = entityId;
    }
    if(!param.endpoint.unicastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Unicast Locator List for Writer contains invalid Locator");
        return nullptr;
    }
    if(!param.endpoint.multicastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Multicast Locator List for Writer contains invalid Locator");
        return nullptr;
    }
    if(!param.endpoint.outLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Output Locator List for Writer contains invalid Locator");
        return nullptr;
    }
    if (((param.throughputController.bytesPerPeriod != UINT32_MAX && param.throughputController.periodMillisecs != 0) ||
                (att_.throughputController.bytesPerPeriod != UINT32_MAX && att_.throughputController.periodMillisecs != 0)) &&
            param.mode != ASYNCHRONOUS_WRITER)
    {
        logError(RTPS_PARTICIPANT, "Writer has to be configured to publish asynchronously, because a flowcontroller was configured");
        return nullptr;
    }

    // Get properties.
#if HAVE_SECURITY
    bool submessage_protection = false;
    const std::string* property_value = PropertyPolicyHelper::find_property(param.endpoint.properties, "rtps.endpoint.submessage_protection_kind");

    if(property_value != nullptr && property_value->compare("ENCRYPT") == 0)
        submessage_protection = true;

    bool payload_protection = false;
    property_value = PropertyPolicyHelper::find_property(param.endpoint.properties, "rtps.endpoint.payload_protection_kind");

    if(property_value != nullptr && property_value->compare("ENCRYPT") == 0)
        payload_protection = true;
#endif

    WriterAttributes attributes(param);

    // Normalize unicast locators
    if(!attributes.endpoint.unicastLocatorList.empty())
    {
        network_factory_.NormalizeLocators(attributes.endpoint.unicastLocatorList);
    }

    std::shared_ptr<RTPSWriter::impl> writer_impl;
    GUID_t guid(guid_.guidPrefix, entId);
    if(attributes.endpoint.reliabilityKind == BEST_EFFORT)
    {
        writer_impl.reset(new StatelessWriter::impl(*this, guid, attributes, history, listener));
    }
    else if(attributes.endpoint.reliabilityKind == RELIABLE)
    {
        writer_impl.reset(new StatefulWriter::impl(*this, guid, attributes, history, listener));
    }

    if(!writer_impl)
    {
        return nullptr;
    }

    if(!writer_impl->init())
    {
        return nullptr;
    }

#if HAVE_SECURITY
    if(submessage_protection || payload_protection)
    {
        if(submessage_protection)
        {
            writer_impl->is_submessage_protected(true);
        }
        if(payload_protection)
        {
            writer_impl->is_payload_protected(true);
        }

        if(!security_manager_.register_local_writer(writer_impl->getGuid(),
                    attributes.endpoint.properties.properties()))
        {
            return nullptr;
        }
    }
#endif

    createSendResources((Endpoint *)writer_impl.get());
    if(attributes.endpoint.reliabilityKind == RELIABLE)
    {
        if (!createAndAssociateReceiverswithEndpoint(*writer_impl))
        {
            return nullptr;
        }
    }

    // Asynchronous thread runs regardless of mode because of
    // nack response duties.
    AsyncWriterThread::add_writer(*writer_impl);

    std::lock_guard<std::recursive_mutex> guard(*mutex_);
    all_writer_list_.push_back(writer_impl);

    if(!isBuiltin)
    {
        user_writer_list_.push_back(writer_impl.get());
    }

    // If the terminal throughput controller has proper user defined values, instantiate it
    if (attributes.throughputController.bytesPerPeriod != UINT32_MAX && attributes.throughputController.periodMillisecs != 0)
    {
        std::unique_ptr<FlowController> controller(new ThroughputController(attributes.throughputController,
                    writer_impl.get()));
        writer_impl->add_flow_controller(std::move(controller));
    }

    return writer_impl;
}


std::shared_ptr<RTPSReader::impl> RTPSParticipant::impl::create_reader(const ReaderAttributes& param, ReaderHistory* hist,
        RTPSReader::impl::Listener* listener, const EntityId_t& entityId, bool isBuiltin, bool enable)
{
    std::string type = (param.endpoint.reliabilityKind == RELIABLE) ? "RELIABLE" :"BEST_EFFORT";
    logInfo(RTPS_PARTICIPANT," of type " << type);
    EntityId_t entId;
    if(entityId == c_EntityId_Unknown)
    {
        if(param.endpoint.topicKind == NO_KEY)
            entId.value[3] = 0x04;
        else if(param.endpoint.topicKind == WITH_KEY)
            entId.value[3] = 0x07;
        uint32_t idnum;
        if(param.endpoint.getEntityID()>0)
            idnum = param.endpoint.getEntityID();
        else
        {
            id_counter_++;
            idnum = id_counter_;
        }

        octet* c = (octet*)&idnum;
        entId.value[2] = c[0];
        entId.value[1] = c[1];
        entId.value[0] = c[2];
        if(this->existsEntityId(entId,WRITER))
        {
            logError(RTPS_PARTICIPANT,"A reader with the same entityId already exists in this RTPSParticipant");
            return nullptr;
        }
    }
    else
    {
        entId = entityId;
    }
    if(!param.endpoint.unicastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Unicast Locator List for Reader contains invalid Locator");
        return nullptr;
    }
    if(!param.endpoint.multicastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Multicast Locator List for Reader contains invalid Locator");
        return nullptr;
    }
    if(!param.endpoint.outLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Output Locator List for Reader contains invalid Locator");
        return nullptr;
    }

    // Get properties.
#if HAVE_SECURITY
    bool submessage_protection = false;
    const std::string* property_value = PropertyPolicyHelper::find_property(param.endpoint.properties, "rtps.endpoint.submessage_protection_kind");

    if(property_value != nullptr && property_value->compare("ENCRYPT") == 0)
    {
        submessage_protection = true;
    }

    bool payload_protection = false;
    property_value = PropertyPolicyHelper::find_property(param.endpoint.properties, "rtps.endpoint.payload_protection_kind");

    if(property_value != nullptr && property_value->compare("ENCRYPT") == 0)
    {
        payload_protection = true;
    }
#endif

    ReaderAttributes attributes(param);

    // Normalize unicast locators
    if (!attributes.endpoint.unicastLocatorList.empty())
    {
        network_factory_.NormalizeLocators(attributes.endpoint.unicastLocatorList);
    }

    std::shared_ptr<RTPSReader::impl> reader_impl;
    GUID_t guid(guid_.guidPrefix, entId);
    if(attributes.endpoint.reliabilityKind == BEST_EFFORT)
    {
        reader_impl.reset(new StatelessReader::impl(*this, guid, attributes, hist, listener));
    }
    else if(attributes.endpoint.reliabilityKind == RELIABLE)
    {
        reader_impl.reset(new StatefulReader::impl(*this, guid, attributes, hist, listener));
    }

    if(!reader_impl)
    {
        return nullptr;
    }

    if(!reader_impl->init())
    {
        return nullptr;
    }

#if HAVE_SECURITY
    if(submessage_protection || payload_protection)
    {
        if(submessage_protection)
        {
            reader_impl->is_submessage_protected(true);
        }
        if(payload_protection)
        {
            reader_impl->is_payload_protected(true);
        }

        if(!security_manager_.register_local_reader(reader_impl->getGuid(),
                    attributes.endpoint.properties.properties()))
        {
            return nullptr;
        }
    }
#endif

    if(attributes.endpoint.reliabilityKind == RELIABLE)
    {
        createSendResources(reader_impl.get());
    }

    if(isBuiltin)
    {
        reader_impl->setTrustedWriter(TrustedWriter(reader_impl->getGuid().entityId));
    }

    if(enable)
    {
        if (!createAndAssociateReceiverswithEndpoint(*reader_impl))
        {
            return nullptr;
        }
    }

    std::lock_guard<std::recursive_mutex> guard(*mutex_);
    all_reader_list_.push_back(reader_impl);

    if(!isBuiltin)
    {
        user_reader_list_.push_back(reader_impl.get());
    }

    return reader_impl;
}

//TODO(Ricardo) Think if api for rtpsreader has to use the shared_ptr.
bool RTPSParticipant::impl::enable_reader(RTPSReader::impl& reader)
{
    if(assign_endpoint_listen_resources(reader))
    {
        return true;
    }

    return false;
}

bool RTPSParticipant::impl::register_writer(RTPSWriter::impl& writer, TopicAttributes& topicAtt,
        WriterQos& wqos)
{
    return this->builtin_protocols_->add_local_writer(writer, topicAtt, wqos);
}

bool RTPSParticipant::impl::register_reader(RTPSReader::impl& reader, TopicAttributes& topicAtt,
        ReaderQos& rqos)
{
    return this->builtin_protocols_->add_local_reader(reader, topicAtt, rqos);
}

bool RTPSParticipant::impl::update_local_writer(RTPSWriter::impl& writer, WriterQos& wqos)
{
    return this->builtin_protocols_->update_local_writer(writer, wqos);
}

bool RTPSParticipant::impl::update_local_reader(RTPSReader::impl& reader, ReaderQos& rqos)
{
    return this->builtin_protocols_->update_local_reader(reader, rqos);
}

/*
 *
 * AUXILIARY METHODS
 *
 *  */


bool RTPSParticipant::impl::existsEntityId(const EntityId_t& ent, EndpointKind_t kind) const
{
    if(kind == WRITER)
    {
        for(std::vector<RTPSWriter::impl*>::const_iterator it = user_writer_list_.begin();
                it != user_writer_list_.end(); ++it)
        {
            if(ent == (*it)->getGuid().entityId)
                return true;
        }
    }
    else
    {
        for(std::vector<RTPSReader::impl*>::const_iterator it = user_reader_list_.begin();
                it!=user_reader_list_.end();++it)
        {
            if(ent == (*it)->getGuid().entityId)
                return true;
        }
    }
    return false;
}


/*
 *
 * RECEIVER RESOURCE METHODS
 *
 */

bool RTPSParticipant::impl::assign_endpoint_listen_resources(Endpoint& endpoint)
{
    //Tag the endpoint with the ReceiverResources
    bool valid = true;

    /* No need to check for emptiness on the lists, as it was already done on part function
       In case are using the default list of Locators they have already been embedded to the parameters */

    //UNICAST
    assign_endpoint_to_locatorlist(endpoint, endpoint.getAttributes()->unicastLocatorList);
    //MULTICAST
    assign_endpoint_to_locatorlist(endpoint, endpoint.getAttributes()->multicastLocatorList);
    return valid;
}

bool RTPSParticipant::impl::createAndAssociateReceiverswithEndpoint(Endpoint& pend){
    /*	This function...
        - Asks the network factory for new resources
        - Encapsulates the new resources within the ReceiverControlBlock list
        - Associated the endpoint to the new elements in the list
        - Launches the listener thread
        */
    // 1 - Ask the network factory to generate the elements that do still not exist
    std::vector<ReceiverResource> newItems;							//Store the newly created elements
    std::vector<ReceiverResource> newItemsBuffer;					//Store intermediate results
    //Iterate through the list of unicast and multicast locators the endpoint has... unless its empty
    //In that case, just use the standard
    if (pend.getAttributes()->unicastLocatorList.empty() && pend.getAttributes()->multicastLocatorList.empty())
    {
        //Default unicast
        pend.getAttributes()->unicastLocatorList = att_.defaultUnicastLocatorList;
    }
    createReceiverResources(pend.getAttributes()->unicastLocatorList, false);
    createReceiverResources(pend.getAttributes()->multicastLocatorList, false);

    // Associate the Endpoint with ReceiverResources inside ReceiverControlBlocks
    assign_endpoint_listen_resources(pend);
    return true;
}

void RTPSParticipant::impl::performListenOperation(ReceiverControlBlock *receiver, Locator_t input_locator)
{
    while(receiver->resourceAlive)
    {
        // Blocking receive.
        auto& msg = receiver->mp_receiver->m_rec_msg;
        CDRMessage::initCDRMsg(&msg);
        if(!receiver->Receiver.Receive(msg.buffer, msg.max_size, msg.length, input_locator))
            continue;

        // Processes the data through the CDR Message interface.
        receiver->mp_receiver->processCDRMsg(guid_.guidPrefix, &input_locator, &receiver->mp_receiver->m_rec_msg);
    }
}


bool RTPSParticipant::impl::assign_endpoint_to_locatorlist(Endpoint& endpoint, LocatorList_t& list)
{
    /* Note:
       The previous version of this function associated (or created) ListenResources and added the endpoint to them.
       It then requested the list of Locators the Listener is listening to and appended to the LocatorList_t from the paremeters.

       This has been removed becuase it is considered redundant. For ReceiveResources that listen on multiple interfaces, only
       one of the supported Locators is needed to make the match, and the case of new ListenResources being created has been removed
       since its the NetworkFactory the one that takes care of Resource creation.
       */
    LocatorList_t finalList;
    for(auto lit = list.begin();lit != list.end();++lit)
    {
        //Iteration of all Locators within the Locator list passed down as argument
        std::lock_guard<std::mutex> guard(receiver_resource_list_mutex_);
        //Check among ReceiverResources whether the locator is supported or not
        for (auto it = receiver_resource_list_.begin(); it != receiver_resource_list_.end(); ++it)
        {
            //Take mutex for the resource since we are going to interact with shared resources
            //std::lock_guard<std::mutex> guard((*it).mtx);
            if ((*it).Receiver.SupportsLocator(*lit))
            {
                if(endpoint.getAttributes()->endpointKind == WRITER)
                {
                    //Supported! Take mutex and update lists - We maintain reader/writer discrimination just in case
                    (*it).mp_receiver->associate_writer(&dynamic_cast<RTPSWriter::impl&>(endpoint));
                    // end association between reader/writer and the receive resources
                }
                else if(endpoint.getAttributes()->endpointKind == READER)
                {
                    //Supported! Take mutex and update lists - We maintain reader/writer discrimination just in case
                    (*it).mp_receiver->associate_reader(&dynamic_cast<RTPSReader::impl&>(endpoint));
                    // end association between reader/writer and the receive resources
                }
            }

        }
        //Finished iteratig through all ListenResources for a single Locator (from the parameter list).
        //Since this function is called after checking with NetFactory we do not have to create any more resource.
    }
    return true;
}

bool RTPSParticipant::impl::createSendResources(Endpoint *pend)
{
    std::vector<SenderResource> newSenders;
    std::vector<SenderResource> SendersBuffer;
    if (pend->att_.outLocatorList.empty()){
        //Output locator ist is empty, use predetermined ones
        pend->att_.outLocatorList = att_.defaultOutLocatorList;		//Tag the Endpoint with the Default list so it can use it to send
        //Already created them on constructor, so we can skip the creation
        return true;
    }
    //Output locators have been specified, create them
    for (auto it = pend->att_.outLocatorList.begin(); it != pend->att_.outLocatorList.end(); ++it){
        SendersBuffer = network_factory_.BuildSenderResources((*it));
        for(auto mit = SendersBuffer.begin(); mit!= SendersBuffer.end(); ++mit){
            newSenders.push_back(std::move(*mit));
        }
        //newSenders.insert(newSenders.end(), SendersBuffer.begin(), SendersBuffer.end());
        SendersBuffer.clear();
    }

    std::lock_guard<std::mutex> guard(send_resources_mutex_);
    for(auto mit = newSenders.begin();mit!=newSenders.end();++mit){
        sender_resource_.push_back(std::move(*mit));
    }

    return true;
}

void RTPSParticipant::impl::createReceiverResources(LocatorList_t& Locator_list, bool ApplyMutation)
{
    std::vector<ReceiverResource> newItemsBuffer;

    for(auto it_loc = Locator_list.begin(); it_loc != Locator_list.end(); ++it_loc)
    {
        bool ret  = network_factory_.BuildReceiverResources((*it_loc), newItemsBuffer);

        if(!ret && ApplyMutation)
        {
            int tries = 0;
            while(!ret && (tries < MutationTries)){
                tries++;
                (*it_loc) = applyLocatorAdaptRule(*it_loc);
                ret = network_factory_.BuildReceiverResources((*it_loc), newItemsBuffer);
            }
        }

        for(auto it_buffer = newItemsBuffer.begin(); it_buffer != newItemsBuffer.end(); ++it_buffer)
        {
            std::lock_guard<std::mutex> lock(receiver_resource_list_mutex_);
            //Push the new items into the ReceiverResource buffer
            receiver_resource_list_.push_back(ReceiverControlBlock(std::move(*it_buffer)));
            //Create and init the MessageReceiver
            receiver_resource_list_.back().mp_receiver = new MessageReceiver(*this, network_factory_.get_max_message_size_between_transports());
            receiver_resource_list_.back().mp_receiver->init(network_factory_.get_max_message_size_between_transports());

            //Init the thread
            receiver_resource_list_.back().m_thread = new std::thread(&RTPSParticipant::impl::performListenOperation,this, &(receiver_resource_list_.back()),(*it_loc));
        }
        newItemsBuffer.clear();
    }
}

bool RTPSParticipant::impl::remove_writer(std::shared_ptr<RTPSWriter::impl>& writer)
{
    bool found_in_users = false;
    {
        std::lock_guard<std::recursive_mutex> guard(*mutex_);

        auto wit = all_writer_list_.begin();
        for(; wit != all_writer_list_.end(); ++wit)
        {
            if(*wit == writer) //Found it
            {
                break;
            }
        }

        if(wit == all_writer_list_.end())
        {
            return false;
        }

        all_writer_list_.erase(wit);

        for(auto witu = user_writer_list_.begin(); witu != user_writer_list_.end(); ++witu)
        {
            if(*witu == writer.get()) //Found it
            {
                user_writer_list_.erase(witu);
                found_in_users = true;
                break;
            }
        }
    }

    AsyncWriterThread::remove_writer(*writer);

    receiver_resource_list_mutex_.lock();
    for(auto it=receiver_resource_list_.begin();it!=receiver_resource_list_.end();++it)
    {
        (*it).mp_receiver->remove_writer(writer.get());
    }
    receiver_resource_list_mutex_.unlock();

    //TODO(Ricardo) Remove this and ask with a function if it is writer.
    if(found_in_users && builtin_protocols_)
    {
        builtin_protocols_->remove_local_writer(*writer);
    }

#if HAVE_SECURITY
    if(writer->is_submessage_protected() || writer->is_payload_protected())
    {
        security_manager_.unregister_local_writer(writer->getGuid());
    }
#endif

    writer->deinit();
    return true;
}


bool RTPSParticipant::impl::remove_reader(std::shared_ptr<RTPSReader::impl>& reader)
{
    bool found_in_users = false;
    {
        std::lock_guard<std::recursive_mutex> guard(*mutex_);

        auto rit = all_reader_list_.begin();
        for(; rit != all_reader_list_.end(); ++rit)
        {
            if(*rit == reader) //Found it
            {
                break;
            }
        }

        if(rit == all_reader_list_.end())
        {
            return false;
        }

        all_reader_list_.erase(rit);

        for(auto ritu = user_reader_list_.begin(); ritu != user_reader_list_.end(); ++ritu)
        {
            if(*ritu == reader.get()) //Found it
            {
                user_reader_list_.erase(ritu);
                found_in_users = true;
                break;
            }
        }
    }

    receiver_resource_list_mutex_.lock();
    for(auto it=receiver_resource_list_.begin();it!=receiver_resource_list_.end();++it)
    {
        (*it).mp_receiver->remove_reader(reader.get());
    }
    receiver_resource_list_mutex_.unlock();

    if(found_in_users && builtin_protocols_)
    {
        builtin_protocols_->remove_local_reader(*reader);
    }

#if HAVE_SECURITY
    if(reader->is_submessage_protected() || reader->is_payload_protected())
    {
        security_manager_.unregister_local_reader(reader->getGuid());
    }
#endif


    reader->deinit();
    return true;
}


ResourceEvent& RTPSParticipant::impl::getEventResource()
{
    return *this->event_thread_;
}

std::pair<StatefulReader*,StatefulReader*> RTPSParticipant::impl::getEDPReaders()
{
    std::pair<StatefulReader*,StatefulReader*> buffer;
    EDPSimple *EDPPointer = dynamic_cast<EDPSimple*>(builtin_protocols_->mp_PDP->getEDP());
    if(EDPPointer != nullptr)
    {
        //TODO(Ricardo) Review.
        //Means the EDP attached is actually non static and therefore it has Readers
        //buffer.first=EDPPointer->mp_SubReader.first;
        //buffer.second=EDPPointer->mp_PubReader.first;
    }else{
        buffer.first=nullptr;
        buffer.second=nullptr;
    }
    return buffer;

}

std::vector<std::string> RTPSParticipant::impl::getParticipantNames() const
{
    std::vector<std::string> participant_names;
    auto pdp = builtin_protocols_->mp_PDP;
    for (auto it = pdp->ParticipantProxiesBegin();
        it != pdp->ParticipantProxiesEnd();
        ++it)
    {
      participant_names.emplace_back((*it)->m_participantName);
    }
    return participant_names;
}

void RTPSParticipant::impl::sendSync(CDRMessage_t* msg, Endpoint *pend, const Locator_t& destination_loc)
{
    std::lock_guard<std::mutex> guard(send_resources_mutex_);
    for (auto it = sender_resource_.begin(); it != sender_resource_.end(); ++it)
    {
        bool sendThroughResource = false;
        for (auto sit = pend->att_.outLocatorList.begin(); sit != pend->att_.outLocatorList.end(); ++sit)
        {
            if ((*it).SupportsLocator((*sit)))
            {
                sendThroughResource = true;
                break;
            }
        }

        if (sendThroughResource)
            (*it).Send(msg->buffer, msg->length, destination_loc);
    }
}

void RTPSParticipant::impl::announceRTPSParticipantState()
{
    return builtin_protocols_->announceRTPSParticipantState();
}

void RTPSParticipant::impl::stopRTPSParticipantAnnouncement()
{
    return builtin_protocols_->stopRTPSParticipantAnnouncement();
}

void RTPSParticipant::impl::resetRTPSParticipantAnnouncement()
{
    return builtin_protocols_->resetRTPSParticipantAnnouncement();
}

void RTPSParticipant::impl::loose_next_change()
{
    //NOTE: This is replaced by the test transport
    //this->mp_send_thr->loose_next_change();
}


bool RTPSParticipant::impl::newRemoteEndpointDiscovered(const GUID_t& pguid, int16_t userDefinedId,EndpointKind_t kind)
{
    if(att_.builtin.use_STATIC_EndpointDiscoveryProtocol == false)
    {
        logWarning(RTPS_PARTICIPANT,"Remote Endpoints can only be activated with static discovery protocol");
        return false;
    }
    return builtin_protocols_->mp_PDP->newRemoteEndpointStaticallyDiscovered(pguid,userDefinedId,kind);
}

void RTPSParticipant::impl::ResourceSemaphorePost()
{
    if(resources_semaphore_ != nullptr)
    {
        resources_semaphore_->post();
    }
}

void RTPSParticipant::impl::ResourceSemaphoreWait()
{
    if (resources_semaphore_ != nullptr)
    {
        resources_semaphore_->wait();
    }

}

void RTPSParticipant::impl::assertRemoteRTPSParticipantLiveliness(const GuidPrefix_t& guidP)
{
    this->builtin_protocols_->mp_PDP->assertRemoteParticipantLiveliness(guidP);
}

uint32_t RTPSParticipant::impl::getMaxMessageSize() const
{
    uint32_t minMaxMessageSize = UINT32_MAX;
    if(att_.useBuiltinTransports)
    {
        UDPv4TransportDescriptor defaultDescriptor;
        minMaxMessageSize = defaultDescriptor.maxMessageSize;
    }
    for(const auto& it : att_.userTransports)
    {
        if(minMaxMessageSize > (*it).maxMessageSize)
            minMaxMessageSize = (*it).maxMessageSize;
    }

    return minMaxMessageSize;
}

uint32_t RTPSParticipant::impl::getMaxDataSize()
{
    return calculateMaxDataSize(getMaxMessageSize());
}

uint32_t RTPSParticipant::impl::calculateMaxDataSize(uint32_t length)
{
    uint32_t maxDataSize = length;

#if HAVE_SECURITY
    // If there is rtps messsage protection, reduce max size for messages,
    // because extra data is added on encryption.
    if(is_rtps_protected_)
    {
        maxDataSize -= security_manager_.calculate_extra_size_for_rtps_message();
    }
#endif

    // RTPS header
    maxDataSize -= RTPSMESSAGE_HEADER_SIZE;

    return maxDataSize;
}

bool RTPSParticipant::impl::networkFactoryHasRegisteredTransports() const
{
    return network_factory_.numberOfRegisteredTransports() > 0;
}

PDPSimple* RTPSParticipant::impl::pdpsimple()
{
    return builtin_protocols_->mp_PDP;
}


bool RTPSParticipant::impl::get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo)
{
    ParticipantProxyData pdata;

    if(builtin_protocols_->mp_PDP->lookupWriterProxyData(writerGuid, returnedInfo, pdata))
    {
        return true;
    }

    return false;
}

bool RTPSParticipant::impl::get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo)
{
    ParticipantProxyData pdata;

    if(builtin_protocols_->mp_PDP->lookupReaderProxyData(readerGuid, returnedInfo, pdata))
    {
        return true;
    }

    return false;
}

#if HAVE_SECURITY
void RTPSParticipant::impl::set_endpoint_rtps_protection_supports(Endpoint* endpoint, bool support)
{
    endpoint->supports_rtps_protection(support);
}
#endif
