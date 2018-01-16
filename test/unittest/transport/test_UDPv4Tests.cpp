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

#include <fastrtps/transport/test_UDPv4Transport.h>
#include <gtest/gtest.h>
#include <thread>
#include <fastrtps/rtps/common/CDRMessage_t.h>
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/qos/ParameterList.h>
#include <fastrtps/log/Log.h>

#include <memory>
#include <string>

static uint32_t g_default_port = 7400;

using namespace eprosima::fastrtps::rtps;

class test_UDPv4Tests: public ::testing::Test
{
   public:
   test_UDPv4Tests()
   {
      HELPER_SetDescriptorDefaults();
   }

   ~test_UDPv4Tests()
   {
      Log::KillThread();
   }

   void HELPER_SetDescriptorDefaults();
   void HELPER_WarmUpOutput(test_UDPv4Transport& transport);
   void HELPER_FillDataMessage(CDRMessage_t& message, SequenceNumber_t sequenceNumber);
   void HELPER_FillAckNackMessage(CDRMessage_t& message);
   void HELPER_FillHeartbeatMessage(CDRMessage_t& message);

   test_UDPv4TransportDescriptor descriptor;
   std::unique_ptr<std::thread> senderThread;
   std::unique_ptr<std::thread> receiverThread;
};

TEST_F(test_UDPv4Tests, DATA_messages_dropped)
{
   // Given
   descriptor.dropDataMessagesPercentage = 100;
   test_UDPv4Transport transportUnderTest(descriptor);
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillDataMessage(testDataMessage, SequenceNumber_t());
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(1, test_UDPv4Transport::DropLog.size());
}

TEST_F(test_UDPv4Tests, ACKNACK_messages_dropped)
{
   // Given
   descriptor.dropAckNackMessagesPercentage = 100;
   test_UDPv4Transport transportUnderTest(descriptor);
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillAckNackMessage(testDataMessage);
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(1, test_UDPv4Transport::DropLog.size());
}

TEST_F(test_UDPv4Tests, HEARTBEAT_messages_dropped)
{
   // Given
   descriptor.dropHeartbeatMessagesPercentage = 100;
   test_UDPv4Transport transportUnderTest(descriptor);
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillHeartbeatMessage(testDataMessage);
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(1, test_UDPv4Transport::DropLog.size());
}

TEST_F(test_UDPv4Tests, Dropping_by_random_chance)
{
   // Given
   descriptor.percentageOfMessagesToDrop = 100; // To avoid a non-deterministic test
   test_UDPv4Transport transportUnderTest(descriptor);
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillAckNackMessage(testDataMessage);
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(3, test_UDPv4Transport::DropLog.size());
}

TEST_F(test_UDPv4Tests, dropping_by_sequence_number)
{
   // Given
   std::vector<SequenceNumber_t> sequenceNumbersToDrop(1);
   sequenceNumbersToDrop.back().low = 1;

   descriptor.sequenceNumberDataMessagesToDrop = sequenceNumbersToDrop;
   test_UDPv4Transport transportUnderTest(descriptor);
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillDataMessage(testDataMessage, sequenceNumbersToDrop.back());
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(1, test_UDPv4Transport::DropLog.size());
}

TEST_F(test_UDPv4Tests, No_drops_when_unrequested)
{
   // Given
   descriptor.dropHeartbeatMessagesPercentage = 100;
   descriptor.dropDataMessagesPercentage = 100;
   descriptor.granularMode = false;

   test_UDPv4Transport transportUnderTest(descriptor); // Default, no drops
   transportUnderTest.init();
   CDRMessage_t testDataMessage;
   HELPER_FillAckNackMessage(testDataMessage);
   HELPER_WarmUpOutput(transportUnderTest);
   Locator_t locator;
   locator.port = g_default_port;
   locator.kind = LOCATOR_KIND_UDPv4;
   locator.set_IP4_address(239, 255, 1, 4);

   // Then
   ASSERT_TRUE(transportUnderTest.Send(testDataMessage.buffer, testDataMessage.length, locator, locator));
   ASSERT_EQ(0, test_UDPv4Transport::DropLog.size());
}

void test_UDPv4Tests::HELPER_SetDescriptorDefaults()
{
   descriptor.sendBufferSize = 80;
   descriptor.receiveBufferSize = 80;
   descriptor.dropDataMessagesPercentage = 0;
   descriptor.dropDataFragMessagesPercentage = 0;
   descriptor.dropAckNackMessagesPercentage = 0;
   descriptor.dropHeartbeatMessagesPercentage = 0;
   descriptor.percentageOfMessagesToDrop = 0;
   descriptor.dropLogLength = 10;
   descriptor.granularMode = false;
}

void test_UDPv4Tests::HELPER_WarmUpOutput(test_UDPv4Transport& transport)
{
   Locator_t outputChannelLocator;
   outputChannelLocator.port = g_default_port;
   outputChannelLocator.kind = LOCATOR_KIND_UDPv4;
   ASSERT_TRUE(transport.OpenOutputChannel(outputChannelLocator));
}

void test_UDPv4Tests::HELPER_FillDataMessage(CDRMessage_t& message, SequenceNumber_t sequenceNumber)
{
   GuidPrefix_t prefix;
   TopicKind_t topic = WITH_KEY;
   EntityId_t entityID;
   CacheChange_t change;
   ParameterList_t parameters;
   change.sequence_number = sequenceNumber; // Here is where the SN propagates from
	RTPSMessageCreator::addMessageData(&message, prefix, &change, topic, entityID, false, &parameters);
}

void test_UDPv4Tests::HELPER_FillAckNackMessage(CDRMessage_t& message)
{
   GuidPrefix_t prefix;
   EntityId_t entityID;
   SequenceNumberSet_t set;
	RTPSMessageCreator::addMessageAcknack(&message, prefix, prefix, entityID, entityID, set, 0, false);
}

void test_UDPv4Tests::HELPER_FillHeartbeatMessage(CDRMessage_t& message)
{
   GuidPrefix_t prefix;
   EntityId_t entityID;
   SequenceNumber_t sn1;
   SequenceNumber_t sn2;
	RTPSMessageCreator::addMessageHeartbeat(&message, prefix, entityID, entityID, sn1, sn2, 0, false, false);
}

int main(int argc, char **argv)
{
    if(const char* env_p = std::getenv("PORT_RANDOM_NUMBER"))
        g_default_port = std::stoi(env_p);

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
