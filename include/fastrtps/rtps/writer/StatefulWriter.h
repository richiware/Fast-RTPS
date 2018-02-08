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
 * @file StatefulWriter.h
 *
 */

#ifndef __RTPS_WRITER_STATEFULWRITER_H__
#define __RTPS_WRITER_STATEFULWRITER_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "RTPSWriter.h"

namespace eprosima {
namespace fastrtps {
namespace rtps {

/**
 * Class StatefulWriter, specialization of RTPSWriter that maintains information of each matched Reader.
 * @ingroup WRITER_MODULE
 */
class StatefulWriter : public RTPSWriter
{
    public:

        class impl;

        //!Constructor
        StatefulWriter(RTPSParticipant& participant, const WriterAttributes& att, WriterHistory& hist,
                WriterListener* listen = nullptr);

        //!Destructor
        virtual ~StatefulWriter() = default;

        /**
         * Add a matched reader.
         * @param ratt Attributes of the reader to add.
         * @return True if added.
         */
        bool matched_reader_add(const RemoteReaderAttributes& ratt);
        /**
         * Remove a matched reader.
         * @param ratt Attributes of the reader to remove.
         * @return True if removed.
         */
        bool matched_reader_remove(const RemoteReaderAttributes& ratt);
        /**
         * Tells us if a specific Reader is matched against this writer
         * @param ratt Attributes of the reader to remove.
         * @return True if it was matched.
         */
        bool matched_reader_is_matched(const RemoteReaderAttributes& ratt);

        bool wait_for_all_acked(const Duration_t& max_wait);

        /**
         * Update the Attributes of the Writer.
         * @param att New attributes
         */
        void updateAttributes(WriterAttributes& att);

    private:

        StatefulWriter& operator=(const StatefulWriter&) = delete;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif
#endif //__RTPS_WRITER_STATEFULWRITER_H__
