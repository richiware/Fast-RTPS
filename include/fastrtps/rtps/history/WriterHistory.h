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
 *
 */

#ifndef __RTPS_HISTORY_WRITERHISTORY_H__
#define __RTPS_HISTORY_WRITERHISTORY_H__

#include "../../fastrtps_dll.h"
#include "CacheChangePool.h"

#include <memory>
// C++14 #include <experimental/propagate_const>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class HistoryAttributes;
struct SequenceNumber_t;

/**
 * Class WriterHistory, container of the different CacheChanges of a writer
 * @ingroup WRITER_MODULE
 */
class WriterHistory
{
    public:

    class impl;

    /**
     * Constructor of the WriterHistory.
     */
    RTPS_DllAPI WriterHistory(const HistoryAttributes& attributes);

    RTPS_DllAPI ~WriterHistory();

    /**
     * Add a CacheChange_t to the ReaderHistory.
     * @param a_change Pointer to the CacheChange to add.
     * @return True if added.
     */
    RTPS_DllAPI SequenceNumber_t add_change(CacheChange_ptr& change);

    RTPS_DllAPI CacheChange_ptr remove_change(const SequenceNumber_t& sequence_number);

    /**
     * Remove the CacheChange_t with the minimum sequenceNumber.
     * @return True if correctly removed.
     */
    RTPS_DllAPI CacheChange_ptr remove_min_change();

    private:

    friend impl& get_implementation(WriterHistory& history);

    //C++14 std::experimental::propagate_const<std::unique_ptr<impl::WriterHistory>> impl_;
    std::unique_ptr<WriterHistory::impl> impl_;
};

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif // __RTPS_HISTORY_WRITERHISTORY_H__
