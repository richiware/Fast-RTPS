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
 * @file RTPSWriterCollector.h
 */

#ifndef _RTPS_WRITER_RTPSWRITERCOLLECTOR_H_
#define _RTPS_WRITER_RTPSWRITERCOLLECTOR_H_

#include <fastrtps/rtps/common/SequenceNumber.h>
#include <fastrtps/rtps/common/FragmentNumber.h>
#include <fastrtps/rtps/common/CacheChange.h>

#include <vector>
#include <cassert>

namespace eprosima {
namespace fastrtps {
namespace rtps {

template<class T>
class RTPSWriterCollector
{
    public:

        struct Item
        {
            Item(const SequenceNumber_t& seq_num, const FragmentNumber_t& frag_num,
                    const CacheChange_t* const change) : sequence_number(seq_num),
                                        fragment_number(frag_num),
                                        cachechange(change)

            {
                assert(seq_num == change->sequence_number);
            }

            //! Sequence number of the CacheChange.
            SequenceNumber_t sequence_number;
            /*!
             *  Fragment number of the represented fragment.
             *  If value is zero, it represents a whole change.
             */
            FragmentNumber_t fragment_number;

            const CacheChange_t* const cachechange;

            mutable std::vector<T> remote_readers;
        };

        struct ItemCmp
        {
            bool operator()(const Item& a, const Item& b) const
            {
                if(a.sequence_number < b.sequence_number)
                    return true;
                else if(a.sequence_number == b.sequence_number)
                    if(a.fragment_number < b.fragment_number)
                        return true;

                return false;
            }
        };

        typedef std::set<Item, ItemCmp> ItemSet;

        void add_change(const CacheChange_t* const change, const T& remote_reader, const FragmentNumberSet_t opt_fragment_not_sent)
        {
            if(change->getFragmentSize() > 0)
            {
                for(auto sn = opt_fragment_not_sent.get_begin(); sn != opt_fragment_not_sent.get_end(); ++sn)
                {
                    assert(*sn <= change->getDataFragments()->size());
                    auto it = items_.emplace(change->sequence_number, *sn, change);
                    it.first->remote_readers.push_back(remote_reader);
                }
            }
            else
            {
                auto it = items_.emplace(change->sequence_number, 0, change);
                it.first->remote_readers.push_back(remote_reader);
            }
        }

        bool empty()
        {
            return items_.empty();
        }

        size_t size()
        {
            return items_.size();
        }

        Item pop()
        {
            auto it = items_.begin();
            Item ret = *it;
            items_.erase(it);
            return ret;
        }

        void clear()
        {
            return items_.clear();
        }

        ItemSet& items()
        {
            return items_;
        }

    private:

        ItemSet items_;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // _RTPS_WRITER_RTPSWRITERCOLLECTOR_H_
