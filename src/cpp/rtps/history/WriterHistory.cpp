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
 * @file WriterHistory.cpp
 *
 */

#include "WriterHistoryImpl.h"
#include <fastrtps/rtps/writer/RTPSWriter.h>
#include <fastrtps/log/Log.h>

#include <mutex>

#if defined(__DEBUG)
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#endif

namespace eprosima {
namespace fastrtps{
namespace rtps {

typedef std::pair<InstanceHandle_t,std::vector<CacheChange_t*>> t_pairKeyChanges;
typedef std::vector<t_pairKeyChanges> t_vectorPairKeyChanges;

WriterHistory::WriterHistory(const HistoryAttributes& attributes) :
    impl_(new WriterHistory::impl(attributes))
{
}

WriterHistory::~WriterHistory() = default;

WriterHistory::impl::impl(const HistoryAttributes& attributes): attributes_(attributes)
{
}

SequenceNumber_t WriterHistory::add_change(CacheChange_ptr& change)
{
    return impl_->add_change(change);
}

SequenceNumber_t WriterHistory::impl::add_change(CacheChange_ptr& change)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return add_change_nts(change);
}

SequenceNumber_t WriterHistory::impl::add_change_nts(CacheChange_ptr& change)
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    assert(change);

    if((attributes_.memoryPolicy == PREALLOCATED_MEMORY_MODE) &&
            change->serialized_payload.length > attributes_.payloadMaxSize)
    {
        logError(RTPS_HISTORY,
                "Change payload size of '" << change->serialized_payload.length <<
                "' bytes is larger than the history payload size of '" << attributes_.payloadMaxSize <<
                "' bytes and cannot be resized.");
        return SequenceNumber_t::unknown();
    }

    bool add_permitted = true;
    change->sequence_number = ++last_cachechange_seqnum_;

    if(call_after_adding_change_)
    {
        add_permitted = call_after_adding_change_(*change);
    }

    if(add_permitted)
    {
        logInfo(RTPS_HISTORY, "Change " << change->sequence_number << " added with "
                << change->serialized_payload.length << " bytes");

        changes_.emplace_hint(changes_.end(), change->sequence_number, std::move(change));

        return last_cachechange_seqnum_;
    }

    --last_cachechange_seqnum_;
    return SequenceNumber_t::unknown();
}

CacheChange_ptr WriterHistory::remove_change(const SequenceNumber_t& sequence_number)
{
    return impl_->remove_change(sequence_number);
}

CacheChange_ptr WriterHistory::impl::remove_change(const SequenceNumber_t& sequence_number)
{
    std::unique_lock<std::mutex> lock(mutex_);
    iterator change_it = changes_.find(sequence_number);
    return remove_change_nts_(change_it);
}

CacheChange_ptr WriterHistory::impl::remove_change_nts(const SequenceNumber_t& sequence_number)
{
    iterator change_it = changes_.find(sequence_number);
    return remove_change_nts_(change_it);
}

CacheChange_ptr WriterHistory::impl::remove_change_nts_(iterator& element)
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    if(element != changes_.end())
    {
        bool remove_permitted = true;

        if(call_after_deleting_change_)
        {
            remove_permitted = call_after_deleting_change_(element->first, element->second->instance_handle);
        }

        if(remove_permitted)
        {
            CacheChange_ptr change_to_remove(std::move(element->second));
            assert(change_to_remove);
            changes_.erase(element);
            return change_to_remove;
        }
    }

    return CacheChange_ptr();
}

CacheChange_ptr WriterHistory::remove_min_change()
{
    return impl_->remove_min_change();
}

CacheChange_ptr WriterHistory::impl::remove_min_change()
{
    std::unique_lock<std::mutex> lock(mutex_);
    auto change_it = changes_.begin();
    return remove_change_nts_(change_it);
}

void WriterHistory::impl::clear()
{
    std::unique_lock<std::mutex> lock(mutex_);
    changes_.clear();
}

SequenceNumber_t WriterHistory::impl::get_min_sequence_number_nts() const
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    if(changes_.size() > 0)
    {
        return changes_.begin()->first;
    }

    return SequenceNumber_t::unknown();
}

SequenceNumber_t WriterHistory::impl::get_max_sequence_number_nts() const
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    if(changes_.size() > 0)
    {
        return changes_.rbegin()->first;
    }

    return SequenceNumber_t::unknown();
}

/*
 * This function has to be used having WriterHistory's mutex lock.
 * On debug it check this precondition.
 */
const CacheChange_t* WriterHistory::impl::get_change_nts(const SequenceNumber_t& sequence_number) const
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    CacheChange_t* cachechange = nullptr;
    auto cachechange_it = changes_.find(sequence_number);

    if(cachechange_it != changes_.end())
    {
        cachechange = &*cachechange_it->second;
    }

    return cachechange;
}

WriterHistory::impl::const_iterator WriterHistory::impl::begin_nts()
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    return changes_.cbegin();
}

WriterHistory::impl::const_iterator WriterHistory::impl::end_nts()
{
#if defined(__DEBUG)
    assert(get_mutex_owner() == get_thread_id());
#endif

    return changes_.cend();
}

std::unique_lock<std::mutex> WriterHistory::impl::lock_for_transaction()
{
    return std::unique_lock<std::mutex>(mutex_);
}

#if defined(__DEBUG)
int WriterHistory::impl::get_mutex_owner() const
{
    auto mutex = mutex_.native_handle();
    return mutex->__data.__owner;
}

int WriterHistory::impl::get_thread_id() const
{
    return syscall(__NR_gettid);
}
#endif

SequenceNumber_t WriterHistory::impl::next_sequence_number_nts() const
{
    return last_cachechange_seqnum_ + 1;
}

} /* namespace rtps */
} /* namespace fastrtps */
} /* namespace eprosima */
