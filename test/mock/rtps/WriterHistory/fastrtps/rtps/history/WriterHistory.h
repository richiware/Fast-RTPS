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
 * @file WriterHistory.h
 */

#ifndef _RTPS_HISTORY_WRITERHISTORY_H_
#define _RTPS_HISTORY_WRITERHISTORY_H_

#include <fastrtps/rtps/attributes/HistoryAttributes.h>
#include <fastrtps/rtps/history/CacheChangePool.h>

#include <condition_variable>
#include <gmock/gmock.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

class WriterHistory
{
    public:


        WriterHistory(const HistoryAttributes& /*att*/) : samples_number_(0) {}

        MOCK_METHOD1(add_change_mock, bool(CacheChange_ptr&));

        bool add_change(CacheChange_ptr& change)
        {
            bool ret = add_change_mock(change);
            samples_number_mutex_.lock();
            ++samples_number_;
            change->sequence_number = ++last_sequence_number_;
            samples_number_mutex_.unlock();
            samples_number_cond_.notify_all();
            return ret;
        }

        MOCK_METHOD1(remove_change, CacheChange_ptr(const SequenceNumber_t&));

        void wait_for_more_samples_than(unsigned int minimum)
        {
            std::unique_lock<std::mutex> lock(samples_number_mutex_);

            if(samples_number_ <= minimum)
            {
                samples_number_cond_.wait(lock, [&]() {return samples_number_ > minimum;});
            }
        }

    private:

        std::condition_variable samples_number_cond_;
        std::mutex samples_number_mutex_;
        unsigned int samples_number_;
        SequenceNumber_t last_sequence_number_;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // _RTPS_HISTORY_WRITERHISTORY_H_
