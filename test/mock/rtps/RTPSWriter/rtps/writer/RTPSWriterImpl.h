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
 * @file RTPSWriterImpl.h
 */

#ifndef __RTPS_WRITER_RTPSWRITERIMPL_H__
#define __RTPS_WRITER_RTPSWRITERIMPL_H__

#include <fastrtps/rtps/writer/RTPSWriter.h>
#include <fastrtps/rtps/attributes/WriterAttributes.h>
#include <fastrtps/rtps/Endpoint.h>
#include <fastrtps/rtps/history/CacheChangePool.h>
#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/common/MatchingInfo.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

class RTPSWriter::impl : public Endpoint
{
    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onWriterMatched(RTPSWriter::impl&, const MatchingInfo&) {}
        };

        virtual ~impl() = default;

        MOCK_METHOD1(matched_reader_add, bool(RemoteReaderAttributes&));

        MOCK_METHOD1(matched_reader_remove, bool(RemoteReaderAttributes&));

        MOCK_METHOD2(new_change, CacheChange_ptr(ChangeKind_t, const InstanceHandle_t&));

        WriterHistory::impl* history_;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // __RTPS_WRITER_RTPSWRITERIMPL_H__
