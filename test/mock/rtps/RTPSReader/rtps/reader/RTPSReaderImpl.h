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
 * @file RTPSReaderImpl.h
 */

#ifndef __RTPS_READER_RTPSREADERIMPL_H__
#define __RTPS_READER_RTPSREADERIMPL_H__

#include <fastrtps/rtps/reader/RTPSReader.h>
#include <fastrtps/rtps/Endpoint.h>
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/rtps/common/MatchingInfo.h>

#include <gmock/gmock.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

class RemoteWriterAttributes;

class RTPSReader::impl : public Endpoint
{
    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onReaderMatched(RTPSReader::impl&, const MatchingInfo&) {}

                virtual void onNewCacheChangeAdded(RTPSReader::impl&, const CacheChange_t* const) {}
        };

        virtual ~impl() = default;

        MOCK_METHOD1(matched_writer_add, bool(RemoteWriterAttributes&));

        MOCK_METHOD1(matched_writer_remove, bool(RemoteWriterAttributes&));

        ReaderHistory* getHistory()
        {
            return history_;
        }

        ReaderHistory* history_;

        RTPSReader::impl::Listener* listener_;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // __RTPS_READER_RTPSREADERIMPL_H__
