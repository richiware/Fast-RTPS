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
 * @file StatefulReader.h
 */

#ifndef __RTPS_READER_STATEFULREADER_H__
#define __RTPS_READER_STATEFULREADER_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "RTPSReader.h"

namespace eprosima {
namespace fastrtps{
namespace rtps {

class WriterProxy;
struct GUID_t;

/**
 * Class StatefulReader, specialization of RTPSReader than stores the state of the matched writers.
 * @ingroup READER_MODULE
 */
class StatefulReader : public RTPSReader
{
    public:

        class impl;

        StatefulReader(RTPSParticipant& participant, const ReaderAttributes& att, ReaderHistory* hist,
                ReaderListener* listener = nullptr);

        virtual ~StatefulReader() = default;

        /**
         * Add a matched writer represented by a WriterProxyData object.
         * @param wdata Pointer to the WPD object to add.
         * @return True if correctly added.
         */
        bool matched_writer_add(const RemoteWriterAttributes& wdata);
        /**
         * Remove a WriterProxyData from the matached writers.
         * @param wdata Pointer to the WPD object.
         * @param deleteWP If the Reader has to delete the associated WP object or not.
         * @return True if correct.
         */
        bool matched_writer_remove(const RemoteWriterAttributes& wdata,bool deleteWP);
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
         * Look for a specific WriterProxy.
         * @param writerGUID GUID_t of the writer we are looking for.
         * @param WP Pointer to pointer to a WriterProxy.
         * @return True if found.
         */
        //TODO (Ricardo) Review here in api
        bool matched_writer_lookup(const GUID_t& writerGUID, WriterProxy** WP);

        /**
         * Read the next unread CacheChange_t from the history
         * @param change Pointer to pointer of CacheChange_t
         * @param wpout Pointer to pointer the matched writer proxy
         * @return True if read.
         */
        bool nextUnreadCache(CacheChange_t** change,WriterProxy** wpout=nullptr);

        /**
         * Take the next CacheChange_t from the history;
         * @param change Pointer to pointer of CacheChange_t
         * @param wpout Pointer to pointer the matched writer proxy
         * @return True if read.
         */
        bool nextUntakenCache(CacheChange_t** change,WriterProxy** wpout=nullptr);

        /*!
         * @brief Returns there is a clean state with all Writers.
         * It occurs when the Reader received all samples sent by Writers. In other words,
         * its WriterProxies are up to date.
         * @return There is a clean state with all Writers.
         */
        bool isInCleanState() const;


    protected:

        StatefulReader(std::shared_ptr<RTPSReader::impl>& impl, ReaderListener* listener = nullptr);

    private:

        friend StatefulReader* create_statefulreader_from_implementation(std::shared_ptr<RTPSReader::impl>&);
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif
#endif // __RTPS_READER_STATEFULREADER_H__*/
