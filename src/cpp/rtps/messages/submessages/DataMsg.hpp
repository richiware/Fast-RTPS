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
 * DataSubMessage.hpp
 *
 */

namespace eprosima{
namespace fastrtps{
namespace rtps{

bool RTPSMessageCreator::addMessageData(CDRMessage_t* msg,
        GuidPrefix_t& guidprefix, const CacheChange_t* change,TopicKind_t topicKind,const EntityId_t& readerId,bool expectsInlineQos,ParameterList_t* inlineQos)
{
    try{

        RTPSMessageCreator::addHeader(msg,guidprefix);

        RTPSMessageCreator::addSubmessageInfoTS_Now(msg,false);

        RTPSMessageCreator::addSubmessageData(msg, change,topicKind,readerId,expectsInlineQos,inlineQos);

        msg->length = msg->pos;
    }
    catch(int e)
    {
        logError(RTPS_CDR_MSG, "Data message error" << e << endl)

            return false;
    }
    return true;
}



bool RTPSMessageCreator::addSubmessageData(CDRMessage_t* msg, const CacheChange_t* change,
        TopicKind_t topicKind, const EntityId_t& readerId, bool expectsInlineQos, ParameterList_t* inlineQos) {
    CDRMessage_t& submsgElem = g_pool_submsg.reserve_CDRMsg((uint16_t)change->serialized_payload.length);
    CDRMessage::initCDRMsg(&submsgElem);
    //Create the two CDR msgs
    //CDRMessage_t submsgElem;
    octet flags = 0x0;
#if EPROSIMA_BIG_ENDIAN
    submsgElem.msg_endian = BIGEND;
#else
    flags = flags | BIT(0);
    submsgElem.msg_endian = LITTLEEND;
#endif

    //Find out flags
    bool dataFlag = false;
    bool keyFlag = false;
    bool inlineQosFlag = false;
    if(change->kind == ALIVE && change->serialized_payload.length>0 && change->serialized_payload.data!=NULL)
    {
        dataFlag = true;
        keyFlag = false;
    }
    else
    {
        dataFlag = false;
        keyFlag = true;
    }
    if(topicKind == NO_KEY)
        keyFlag = false;
    inlineQosFlag = false;
    // cout << "expects inline qos: " << expectsInlineQos << " topic KIND: " << (topicKind == WITH_KEY) << endl;
    if(inlineQos != NULL || expectsInlineQos || change->kind != ALIVE) //expects inline qos
    {
        if(topicKind == WITH_KEY)
        {
            flags = flags | BIT(1);
            inlineQosFlag = true;
            //cout << "INLINE QOS FLAG TO 1 " << endl;
            keyFlag = false;
        }
    }
    // Maybe the inline QoS because a WriteParam.
    else if(change->write_params.related_sample_identity() != SampleIdentity::unknown())
    {
        inlineQosFlag = true;
        flags = flags | BIT(1);
    }

    if(dataFlag)
        flags = flags | BIT(2);
    if(keyFlag)
        flags = flags | BIT(3);

    octet status = 0;
    if(change->kind == NOT_ALIVE_DISPOSED)
        status = status | BIT(0);
    if(change->kind == NOT_ALIVE_UNREGISTERED)
        status = status | BIT(1);
    if(change->kind == NOT_ALIVE_DISPOSED_UNREGISTERED)
    {
        status = status | BIT(0);
        status = status | BIT(1);
    }

    // TODO Check, because I saw init the message two times (other on RTPSMessageGroup::prepareDataSubM)
    CDRMessage::initCDRMsg(&submsgElem);
    bool added_no_error = true;
    try{
        //First we create the submsgElements:
        //extra flags. not in this version.
        added_no_error &= CDRMessage::addUInt16(&submsgElem,0);
        //octet to inline Qos is 12, may change in future versions
        added_no_error &= CDRMessage::addUInt16(&submsgElem,RTPSMESSAGE_OCTETSTOINLINEQOS_DATASUBMSG);
        //Entity ids
        added_no_error &= CDRMessage::addEntityId(&submsgElem,&readerId);
        added_no_error &= CDRMessage::addEntityId(&submsgElem,&change->writer_guid.entityId);
        //Add Sequence Number
        added_no_error &= CDRMessage::addSequenceNumber(&submsgElem,&change->sequence_number);
        //Add INLINE QOS AND SERIALIZED PAYLOAD DEPENDING ON FLAGS:


        if(inlineQosFlag) //inlineQoS
        {
            if(change->write_params.related_sample_identity() != SampleIdentity::unknown())
            {
                CDRMessage::addParameterSampleIdentity(&submsgElem, change->write_params.related_sample_identity());
            }

            if(topicKind == WITH_KEY)
            {
                //cout << "ADDDING PARAMETER KEY " << endl;
                CDRMessage::addParameterKey(&submsgElem,&change->instance_handle);
            }

            if(change->kind != ALIVE)
                CDRMessage::addParameterStatus(&submsgElem,status);

            if(inlineQos!=NULL)
                ParameterList::writeParameterListToCDRMsg(&submsgElem, inlineQos, false);
            else
                CDRMessage::addParameterSentinel(&submsgElem);
        }

        //Add Serialized Payload
        if(dataFlag)
            added_no_error &= CDRMessage::addData(&submsgElem, change->serialized_payload.data, change->serialized_payload.length);

        if(keyFlag)
        {
            added_no_error &= CDRMessage::addOctet(&submsgElem,0); //ENCAPSULATION
            if(submsgElem.msg_endian == BIGEND)
                added_no_error &= CDRMessage::addOctet(&submsgElem,PL_CDR_BE); //ENCAPSULATION
            else
                added_no_error &= CDRMessage::addOctet(&submsgElem,PL_CDR_LE); //ENCAPSULATION

            added_no_error &= CDRMessage::addUInt16(&submsgElem,0); //ENCAPSULATION OPTIONS
            added_no_error &= CDRMessage::addParameterKey(&submsgElem,&change->instance_handle);
            added_no_error &= CDRMessage::addParameterStatus(&submsgElem,status);
            added_no_error &= CDRMessage::addParameterSentinel(&submsgElem);
        }

        // Align submessage to rtps alignment (4).
        uint32_t align = (4 - submsgElem.pos % 4) & 3;
        for(uint32_t count = 0; count < align; ++count)
            added_no_error &= CDRMessage::addOctet(&submsgElem, 0);

        //if(align > 0)
        {
            //submsgElem.pos += align;
            //submsgElem.length += align;
        }

        //Once the submessage elements are added, the submessage header is created, assigning the correct size.
        added_no_error &= RTPSMessageCreator::addSubmessageHeader(msg, DATA,flags, (uint16_t)submsgElem.length);
        //Append Submessage elements to msg

        added_no_error &= CDRMessage::appendMsg(msg, &submsgElem);
        g_pool_submsg.release_CDRMsg(submsgElem);
    }
    catch(int t){
        logError(RTPS_CDR_MSG,"Data SUBmessage not created"<<t<<endl)

            return false;
    }
    return added_no_error;
}


bool RTPSMessageCreator::addMessageDataFrag(CDRMessage_t* msg, GuidPrefix_t& guidprefix,
        const CacheChange_t* change, uint32_t fragment_number, TopicKind_t topicKind, const EntityId_t& readerId,
        bool expectsInlineQos, ParameterList_t* inlineQos)
{

    try{

        RTPSMessageCreator::addHeader(msg, guidprefix);

        RTPSMessageCreator::addSubmessageInfoTS_Now(msg, false);

        // Calculate fragment start
        uint32_t fragment_start = change->getFragmentSize() * (fragment_number - 1);
        // Calculate fragment size. If last fragment, size may be smaller
        uint32_t fragment_size = fragment_number < change->getFragmentCount() ? change->getFragmentSize() :
            change->serialized_payload.length - fragment_start;

        // TODO (Ricardo). Check to create special wrapper.
        CacheChange_t change_to_add;
        change_to_add.copy_not_memcpy(*change);
        change_to_add.serialized_payload.data = change->serialized_payload.data + fragment_start;
        change_to_add.serialized_payload.length = fragment_size;

        RTPSMessageCreator::addSubmessageDataFrag(msg, &change_to_add, fragment_number, change->serialized_payload.length,
                topicKind, readerId, expectsInlineQos, inlineQos);

        change_to_add.serialized_payload.data = NULL;

        msg->length = msg->pos;
    }
    catch (int e)
    {
        logError(RTPS_CDR_MSG, "Data message error" << e << endl)
            return false;
    }
    return true;
}



bool RTPSMessageCreator::addSubmessageDataFrag(CDRMessage_t* msg, const CacheChange_t* change, uint32_t fragment_number,
        uint32_t sample_size, TopicKind_t topicKind, const EntityId_t& readerId, bool expectsInlineQos,
        ParameterList_t* inlineQos)
{
    CDRMessage_t& submsgElem = g_pool_submsg.reserve_CDRMsg((uint16_t)change->serialized_payload.length);
    CDRMessage::initCDRMsg(&submsgElem);
    //Create the two CDR msgs
    //CDRMessage_t submsgElem;
    octet flags = 0x0;
#if EPROSIMA_BIG_ENDIAN
    submsgElem.msg_endian = BIGEND;
#else
    flags = flags | BIT(0);
    submsgElem.msg_endian = LITTLEEND;
#endif

    //Find out flags
    bool keyFlag = false;
    bool inlineQosFlag = false;
    if (change->kind == ALIVE && change->serialized_payload.length>0 && change->serialized_payload.data != NULL)
    {
        keyFlag = false;
    }
    else
    {
        keyFlag = true;
    }

    if (topicKind == NO_KEY)
        keyFlag = false;

    // cout << "expects inline qos: " << expectsInlineQos << " topic KIND: " << (topicKind == WITH_KEY) << endl;
    if (inlineQos != NULL || expectsInlineQos || change->kind != ALIVE) //expects inline qos
    {
        if (topicKind == WITH_KEY)
        {
            flags = flags | BIT(1);
            inlineQosFlag = true;
            //cout << "INLINE QOS FLAG TO 1 " << endl;
            keyFlag = false;
        }
    }
    // Maybe the inline QoS because a WriteParam.
    else if (change->write_params.related_sample_identity() != SampleIdentity::unknown())
    {
        inlineQosFlag = true;
        flags = flags | BIT(1);
    }

    if (keyFlag)
        flags = flags | BIT(2);

    octet status = 0;
    if (change->kind == NOT_ALIVE_DISPOSED)
        status = status | BIT(0);
    if (change->kind == NOT_ALIVE_UNREGISTERED)
        status = status | BIT(1);

    if (change->kind == NOT_ALIVE_DISPOSED_UNREGISTERED)
    {
        status = status | BIT(0);
        status = status | BIT(1);
    }

    // TODO Check, because I saw init the message two times (other on RTPSMessageGroup::prepareDataSubM)
    CDRMessage::initCDRMsg(&submsgElem);
    bool added_no_error = true;

    try
    {
        //First we create the submsgElements:
        //extra flags. not in this version.
        added_no_error &= CDRMessage::addUInt16(&submsgElem, 0);

        //octet to inline Qos is 28, may change in future versions
        added_no_error &= CDRMessage::addUInt16(&submsgElem, RTPSMESSAGE_OCTETSTOINLINEQOS_DATAFRAGSUBMSG);

        //Entity ids
        added_no_error &= CDRMessage::addEntityId(&submsgElem, &readerId);
        added_no_error &= CDRMessage::addEntityId(&submsgElem, &change->writer_guid.entityId);

        //Add Sequence Number
        added_no_error &= CDRMessage::addSequenceNumber(&submsgElem, &change->sequence_number);

        // Add fragment starting number
        added_no_error &= CDRMessage::addUInt32(&submsgElem, fragment_number); // fragments start in 1

        // Add fragments in submessage
        added_no_error &= CDRMessage::addUInt16(&submsgElem, 1); // we are sending one fragment

        // Add fragment size
        added_no_error &= CDRMessage::addUInt16(&submsgElem, change->getFragmentSize());

        // Add total sample size
        added_no_error &= CDRMessage::addUInt32(&submsgElem, sample_size); //TODO(Ricardo) Sample size in CacheChange

        //Add INLINE QOS AND SERIALIZED PAYLOAD DEPENDING ON FLAGS:
        if (inlineQosFlag) //inlineQoS
        {
            if(change->write_params.related_sample_identity() != SampleIdentity::unknown())
                CDRMessage::addParameterSampleIdentity(&submsgElem, change->write_params.related_sample_identity());

            if(topicKind == WITH_KEY)
                CDRMessage::addParameterKey(&submsgElem,&change->instance_handle);

            if(change->kind != ALIVE)
                CDRMessage::addParameterStatus(&submsgElem,status);

            if(inlineQos!=NULL)
                ParameterList::writeParameterListToCDRMsg(&submsgElem, inlineQos, false);
            else
                CDRMessage::addParameterSentinel(&submsgElem);
        }

        //Add Serialized Payload XXX TODO
        if (!keyFlag) // keyflag = 0 means that the serializedPayload SubmessageElement contains the serialized Data 
        {
            added_no_error &= CDRMessage::addData(&submsgElem, change->serialized_payload.data,
                    change->serialized_payload.length);
        }
        else
        {   // keyflag = 1 means that the serializedPayload SubmessageElement contains the serialized Key 
            /*
               added_no_error &= CDRMessage::addOctet(&submsgElem, 0); //ENCAPSULATION
               if (submsgElem.msg_endian == BIGEND)
               added_no_error &= CDRMessage::addOctet(&submsgElem, PL_CDR_BE); //ENCAPSULATION
               else
               added_no_error &= CDRMessage::addOctet(&submsgElem, PL_CDR_LE); //ENCAPSULATION

               added_no_error &= CDRMessage::addUInt16(&submsgElem, 0); //ENCAPSULATION OPTIONS
               added_no_error &= CDRMessage::addParameterKey(&submsgElem, &change->instanceHandle);
               added_no_error &= CDRMessage::addParameterStatus(&submsgElem, status);
               added_no_error &= CDRMessage::addParameterSentinel(&submsgElem);
               */
            return false;
        }

        // TODO(Ricardo) This should be on cachechange.
        // Align submessage to rtps alignment (4).
        uint32_t align = (4 - submsgElem.pos % 4) & 3;
        for (uint32_t count = 0; count < align; ++count)
            added_no_error &= CDRMessage::addOctet(&submsgElem, 0);

        //Once the submessage elements are added, the submessage header is created, assigning the correct size.
        added_no_error &= RTPSMessageCreator::addSubmessageHeader(msg, DATA_FRAG, flags, (uint16_t)submsgElem.length);

        //Append Submessage elements to msg
        added_no_error &= CDRMessage::appendMsg(msg, &submsgElem);
        g_pool_submsg.release_CDRMsg(submsgElem);

    }
    catch (int t){
        logError(RTPS_CDR_MSG, "Data SUBmessage not created" << t << endl)
            return false;
    }

    return added_no_error;
}


}
}
}
