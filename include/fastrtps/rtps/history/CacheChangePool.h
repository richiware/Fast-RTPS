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
 * @file CacheChangePool.h
 *
 */

#ifndef _RTPS_HISTORY_CACHECHANGEPOOL_H_
#define _RTPS_HISTORY_CACHECHANGEPOOL_H_

#include "../common/CacheChange.h"

#include <vector>
#include <mutex>

namespace eprosima {
namespace fastrtps {
namespace rtps {

/**
 * Class CacheChangePool, used by the HistoryCache to pre-reserve a number of CacheChange_t to avoid dynamically reserving memory in the middle of execution loops.
 * @ingroup COMMON_MODULE
 */
class CacheChangePool
{
    public:

        /**
         * Constructor.
         * @param pool_size The initial pool size
         * @param payload_size The initial payload size associated with the pool.
         * @param max_pool_size Maximum payload size. If set to 0 the pool will keep reserving until something breaks.
         * @param memoryPolicy Memory management policy.
         */
        CacheChangePool(int32_t pool_size, uint32_t payload_size, int32_t max_pool_size);

        virtual ~CacheChangePool();

        /*!
         * @brief Reserves a CacheChange from the pool.
         * @param chan Returned pointer to the reserved CacheChange.
         * @param calculateSizeFunc Function that returns the size of the data which will go into the CacheChange.
         * This function is executed depending on the memory management policy (DYNAMIC_RESERVE_MEMORY_MODE and
         * PREALLOCATED_WITH_REALLOC_MEMORY_MODE)
         * @return True whether the CacheChange could be allocated. In other case returns false.
         */
        CacheChange_ptr reserve_cache();

        //!Get the initial payload size associated with the Pool.
        //TODO(Ricardo) Review if necesssary
        uint32_t get_initial_payload_size() const { return payload_size_; }

    private:

        friend void CacheChangePoolDeleter::operator()(CacheChange_t*);

        //!Release a Cache back to the pool.
        void release_cache(CacheChange_t*);

        bool allocate_group_nts(uint32_t pool_size);

        uint32_t payload_size_;
        uint32_t pool_size_;
        uint32_t max_pool_size_;
        std::vector<CacheChange_t*> free_caches_;
        std::mutex mutex_;
};

}
} /* namespace rtps */
} /* namespace eprosima */

#endif /* _RTPS_HISTORY_CACHECHANGEPOOL_H_ */
