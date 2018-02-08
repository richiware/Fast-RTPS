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

#ifndef _RTPS_RESOURCES_ASYNC_INTEREST_TREE_H_
#define _RTPS_RESOURCES_ASYNC_INTEREST_TREE_H_

#include "../writer/RTPSWriter.h"
#include "../participant/RTPSParticipant.h"

#include <mutex>
#include <set>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class AsyncInterestTree
{
    public:

        AsyncInterestTree();
        /**
         * Registers a writer in a hidden set.
         * Threadsafe thanks to set swap.
         */
        void RegisterInterest(const RTPSWriter::impl&);

        /**
         * Registers all writers from  participant in a hidden set.
         * Threadsafe thanks to set swap.
         */
        void RegisterInterest(const RTPSParticipant::impl&);

        /**
         * Clears the visible set and swaps
         * with the hidden set.
         */
        void Swap();

        //! Extracts from the visible set 
        std::set<const RTPSWriter::impl*> GetInterestedWriters() const;

    private:
        std::set<const RTPSWriter::impl*> mInterestAlpha, mInterestBeta;
        mutable std::mutex mMutexActive, mMutexHidden;

        std::set<const RTPSWriter::impl*>* mActiveInterest;
        std::set<const RTPSWriter::impl*>* mHiddenInterest;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // _RTPS_RESOURCES_ASYNC_INTEREST_TREE_H_
