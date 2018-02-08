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
 * @file StatelessReader.h
 */


#ifndef __RTPS_READER_STATELESSREADER_H__
#define __RTPS_READER_STATELESSREADER_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "RTPSReader.h"

namespace eprosima {
namespace fastrtps{
namespace rtps {

/**
 * Class StatelessReader, specialization of the RTPSReader for Best Effort Readers.
 * @ingroup READER_MODULE
 */
class StatelessReader: public RTPSReader
{
    public:

        class impl;

        StatelessReader(RTPSParticipant&, const ReaderAttributes& att, ReaderHistory* hist,
                ReaderListener* listen = nullptr);

        virtual ~StatelessReader() = default;

        /**
         * Add a matched writer represented by a WriterProxyData object.
         * @param wdata Pointer to the WPD object to add.
         * @return True if correctly added.
         */
        bool matched_writer_add(const RemoteWriterAttributes& wdata);
        /**
         * Remove a WriterProxyData from the matached writers.
         * @param wdata Pointer to the WPD object.
         * @return True if correct.
         */
        bool matched_writer_remove(const RemoteWriterAttributes& wdata);

        /**
         * Tells us if a specific Writer is matched against this reader
         * @param wdata Pointer to the WriterProxyData object
         * @return True if it is matched.
         */
        bool matched_writer_is_matched(const RemoteWriterAttributes& wdata);

        /**
         * Read the next unread CacheChange_t from the history
         * @param change Pointer to pointer of CacheChange_t
         * @param wpout Pointer to pointer of the matched writer proxy
         * @return True if read.
         */
        bool nextUnreadCache(CacheChange_t** change,WriterProxy** wpout=nullptr);
        /**
         * Take the next CacheChange_t from the history;
         * @param change Pointer to pointer of CacheChange_t
         * @param wpout Pointer to pointer of the matched writer proxy
         * @return True if read.
         */
        bool nextUntakenCache(CacheChange_t** change,WriterProxy** wpout=nullptr);

        /*!
         * @brief Returns there is a clean state with all Writers.
         * StatelessReader allways return true;
         * @return true
         */
        bool isInCleanState() const { return true; }

        private:

        StatelessReader(std::shared_ptr<RTPSReader::impl>& impl, ReaderListener* listener = nullptr);

        friend StatelessReader* create_statelessreader_from_implementation(std::shared_ptr<RTPSReader::impl>&);
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif
#endif /* __RTPS_READER_STATELESSREADER_H__*/
