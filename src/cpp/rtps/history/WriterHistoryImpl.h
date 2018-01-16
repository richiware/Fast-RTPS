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
 * @file WriterHistoryImpl.h
 *
 */

#ifndef _RTPS_HISTORY_WRITERHISTORYIMPL_H_
#define _RTPS_HISTORY_WRITERHISTORYIMPL_H_

#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/attributes/HistoryAttributes.h>

#include <mutex>
#include <map>
#include <functional>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class WriterHistory::impl
{
    public:

    using map = std::map<SequenceNumber_t, CacheChange_ptr>;
    using const_iterator = map::const_iterator;
    using iterator = map::iterator;

    impl(const HistoryAttributes& attributes);

    const HistoryAttributes& attributes() { return attributes_; }

    bool add_change(CacheChange_ptr& change);

    CacheChange_ptr remove_change(const SequenceNumber_t& sequence_number);

    CacheChange_ptr remove_change_nts(const SequenceNumber_t& sequence_number);

    /**
     * Remove the CacheChange_t with the minimum sequenceNumber.
     * @return True if correctly removed.
     */
    CacheChange_ptr remove_min_change();

    void clear();

    std::unique_lock<std::mutex> lock_for_transaction();

    SequenceNumber_t get_min_sequence_number_nts() const;

    SequenceNumber_t get_max_sequence_number_nts() const;

    const CacheChange_t* get_change_nts(const SequenceNumber_t& sequence_number) const;

    const_iterator begin_nts();

    const_iterator end_nts();

#if defined(__DEBUG)
    int get_mutex_owner() const;

    int get_thread_id() const;
#endif

    void call_after_adding_change(std::function<bool(const CacheChange_t&)>&& callback)
    {
        call_after_adding_change_ = std::move(callback);
    }

    void call_after_deleting_change(
            std::function<bool(const SequenceNumber_t&, const InstanceHandle_t&)>&& callback)
    {
        call_after_deleting_change_ = std::move(callback);
    }

    private:

    CacheChange_ptr remove_change_nts_(iterator& element);

    HistoryAttributes attributes_;

    //! Mutex that protects history.
    mutable std::mutex mutex_;

    map changes_;

    std::function<bool(const CacheChange_t&)> call_after_adding_change_;

    std::function<bool(const SequenceNumber_t&, const InstanceHandle_t&)> call_after_deleting_change_;
};

inline WriterHistory::impl& get_implementation(WriterHistory& history)
{
    return *history.impl_;
}

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima
#endif // _RTPS_HISTORY_WRITERHISTORYIMPL_H_
