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

#include "FragmentedChangePitStop.h"
#include <fastrtps/rtps/common/CacheChange.h>
#include "RTPSReaderImpl.h"

using namespace eprosima::fastrtps::rtps;

CacheChange_ptr FragmentedChangePitStop::process(CacheChange_ptr& incoming_change, const uint32_t sampleSize,
        const uint32_t fragmentStartingNum)
{
    CacheChange_ptr returned_value;

    // Search CacheChange_t with the sample sequence number.
    auto range = changes_.equal_range(ChangeInPit(incoming_change->sequence_number));

    auto original_change_cit = range.first;

    // If there is a range, search the CacheChange_t with the same writer GUID_t.
    if(original_change_cit != changes_.end())
    {
        for(; original_change_cit != range.second; ++original_change_cit)
        {
            if(original_change_cit->getChange()->writer_guid == incoming_change->writer_guid)
                break;
        }
    }
    else
        original_change_cit = range.second;

    // If not found an existing CacheChange_t, reserve one and insert.
    if(original_change_cit == range.second)
    {
        CacheChange_ptr original_change = parent_.reserveCache();

        if(!original_change)
        {
            return returned_value;
        }

        if(original_change->serialized_payload.max_size < sampleSize)
        {
            return returned_value;
        }

        //Change comes preallocated (size sampleSize)
        original_change->copy_not_memcpy(*incoming_change);
        // The length of the serialized payload has to be sample size.
        original_change->serialized_payload.length = sampleSize;
        original_change->setFragmentSize(incoming_change->getFragmentSize());

        // Insert
        original_change_cit = changes_.insert(ChangeInPit(std::move(original_change)));
    }

    bool was_updated = false;
    for (uint32_t count = (fragmentStartingNum - 1); count < (fragmentStartingNum - 1) + incoming_change->getFragmentCount(); ++count)
    {
        if(original_change_cit->getChange()->getDataFragments()->at(count) == ChangeFragmentStatus_t::NOT_PRESENT)
        {
            // All cases minus last fragment.
            if (count + 1 != original_change_cit->getChange()->getFragmentCount())
            {
                memcpy(original_change_cit->getChange()->serialized_payload.data + count * original_change_cit->getChange()->getFragmentSize(),
                        incoming_change->serialized_payload.data + (count - (fragmentStartingNum - 1)) * incoming_change->getFragmentSize(), incoming_change->getFragmentSize());
            }
            // Last fragment is a special case when copying.
            else
            {
                memcpy(original_change_cit->getChange()->serialized_payload.data + count * original_change_cit->getChange()->getFragmentSize(),
                        incoming_change->serialized_payload.data + (count - (fragmentStartingNum - 1)) * incoming_change->getFragmentSize(), original_change_cit->getChange()->serialized_payload.length - (count * original_change_cit->getChange()->getFragmentSize()));
            }

            original_change_cit->getChange()->getDataFragments()->at(count) = ChangeFragmentStatus_t::PRESENT;

            was_updated = true;
        }
    }

    // If was updated, check if it is completed.
    if(was_updated)
    {
        auto fit = original_change_cit->getChange()->getDataFragments()->begin();
        for(; fit != original_change_cit->getChange()->getDataFragments()->end(); ++fit)
        {
            if(*fit == ChangeFragmentStatus_t::NOT_PRESENT)
                break;
        }

        // If it is completed, return CacheChange_t and remove information.
        if(fit == original_change_cit->getChange()->getDataFragments()->end())
        {
            returned_value = original_change_cit->release();
            changes_.erase(original_change_cit);
        }
    }

    return returned_value;
}

//TODO(Ricardo) Por lo que he visto usada en heartbeat a vers si hay que enviar heartbeat frag.
//No me gusta que devuelva punter. modificar.
CacheChange_t* FragmentedChangePitStop::find(const SequenceNumber_t& sequence_number, const GUID_t& writer_guid)
{
    CacheChange_t* returnedValue = nullptr;

    auto range = changes_.equal_range(ChangeInPit(sequence_number));

    auto cit = range.first;

    // If there is a range, search the CacheChange_t with the same writer GUID_t.
    if(cit != changes_.end())
    {
        for(; cit != range.second; ++cit)
        {
            if(cit->getChange()->writer_guid == writer_guid)
            {
                returnedValue = &*cit->getChange();
                break;
            }
        }
    }

    return returnedValue;
}

bool FragmentedChangePitStop::try_to_remove(const SequenceNumber_t& sequence_number, const GUID_t& writer_guid)
{
    bool returnedValue = false;

    auto range = changes_.equal_range(ChangeInPit(sequence_number));

    auto cit = range.first;

    // If there is a range, search the CacheChange_t with the same writer GUID_t.
    if(cit != changes_.end())
    {
        for(; cit != range.second; ++cit)
        {
            if(cit->getChange()->writer_guid == writer_guid)
            {
                // Destroy CacheChange_t.
                CacheChange_ptr to_remove(cit->release());
                changes_.erase(cit);
                returnedValue = true;
                break;
            }
        }
    }

    return returnedValue;
}

bool FragmentedChangePitStop::try_to_remove_until(const SequenceNumber_t& sequence_number, const GUID_t& writer_guid)
{
    bool returnedValue = false;

    auto cit = changes_.begin();
    while(cit != changes_.end())
    {
        if(cit->getChange()->sequence_number < sequence_number &&
                cit->getChange()->writer_guid == writer_guid)
        {
            // Destroy CacheChange_t.
            CacheChange_ptr to_remove(cit->release());
            cit = changes_.erase(cit);
            returnedValue = true;
        }
        else
        {
            ++cit;
        }
    }

    return returnedValue;
}
