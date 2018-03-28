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

#include <fastrtps/rtps/writer/ReaderLocator.h>

#include <gtest/gtest.h>

// Class to test
#include <rtps/flowcontrol/ThroughputController.cpp>

using namespace std;
using namespace eprosima::fastrtps::rtps;

static const unsigned int testPayloadSize = 1000;
static const unsigned int controllerSize = 5500;
static const unsigned int periodMillisecs = 100;
static const unsigned int numberOfTestChanges = 10;

static const ThroughputControllerDescriptor testDescriptor = {controllerSize, periodMillisecs};

class ThroughputControllerTests: public ::testing::Test
{
   public:

   ThroughputControllerTests():
      sController(testDescriptor, (const RTPSWriter::impl*)nullptr)
   {
      for (unsigned int i = 0; i < numberOfTestChanges; i++)
      {
         testChanges.emplace_back(new CacheChange_t(testPayloadSize));
         testChanges.back()->sequence_number = {0, i+1};
         testChanges.back()->serialized_payload.length = testPayloadSize;
         testChangesForUse.add_change(testChanges.back().get(), &mock, FragmentNumberSet_t());

         otherChanges.emplace_back(new CacheChange_t(testPayloadSize));
         otherChanges.back()->sequence_number = {0, i+1};
         otherChanges.back()->serialized_payload.length = testPayloadSize;
         otherChangesForUse.add_change(otherChanges.back().get(), &mock, FragmentNumberSet_t());
      }
   }

   ThroughputController sController;
   ReaderLocator mock;
   std::vector<std::unique_ptr<CacheChange_t>> testChanges;
   std::vector<std::unique_ptr<CacheChange_t>> otherChanges;
   RTPSWriterCollector<ReaderLocator*> testChangesForUse;
   RTPSWriterCollector<ReaderLocator*> otherChangesForUse;
};

TEST_F(ThroughputControllerTests, throughput_controller_lets_only_some_elements_through)
{
   // When
   sController(testChangesForUse);

   // Then
   ASSERT_EQ(controllerSize/testPayloadSize, testChangesForUse.size());

   std::this_thread::sleep_for(std::chrono::milliseconds(periodMillisecs + 50));
}

TEST_F(ThroughputControllerTests, if_changes_are_fragmented_throughput_controller_provides_granularity)
{
    // Given fragmented changes
    testChangesForUse.clear();

    std::set<FragmentNumber_t> fragmentSet;
    for(uint32_t i = 1; i <= 10; i++)
        fragmentSet.insert(i);

    for(auto& change : testChanges)
    {
        change->setFragmentSize(100);
        testChangesForUse.add_change(change.get(), &mock, fragmentSet);
    }

    // When
    sController(testChangesForUse);

    // Then
    // The first 5 are completely cleared
    // And the last one is partially cleared
    ASSERT_EQ(55, testChangesForUse.size());

    SequenceNumber_t seqNum(0, 1);
    FragmentNumber_t fragNum = 1;

    while(!testChangesForUse.empty())
    {
        RTPSWriterCollector<ReaderLocator*>::Item item = testChangesForUse.pop();
        ASSERT_EQ(item.sequence_number, seqNum);
        ASSERT_EQ(item.fragment_number, fragNum);

        if(++fragNum > 10)
        {
            ++seqNum;
            fragNum = 1;
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(periodMillisecs + 50));
}

TEST_F(ThroughputControllerTests, throughput_controller_carries_over_multiple_attempts)
{
   // Given
   sController(testChangesForUse);

   // when
   sController(otherChangesForUse);

   // Then
   ASSERT_EQ(0, otherChangesForUse.size());
   std::this_thread::sleep_for(std::chrono::milliseconds(periodMillisecs + 50));
}

TEST_F(ThroughputControllerTests, throughput_controller_resets_completely_after_its_refresh_period)
{
   // Given
   sController(testChangesForUse);
   ASSERT_EQ(5, testChangesForUse.size());

   // The controller is now fully closed, so controllering anything will throw all changes away.
   sController(testChangesForUse);
   ASSERT_EQ(0, testChangesForUse.size());

   // When
   std::this_thread::sleep_for(std::chrono::milliseconds(periodMillisecs + 100));

   // The controller should be open now
   sController(otherChangesForUse);
   EXPECT_EQ(5, otherChangesForUse.size());
   std::this_thread::sleep_for(std::chrono::milliseconds(periodMillisecs + 50));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
