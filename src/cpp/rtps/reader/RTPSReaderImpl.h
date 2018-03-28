// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
#include <fastrtps/rtps/common/CacheChange.h>

namespace eprosima {
namespace fastrtps {
namespace rtps {

// Forward declarations
class ReaderListener;
class ReaderHistory;
class WriterProxy;
struct SequenceNumber_t;
class SequenceNumberSet_t;
class FragmentedChangePitStop;
class MatchingInfo;

/**
 * Class RTPSReader, manages the reception of data from its matched writers.
 * @ingroup READER_MODULE
 */
class RTPSReader::impl : public Endpoint
{
    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onReaderMatched(RTPSReader::impl&, const MatchingInfo&) {}

                /**
                 * This method is called when a new CacheChange_t is added to the ReaderHistory.
                 * @param reader Pointer to the reader.
                 * @param change Pointer to the CacheChange_t. This is a const pointer to const data
                 * to indicate that the user should not dispose of this data himself.
                 * To remove the data call the remove_change method of the ReaderHistory.
                 * reader->getHistory()->remove_change((CacheChange_t*)change).
                 */
                virtual void onNewCacheChangeAdded(RTPSReader::impl&, const CacheChange_t* const) {}
        };

    protected:

        impl(RTPSParticipant::impl& participant, const GUID_t& guid,
                const ReaderAttributes& att, ReaderHistory* hist, Listener* listener = nullptr);

        void deinit_();

    public:

        virtual ~impl();

        virtual bool init() = 0;

        virtual void deinit() = 0;

        /**
         * Add a matched writer represented by its attributes.
         * @param wdata Attributes of the writer to add.
         * @return True if correctly added.
         */
        virtual bool matched_writer_add(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Remove a writer represented by its attributes from the matched writers.
         * @param wdata Attributes of the writer to remove.
         * @return True if correctly removed.
         */
        virtual bool matched_writer_remove(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Tells us if a specific Writer is matched against this reader
         * @param wdata Pointer to the WriterProxyData object
         * @return True if it is matched.
         */
        virtual bool matched_writer_is_matched(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Returns true if the reader accepts a message directed to entityId.
         */
        bool acceptMsgDirectedTo(EntityId_t& entityId);

        /**
         * Processes a new DATA message. Previously the message must have been accepted by function acceptMsgDirectedTo.
         *
         * @param change Pointer to the CacheChange_t.
         * @return true if the reader accepts messages from the.
         */
        virtual bool processDataMsg(CacheChange_t *change) = 0;

        /**
         * Processes a new DATA FRAG message. Previously the message must have been accepted by function acceptMsgDirectedTo.
         *
         * @param change Pointer to the CacheChange_t.
         * @param sampleSize Size of the complete, assembled message.
         * @param fragmentStartingNum Starting number of this particular fragment.
         * @return true if the reader accepts message.
         */
        virtual bool processDataFragMsg(CacheChange_t *change, uint32_t sampleSize, uint32_t fragmentStartingNum) = 0;

        /**
         * Processes a new HEARTBEAT message. Previously the message must have been accepted by function acceptMsgDirectedTo.
         *
         * @return true if the reader accepts messages from the.
         */
        virtual bool processHeartbeatMsg(GUID_t &writerGUID, uint32_t hbCount, SequenceNumber_t &firstSN,
                SequenceNumber_t &lastSN, bool finalFlag, bool livelinessFlag) = 0;

        virtual bool processGapMsg(GUID_t &writerGUID, SequenceNumber_t &gapStart, SequenceNumberSet_t &gapList) = 0;

        /**
         * Method to indicate the reader that some change has been removed due to HistoryQos requirements.
         * @param change Pointer to the CacheChange_t.
         * @param prox Pointer to the WriterProxy.
         * @return True if correctly removed.
         */
        virtual bool change_removed_by_history(CacheChange_t* cachechange, WriterProxy* prox = nullptr) = 0;

        /**
         * Get the associated listener, secondary attached Listener in case it is of coumpound type
         * @return Pointer to the associated reader listener.
         */
        Listener* getListener();

        /**
         * Switch the ReaderListener kind for the Reader.
         * If the RTPSReader does not belong to the built-in protocols it switches out the old one.
         * If it belongs to the built-in protocols, it sets the new ReaderListener callbacks to be called after the 
         * built-in ReaderListener ones.
         * @param target Pointed to ReaderLister to attach
         * @return True is correctly set.
         */
        bool setListener(Listener* target);

        /**
         * Reserve a CacheChange_t.
         * @param change Pointer to pointer to the Cache.
         * @return True if correctly reserved.
         */
        CacheChange_ptr reserveCache();

        /**
         * Read the next unread CacheChange_t from the history
         * @param change POinter to pointer of CacheChange_t
         * @param wp Pointer to pointer to the WriterProxy
         * @return True if read.
         */
        virtual bool nextUnreadCache(CacheChange_t** change, WriterProxy** wp) = 0;

        /**
         * Get the next CacheChange_t from the history to take.
         * @param change Pointer to pointer of CacheChange_t.
         * @param wp Pointer to pointer to the WriterProxy.
         * @return True if read.
         */
        virtual bool nextUntakenCache(CacheChange_t** change, WriterProxy** wp) = 0;

        /**
         * @return True if the reader expects Inline QOS.
         */
        inline bool expectsInlineQos(){ return m_expectsInlineQos; };
        //! Returns a pointer to the associated History.
        inline ReaderHistory* getHistory() {return mp_history;};

        /*!
         * @brief Search if there is a CacheChange_t, giving SequenceNumber_t and writer GUID_t,
         * waiting to be completed because it is fragmented.
         * @param sequence_number SequenceNumber_t of the searched CacheChange_t.
         * @param writer_guid writer GUID_t of the searched CacheChange_t.
         * @return If a CacheChange_t was found, it will be returned. In other case nullptr is returned.
         */
        CacheChange_t* findCacheInFragmentedCachePitStop(const SequenceNumber_t& sequence_number,
                const GUID_t& writer_guid);

        /*!
         * @brief Returns there is a clean state with all Writers.
         * It occurs when the Reader received all samples sent by Writers. In other words,
         * its WriterProxies are up to date.
         * @return There is a clean state with all Writers.
         */
        virtual bool isInCleanState() const = 0;

        /**
         * Get the RTPS participant
         * @return Associated RTPS participant
         */
        inline RTPSParticipant::impl& participant() const { return participant_; }

        void setTrustedWriter(EntityId_t writer)
        {
            m_acceptMessagesFromUnkownWriters=false;
            m_trustedWriterEntityId = writer;
        }

        //TODO(Ricardo) Review
        //!Accept msg from unknwon writers (BE-true,RE-false)
        bool m_acceptMessagesFromUnkownWriters;

    protected:
        //!ReaderHistory
        ReaderHistory* mp_history;
        //!Listener
        Listener* mp_listener;
        //!Accept msg to unknwon readers (default=true)
        bool m_acceptMessagesToUnknownReaders;
        //!Trusted writer (for Builtin)
        EntityId_t m_trustedWriterEntityId;
        //!Expects Inline Qos.
        bool m_expectsInlineQos;

        //TODO Select one
        FragmentedChangePitStop* fragmentedChangePitStop_;

    private:

        impl& operator=(const impl&) = delete;

    public:

        /**
         * Get mutex
         * @return Associated Mutex
         */
        inline std::recursive_mutex* getMutex() const { return mp_mutex; }

    protected:

        //!Endpoint Mutex
        std::recursive_mutex* mp_mutex;
};

inline RTPSReader::impl& get_implementation(RTPSReader& reader) { return *reader.impl_; }

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif // __RTPS_READER_RTPSREADERIMPL_H__
