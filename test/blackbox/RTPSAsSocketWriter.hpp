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
 * @file RTPSAsSocketWriter.hpp
 *
 */

#ifndef _TEST_BLACKBOX_RTPSASSOCKETWRITER_HPP_
#define _TEST_BLACKBOX_RTPSASSOCKETWRITER_HPP_

#include <fastrtps/rtps/participant/RTPSParticipant.h>
#include <fastrtps/rtps/attributes/RTPSParticipantAttributes.h>
#include <fastrtps/rtps/writer/StatefulWriter.h>
#include <fastrtps/rtps/writer/StatelessWriter.h>
#include <fastrtps/rtps/attributes/HistoryAttributes.h>
#include <fastrtps/rtps/history/WriterHistory.h>
#include <fastrtps/rtps/rtps_fwd.h>
#include <fastrtps/rtps/attributes/WriterAttributes.h>

#include <fastcdr/FastBuffer.h>
#include <fastcdr/Cdr.h>

#include <string>
#include <list>
#include <asio.hpp>
#include <gtest/gtest.h>


template<class TypeSupport>
class RTPSAsSocketWriter
{
    public:

        typedef TypeSupport type_support;
        typedef typename type_support::type type;

        RTPSAsSocketWriter(const std::string& magicword) : participant_(nullptr),
        writer_(nullptr), history_(nullptr), initialized_(false), port_(0)
        {
            std::ostringstream mw;
            mw << magicword << "_" << asio::ip::host_name() << "_" << GET_PID();
            magicword_ = mw.str();

#if defined(PREALLOCATED_WITH_REALLOC_MEMORY_MODE_TEST)
            hattr_.memoryPolicy = eprosima::fastrtps::rtps::PREALLOCATED_WITH_REALLOC_MEMORY_MODE;
#elif defined(DYNAMIC_RESERVE_MEMORY_MODE_TEST)
            hattr_.memoryPolicy = eprosima::fastrtps::rtps::DYNAMIC_RESERVE_MEMORY_MODE;
#else
            hattr_.memoryPolicy = eprosima::fastrtps::rtps::PREALLOCATED_MEMORY_MODE;
#endif

            // By default, heartbeat period and nack response delay are 100 milliseconds.
            writer_attr_.times.heartbeatPeriod.seconds = 0;
            writer_attr_.times.heartbeatPeriod.fraction = 4294967 * 100;
            writer_attr_.times.nackResponseDelay.seconds = 0;
            writer_attr_.times.nackResponseDelay.fraction = 4294967 * 100;
        }

        virtual ~RTPSAsSocketWriter()
        {
            if(participant_ != nullptr)
            {
                delete participant_;
            }
            if(history_ != nullptr)
            {
                delete history_;
            }
        }

        void init()
        {
            //Create participant
            eprosima::fastrtps::rtps::RTPSParticipantAttributes pattr;
            pattr.builtin.use_SIMPLE_RTPSParticipantDiscoveryProtocol = false;
            pattr.builtin.use_WriterLivelinessProtocol = false;
            pattr.builtin.domainId = (uint32_t)GET_PID() % 230;
            pattr.participantID = 2;
            participant_ = new eprosima::fastrtps::rtps::RTPSParticipant(pattr,
                    eprosima::fastrtps::rtps::GuidPrefix_t::unknown());
            ASSERT_NE(participant_, nullptr);

            //Create writerhistory
            hattr_.payloadMaxSize = 255 + type_.m_typeSize;
            history_ = new eprosima::fastrtps::rtps::WriterHistory(hattr_);

            //Create writer
            if(writer_attr_.endpoint.reliabilityKind == eprosima::fastrtps::rtps::ReliabilityKind_t::RELIABLE)
            {
                writer_ = new eprosima::fastrtps::rtps::StatefulWriter(*participant_, writer_attr_, *history_);
            }
            else
            {
                writer_ = new eprosima::fastrtps::rtps::StatelessWriter(*participant_, writer_attr_, *history_);
            }
            ASSERT_NE(writer_, nullptr);

            register_reader();

            initialized_ = true;
        }

        bool isInitialized() const { return initialized_; }

        void send(std::list<type>& msgs)
        {
            auto it = msgs.begin();

            while(it != msgs.end())
            {
                eprosima::fastrtps::rtps::CacheChange_ptr ch = writer_->new_change([&]() -> uint32_t
                        {
                        size_t current_alignment =  4 + magicword_.size() + 1;
                        return (uint32_t)(current_alignment + type::getCdrSerializedSize(*it, current_alignment));
                        }
                        , eprosima::fastrtps::rtps::ALIVE);

                if(ch)
                {
                    eprosima::fastcdr::FastBuffer buffer((char*)ch->serialized_payload.data,
                            ch->serialized_payload.max_size);
                    eprosima::fastcdr::Cdr cdr(buffer);

                    cdr << magicword_;
                    cdr << *it;
                    ch->serialized_payload.length = static_cast<uint32_t>(cdr.getSerializedDataLength());

                    history_->add_change(ch);
                    it = msgs.erase(it);
                }
                else
                {
                    std::cout << "ERROR: Writer doesn't return a new CacheChange" << std::endl;
                }
            }
        }

        /*** Function to change QoS ***/
        RTPSAsSocketWriter& reliability(const eprosima::fastrtps::rtps::ReliabilityKind_t kind)
        {
            writer_attr_.endpoint.reliabilityKind = kind;

            if(kind == eprosima::fastrtps::rtps::ReliabilityKind_t::RELIABLE)
                writer_attr_.endpoint.setEntityID(2);
            return *this;
        }

        RTPSAsSocketWriter& add_to_multicast_locator_list(const std::string& ip, uint32_t port)
        {
            ip_ = ip;
            port_ = port;

            eprosima::fastrtps::rtps::Locator_t loc;
            loc.set_IP4_address(ip);
            loc.port = port;
            writer_attr_.endpoint.multicastLocatorList.push_back(loc);

            return *this;
        }

        void register_reader()
        {
            if(port_ == 0)
            {
                std::cout << "ERROR: locator has to be registered previous to call this" << std::endl;
            }

            //Add remote reader (in this case a reader in the same machine)
            eprosima::fastrtps::rtps::GUID_t guid = participant_->guid();

            eprosima::fastrtps::rtps::RemoteReaderAttributes rattr;
            eprosima::fastrtps::rtps::Locator_t loc;
            loc.set_IP4_address(ip_);
            loc.port = port_;
            rattr.endpoint.multicastLocatorList.push_back(loc);

            if(writer_attr_.endpoint.reliabilityKind == eprosima::fastrtps::rtps::RELIABLE)
            {
                rattr.endpoint.reliabilityKind = eprosima::fastrtps::rtps::RELIABLE;
                rattr.guid.guidPrefix.value[0] = guid.guidPrefix.value[0];
                rattr.guid.guidPrefix.value[1] = guid.guidPrefix.value[1];
                rattr.guid.guidPrefix.value[2] = guid.guidPrefix.value[2];
                rattr.guid.guidPrefix.value[3] = guid.guidPrefix.value[3];
                rattr.guid.guidPrefix.value[4] = guid.guidPrefix.value[4];
                rattr.guid.guidPrefix.value[5] = guid.guidPrefix.value[5];
                rattr.guid.guidPrefix.value[6] = guid.guidPrefix.value[6];
                rattr.guid.guidPrefix.value[7] = guid.guidPrefix.value[7];
                rattr.guid.guidPrefix.value[8] = 1;
                rattr.guid.guidPrefix.value[9] = 0;
                rattr.guid.guidPrefix.value[10] = 0;
                rattr.guid.guidPrefix.value[11] = 0;
                rattr.guid.entityId.value[0] = 0;
                rattr.guid.entityId.value[1] = 0;
                rattr.guid.entityId.value[2] = 1;
                rattr.guid.entityId.value[3] = 4;
            }

            writer_->matched_reader_add(rattr);
        }

        RTPSAsSocketWriter& asynchronously(const eprosima::fastrtps::rtps::RTPSWriterPublishMode mode)
        {
            writer_attr_.mode = mode;

            return *this;
        }

        RTPSAsSocketWriter& add_throughput_controller_descriptor_to_pparams(uint32_t bytesPerPeriod, uint32_t periodInMs)
        {
            eprosima::fastrtps::rtps::ThroughputControllerDescriptor descriptor {bytesPerPeriod, periodInMs};
            writer_attr_.throughputController = descriptor;

            return *this;
        }

        RTPSAsSocketWriter& heartbeat_period_seconds(int32_t sec)
        {
            writer_attr_.times.heartbeatPeriod.seconds = sec;
            return *this;
        }

        RTPSAsSocketWriter& heartbeat_period_fraction(uint32_t frac)
        {
            writer_attr_.times.heartbeatPeriod.fraction = frac;
            return *this;
        }

    private:

        eprosima::fastrtps::rtps::RTPSParticipant *participant_;
        eprosima::fastrtps::rtps::RTPSWriter *writer_;
        eprosima::fastrtps::rtps::WriterAttributes writer_attr_;
        eprosima::fastrtps::rtps::WriterHistory *history_;
        eprosima::fastrtps::rtps::HistoryAttributes hattr_;
        bool initialized_;
        std::string magicword_;
        type_support type_;
        std::string ip_;
        uint32_t port_;
};

#endif // _TEST_BLACKBOX_RTPSASSOCKETWRITER_HPP_
