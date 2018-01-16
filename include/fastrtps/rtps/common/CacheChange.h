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
 * @file CacheChange.h
 */

#ifndef CACHECHANGE_H_
#define CACHECHANGE_H_

#include "Types.h"
#include "WriteParams.h"
#include "SerializedPayload.h"
#include "Time_t.h"
#include "InstanceHandle.h"
#include <fastrtps/rtps/common/FragmentNumber.h>

#include <vector>

namespace eprosima
{
    namespace fastrtps
    {
        namespace rtps
        {
            /**
             * @enum ChangeKind_t, different types of CacheChange_t.
             * @ingroup COMMON_MODULE
             */
#if defined(_WIN32)
            enum RTPS_DllAPI ChangeKind_t{
#else
            enum ChangeKind_t{
#endif
                ALIVE,                //!< ALIVE
                NOT_ALIVE_DISPOSED,   //!< NOT_ALIVE_DISPOSED
                NOT_ALIVE_UNREGISTERED,//!< NOT_ALIVE_UNREGISTERED
                NOT_ALIVE_DISPOSED_UNREGISTERED //!<NOT_ALIVE_DISPOSED_UNREGISTERED
            };

            enum ChangeFragmentStatus_t
            {
                NOT_PRESENT = 0,
                PRESENT = 1
            };

            /**
             * Structure CacheChange_t, contains information on a specific CacheChange.
             * @ingroup COMMON_MODULE
             */
            struct RTPS_DllAPI CacheChange_t
            {
                friend struct CacheChangeCmp;

                //!Kind of change, default value ALIVE.
                ChangeKind_t kind;
                //!GUID_t of the writer that generated this change.
                GUID_t writer_guid;
                //!Handle of the data associated wiht this change.
                InstanceHandle_t instance_handle;
                //!SequenceNumber of the change
                SequenceNumber_t sequence_number;
                //!Serialized Payload associated with the change.
                SerializedPayload_t serialized_payload;
                //!Indicates if the cache has been read (only used in READERS)
                bool is_read;
                //!Source TimeStamp (only used in Readers)
                Time_t source_timestamp;

                WriteParams write_params;

                bool is_untyped_;

                /*!
                 * @brief Default constructor.
                 * Creates an empty CacheChange_t.
                 */
                CacheChange_t():
                    kind(ALIVE),
                    is_read(false),
                    is_untyped_(true),
                    dataFragments_(new std::vector<uint32_t>()),
                    fragment_size_(0)
                {
                }

                /**
                 * Constructor with payload size
                 * @param payload_size Serialized payload size
                 */
                // TODO Check pass uint32_t to serializedPayload that needs int16_t.
                CacheChange_t(uint32_t payload_size, bool is_untyped = false):
                    kind(ALIVE),
                    serialized_payload(payload_size),
                    is_read(false),
                    is_untyped_(is_untyped),
                    dataFragments_(new std::vector<uint32_t>()),
                    fragment_size_(0)
                {
                }

                /*!
                 * Copy a different change into this one. All the elements are copied, included the data, allocating new memory.
                 * @param[in] ch_ptr Pointer to the change.
                 * @return True if correct.
                 */
                bool copy(const CacheChange_t& cachechange)
                {
                    kind = cachechange.kind;
                    writer_guid = cachechange.writer_guid;
                    instance_handle = cachechange.instance_handle;
                    sequence_number = cachechange.sequence_number;
                    source_timestamp = cachechange.source_timestamp;
                    write_params = cachechange.write_params;

                    bool ret = serialized_payload.copy(&cachechange.serialized_payload, (cachechange.is_untyped_ ? false : true));

                    setFragmentSize(cachechange.fragment_size_);
                    dataFragments_->assign(cachechange.dataFragments_->begin(), cachechange.dataFragments_->end());

                    is_read = cachechange.is_read;

                    return ret;
                }

                void copy_not_memcpy(const CacheChange_t& cachechange)
                {
                    kind = cachechange.kind;
                    writer_guid = cachechange.writer_guid;
                    instance_handle = cachechange.instance_handle;
                    sequence_number = cachechange.sequence_number;
                    source_timestamp = cachechange.source_timestamp;
                    write_params = cachechange.write_params;

                    // Copy certain values from serializedPayload
                    serialized_payload.encapsulation = cachechange.serialized_payload.encapsulation;

                    setFragmentSize(cachechange.fragment_size_);
                    dataFragments_->assign(cachechange.dataFragments_->begin(), cachechange.dataFragments_->end());

                    is_read = cachechange.is_read;
                }

                ~CacheChange_t()
                {
                    if (dataFragments_)
                        delete dataFragments_;
                }

                uint32_t getFragmentCount() const
                { 
                    return (uint32_t)dataFragments_->size();
                }

                std::vector<uint32_t>* getDataFragments() const { return dataFragments_; }

                uint16_t getFragmentSize() const { return fragment_size_; }

                void setFragmentSize(uint16_t fragment_size)
                {
                    this->fragment_size_ = fragment_size;

                    if (fragment_size == 0) {
                        dataFragments_->clear();
                    } 
                    else
                    {
                        //TODO Mirar si cuando se compatibilice con RTI funciona el calculo, porque ellos
                        //en el sampleSize incluyen el padding.
                        uint32_t size = (serialized_payload.length + fragment_size - 1) / fragment_size;
                        dataFragments_->assign(size, ChangeFragmentStatus_t::NOT_PRESENT);
                    }
                }


                private:

                CacheChange_t(const CacheChange_t&) = delete;
                const CacheChange_t& operator=(const CacheChange_t&) = delete;

                // Data fragments
                std::vector<uint32_t>* dataFragments_;

                // Fragment size
                uint16_t fragment_size_;
            };

#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

            /**
             * Enum ChangeForReaderStatus_t, possible states for a CacheChange_t in a ReaderProxy.
             *  @ingroup COMMON_MODULE
             */
            enum ChangeForReaderStatus_t{
                UNSENT = 0,        //!< UNSENT
                UNACKNOWLEDGED = 1,//!< UNACKNOWLEDGED
                REQUESTED = 2,     //!< REQUESTED
                ACKNOWLEDGED = 3,  //!< ACKNOWLEDGED
                UNDERWAY = 4       //!< UNDERWAY
            };

            /**
             * Enum ChangeFromWriterStatus_t, possible states for a CacheChange_t in a WriterProxy.
             *  @ingroup COMMON_MODULE
             */
            enum ChangeFromWriterStatus_t{
                UNKNOWN = 0,
                MISSING = 1,
                //REQUESTED_WITH_NACK,
                RECEIVED = 2,
                LOST = 3
            };

            /**
             * Struct ChangeForReader_t used to represent the state of a specific change with respect to a specific reader, as well as its relevance.
             *  @ingroup COMMON_MODULE
             */
            class ChangeForReader_t
            {
                friend struct ChangeForReaderCmp;

                public:

                ChangeForReader_t() = delete;

                ChangeForReader_t(const ChangeForReader_t& ch) : status_(ch.status_),
                is_relevant_(ch.is_relevant_), seq_num_(ch.seq_num_), unsent_fragments_(ch.unsent_fragments_),
                num_fragments_(ch.num_fragments_)
                {
                }

                ChangeForReader_t(const CacheChange_t& change) : status_(UNSENT),
                is_relevant_(true), seq_num_(change.sequence_number), num_fragments_(change.getFragmentCount())
                {
                   if (change.getFragmentSize() != 0)
                   {
                       for(uint32_t i = 1; i != change.getFragmentCount() + 1; ++i)
                       {
                           unsent_fragments_.insert(i); // Indexed on 1
                       }
                   }
                }

                ChangeForReader_t(const SequenceNumber_t& seq_num) : status_(UNSENT),
                is_relevant_(false), seq_num_(seq_num), num_fragments_(0)
                {
                }

                ~ChangeForReader_t(){}

                ChangeForReader_t& operator=(const ChangeForReader_t& ch)
                {
                    status_ = ch.status_;
                    is_relevant_ = ch.is_relevant_;
                    seq_num_ = ch.seq_num_;
                    unsent_fragments_ = ch.unsent_fragments_;
                    return *this;
                }

                void set_status(const ChangeForReaderStatus_t status)
                {
                    status_ = status;
                }

                ChangeForReaderStatus_t get_status() const
                {
                    return status_;
                }

                void set_relevance(const bool relevance)
                {
                    is_relevant_ = relevance;
                }

                bool is_relevant() const
                {
                    return is_relevant_;
                }

                const SequenceNumber_t get_sequence_number() const
                {
                    return seq_num_;
                }

                FragmentNumberSet_t get_unsent_fragments() const
                {
                    return unsent_fragments_;
                }

                void mark_all_fragments_as_unsent()
                {
                    for (uint32_t i = 1; i != num_fragments_ + 1; ++i)
                    {
                        unsent_fragments_.insert(i); // Indexed on 1
                    }
                }

                void mark_fragment_as_sent(const FragmentNumber_t& sentFragment)
                {
                    unsent_fragments_.erase(sentFragment);
                }

                void mark_fragment_as_unsent(const FragmentNumberSet_t& unsentFragments)
                {
                    for(auto element : unsentFragments.set)
                        unsent_fragments_.insert(element);
                }

                private:

                //!Status
                ChangeForReaderStatus_t status_;

                //!Boolean specifying if this change is relevant
                bool is_relevant_;

                //!Sequence number
                SequenceNumber_t seq_num_;

                std::set<FragmentNumber_t> unsent_fragments_;

                uint32_t num_fragments_;
            };

            struct ChangeForReaderCmp
            {
                bool operator()(const ChangeForReader_t& a, const ChangeForReader_t& b) const
                {
                    return a.seq_num_ < b.seq_num_;
                }
            };

            /**
             * Struct ChangeFromWriter_t used to indicate the state of a specific change with respect to a specific writer, as well as its relevance.
             *  @ingroup COMMON_MODULE
             */
            class ChangeFromWriter_t
            {
                friend struct ChangeFromWriterCmp;

                public:

                ChangeFromWriter_t() : status_(UNKNOWN), is_relevant_(true)
                {

                }

                ChangeFromWriter_t(const ChangeFromWriter_t& ch) : status_(ch.status_),
                is_relevant_(ch.is_relevant_), seq_num_(ch.seq_num_)
                {
                }

                ChangeFromWriter_t(const SequenceNumber_t& seq_num) : status_(UNKNOWN),
                is_relevant_(true), seq_num_(seq_num)
                {
                }

                ~ChangeFromWriter_t(){};

                ChangeFromWriter_t& operator=(const ChangeFromWriter_t& ch)
                {
                    status_ = ch.status_;
                    is_relevant_ = ch.is_relevant_;
                    seq_num_ = ch.seq_num_;
                    return *this;
                }

                void setStatus(const ChangeFromWriterStatus_t status)
                {
                    status_ = status;
                }

                ChangeFromWriterStatus_t getStatus() const
                {
                    return status_;
                }

                void setRelevance(const bool relevance)
                {
                    is_relevant_ = relevance;
                }

                bool isRelevant() const
                {
                    return is_relevant_;
                }

                const SequenceNumber_t getSequenceNumber() const
                {
                    return seq_num_;
                }

                //! Set change as not valid
                void notValid()
                {
                    is_relevant_ = false;
                }

                private:

                //! Status
                ChangeFromWriterStatus_t status_;

                //! Boolean specifying if this change is relevant
                bool is_relevant_;

                //! Sequence number
                SequenceNumber_t seq_num_;
            };

            struct ChangeFromWriterCmp
            {
                bool operator()(const ChangeFromWriter_t& a, const ChangeFromWriter_t& b) const
                {
                    return a.seq_num_ < b.seq_num_;
                }
            };
#endif
        }
    }
}
#endif /* CACHECHANGE_H_ */
