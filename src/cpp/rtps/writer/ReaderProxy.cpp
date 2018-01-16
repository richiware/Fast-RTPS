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
 * @file ReaderProxy.cpp
 *
 */


#include <fastrtps/rtps/writer/ReaderProxy.h>
#include <fastrtps/rtps/writer/StatefulWriter.h>
#include <fastrtps/utils/TimeConversion.h>
#include <fastrtps/rtps/writer/timedevent/NackResponseDelay.h>
#include <fastrtps/rtps/writer/timedevent/NackSupressionDuration.h>
#include <fastrtps/rtps/writer/timedevent/InitialHeartbeat.h>
#include <fastrtps/log/Log.h>
#include <fastrtps/rtps/resources/AsyncWriterThread.h>
#include <fastrtps/rtps/history/WriterHistory.h>

#include <mutex>

#include <cassert>

using namespace eprosima::fastrtps::rtps;


ReaderProxy::ReaderProxy(const RemoteReaderAttributes& rdata,const WriterTimes& times,StatefulWriter* SW) :
    m_att(rdata), mp_SFW(SW),
    mp_nackResponse(nullptr), mp_nackSupression(nullptr), mp_initialHeartbeat(nullptr), m_lastAcknackCount(0),
    mp_mutex(new std::recursive_mutex()), last_nackfrag_count_(0)
{
    if(rdata.endpoint.reliabilityKind == RELIABLE)
    {
        mp_nackResponse = new NackResponseDelay(*this, TimeConv::Time_t2MilliSecondsDouble(times.nackResponseDelay));
        mp_nackSupression = new NackSupressionDuration(*this, TimeConv::Time_t2MilliSecondsDouble(times.nackSupressionDuration));
        mp_initialHeartbeat = new InitialHeartbeat(*this, TimeConv::Time_t2MilliSecondsDouble(times.initialHeartbeatDelay));
    }

    logInfo(RTPS_WRITER,"Reader Proxy created");
}


ReaderProxy::~ReaderProxy()
{
    destroy_timers();
    delete(mp_mutex);
}

void ReaderProxy::destroy_timers()
{
    if(mp_nackResponse != nullptr)
    {
        delete(mp_nackResponse);
        mp_nackResponse = nullptr;
    }

    if(mp_nackSupression != nullptr)
    {
        delete(mp_nackSupression);
        mp_nackSupression = nullptr;
    }

    if(mp_initialHeartbeat != nullptr)
    {
        delete(mp_initialHeartbeat);
        mp_initialHeartbeat = nullptr;
    }
}

void ReaderProxy::add_change(const ChangeForReader_t& change)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    assert(change.get_sequence_number() > changesFromRLowMark_);
    assert(m_changesForReader.rbegin() != m_changesForReader.rend() ?
            change.get_sequence_number() > m_changesForReader.rbegin()->get_sequence_number() :
            true);

    if(m_changesForReader.size() == 0 && change.get_status() == ACKNOWLEDGED)
    {
        changesFromRLowMark_ = change.get_sequence_number();
        return;
    }

    m_changesForReader.insert(change);

    //TODO (Ricardo) Remove this functionality from here. It is not his place.
    if (change.get_status() == UNSENT)
    {
        AsyncWriterThread::wakeUp(mp_SFW);
    }
}

void ReaderProxy::back_to_live_change(const ChangeForReader_t& change)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    assert(change.get_sequence_number() <= changesFromRLowMark_);

    if(change.get_status() != ACKNOWLEDGED)
    {
        m_changesForReader.insert(change);
        changesFromRLowMark_ = change.get_sequence_number() - 1;

        if (change.get_status() == UNSENT)
        {
            AsyncWriterThread::wakeUp(mp_SFW);
        }
    }
}

size_t ReaderProxy::countChangesForReader() const
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    return m_changesForReader.size();
}

void ReaderProxy::acked_changes_set(const SequenceNumber_t& seqNum)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    if(seqNum > changesFromRLowMark_)
    {
        auto chit = m_changesForReader.find(seqNum);
        m_changesForReader.erase(m_changesForReader.begin(), chit);
        changesFromRLowMark_ = seqNum - 1;
    }
    else
    {
        //TODO Reimplement outside
        // Special case. Currently only used on Builtin StatefulWriters
        // after losing lease duration. Github issue #155

        /*
        SequenceNumber_t current_sequence = seqNum;
        if(seqNum < mp_SFW->get_seq_num_min())
        {
            current_sequence = mp_SFW->get_seq_num_min();
        }
        future_low_mark = current_sequence;

        for(; current_sequence <= changesFromRLowMark_; ++current_sequence)
        {
            CacheChange_t* change = nullptr;

            if(mp_SFW->mp_history->get_change(current_sequence, mp_SFW->getGuid(), &change))
            {
                ChangeForReader_t cr(change);
                cr.set_status(UNACKNOWLEDGED);
                m_changesForReader.insert(cr);
            }
            else
            {
                ChangeForReader_t cr(current_sequence);
                cr.set_status(UNACKNOWLEDGED);
                m_changesForReader.insert(cr);
            }
        }
        */
    }
}

bool ReaderProxy::requested_changes_set(std::vector<SequenceNumber_t>& seqNumSet)
{
    bool isSomeoneWasSetRequested = false;
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    for(std::vector<SequenceNumber_t>::iterator sit=seqNumSet.begin();sit!=seqNumSet.end();++sit)
    {
        auto chit = m_changesForReader.find(*sit);

        if(chit != m_changesForReader.end() && chit->is_relevant())
        {
            ChangeForReader_t newch(*chit);
            newch.set_status(REQUESTED);
            newch.mark_all_fragments_as_unsent();

            auto hint = m_changesForReader.erase(chit);

            m_changesForReader.insert(hint, newch);

            isSomeoneWasSetRequested = true;
        }
    }

    if(isSomeoneWasSetRequested)
    {
        logInfo(RTPS_WRITER,"Requested Changes: " << seqNumSet);
    }

    return isSomeoneWasSetRequested;
}

//TODO(Ricardo) Temporal
//std::vector<const ChangeForReader_t*> ReaderProxy::get_unsent_changes() const
std::vector<ChangeForReader_t*> ReaderProxy::get_unsent_changes()
{
    std::vector<ChangeForReader_t*> unsent_changes;
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    for(auto &change_for_reader : m_changesForReader)
    {
        if(change_for_reader.get_status() == UNSENT)
        {
            unsent_changes.push_back(const_cast<ChangeForReader_t*>(&change_for_reader));
        }
    }

    return unsent_changes;
}

std::vector<const ChangeForReader_t*> ReaderProxy::get_requested_changes() const
{
    std::vector<const ChangeForReader_t*> unsent_changes;
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    auto it = m_changesForReader.begin();
    for (; it!= m_changesForReader.end(); ++it)
        if(it->get_status() == REQUESTED)
            unsent_changes.push_back(&(*it));

    return unsent_changes;
}

void ReaderProxy::set_change_to_status(const SequenceNumber_t& seq_num, ChangeForReaderStatus_t status)
{
    if(seq_num <= changesFromRLowMark_)
        return;

    auto it = m_changesForReader.find(seq_num);
    bool mustWakeUpAsyncThread = false;

    if(it != m_changesForReader.end())
    {
        if(status == ACKNOWLEDGED && it == m_changesForReader.begin())
        {
            m_changesForReader.erase(it);
            changesFromRLowMark_ = seq_num;
        }
        else
        {
            ChangeForReader_t newch(*it);
            newch.set_status(status);
            if (status == UNSENT) mustWakeUpAsyncThread = true;
            auto hint = m_changesForReader.erase(it);
            m_changesForReader.insert(hint, newch);
        }
    }

    if (mustWakeUpAsyncThread)
        AsyncWriterThread::wakeUp(mp_SFW);
}

bool ReaderProxy::mark_fragment_as_sent_for_change(const SequenceNumber_t& sequence_number, FragmentNumber_t fragment)
{
    if(sequence_number <= changesFromRLowMark_)
        return false;

    bool allFragmentsSent = false;
    auto it = m_changesForReader.find(sequence_number);

    bool mustWakeUpAsyncThread = false; 

    if(it != m_changesForReader.end())
    {
        ChangeForReader_t newch(*it);
        newch.mark_fragment_as_sent(fragment);
        if (newch.get_unsent_fragments().isSetEmpty())
        {
            newch.set_status(UNDERWAY); //TODO (Ricardo) Check
            allFragmentsSent = true;
        }
        else
            mustWakeUpAsyncThread = true;
        auto hint = m_changesForReader.erase(it);
        m_changesForReader.insert(hint, newch);
    }

    if (mustWakeUpAsyncThread)
        AsyncWriterThread::wakeUp(mp_SFW);

    return allFragmentsSent;
}

void ReaderProxy::convert_status_on_all_changes(ChangeForReaderStatus_t previous, ChangeForReaderStatus_t next)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);
    bool mustWakeUpAsyncThread = false;

    auto it = m_changesForReader.begin();
    while(it != m_changesForReader.end())
    {
        if(it->get_status() == previous)
        {
            if(next == ACKNOWLEDGED && it == m_changesForReader.begin())
            {
                changesFromRLowMark_ = it->get_sequence_number();
                it = m_changesForReader.erase(it);
                continue;
            }
            else
            {
                ChangeForReader_t newch(*it);
                newch.set_status(next);
                if (next == UNSENT && previous != UNSENT)
                    mustWakeUpAsyncThread = true;
                auto hint = m_changesForReader.erase(it);

                it = m_changesForReader.insert(hint, newch);
            }
        }

        ++it;
    }

    if (mustWakeUpAsyncThread)
        AsyncWriterThread::wakeUp(mp_SFW);
}

void ReaderProxy::setNotValid(const SequenceNumber_t& sequence_number)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    if(sequence_number <= changesFromRLowMark_)
        return;

    auto chit = m_changesForReader.find(sequence_number);

    // Element must be in the container. In other case, bug.
    assert(chit != m_changesForReader.end());

    if(chit == m_changesForReader.begin())
    {
        assert(chit->get_status() != ACKNOWLEDGED);

        // if it is the first element, set state to unacknowledge because from now reader has to confirm
        // it will not be expecting it.
        ChangeForReader_t newch(*chit);
        newch.set_status(UNACKNOWLEDGED);
        newch.set_relevance(false);

        auto hint = m_changesForReader.erase(chit);

        m_changesForReader.insert(hint, newch);
    }
    else
    {
        // In case its state is not ACKNOWLEDGED, set it to UNACKNOWLEDGE because from now reader has to confirm
        // it will not be expecting it.
        ChangeForReader_t newch(*chit);
        if (chit->get_status() != ACKNOWLEDGED)
            newch.set_status(UNACKNOWLEDGED);
        newch.set_relevance(false);

        auto hint = m_changesForReader.erase(chit);

        m_changesForReader.insert(hint, newch);
    }
}

bool ReaderProxy::thereIsUnacknowledged() const
{
    bool returnedValue = false;
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    for(auto it = m_changesForReader.begin(); it!=m_changesForReader.end(); ++it)
    {
        if(it->get_status() == UNACKNOWLEDGED)
        {
            returnedValue = true;
            break;
        }
    }

    return returnedValue;
}

bool ReaderProxy::requested_fragment_set(SequenceNumber_t sequence_number, const FragmentNumberSet_t& frag_set)
{
    std::lock_guard<std::recursive_mutex> guard(*mp_mutex);

    // Locate the outbound change referenced by the NACK_FRAG
    auto changeIter = m_changesForReader.find(sequence_number);
    if (changeIter == m_changesForReader.end())
        return false;

    ChangeForReader_t newch(*changeIter);
    auto hint = m_changesForReader.erase(changeIter);
    newch.mark_fragment_as_unsent(frag_set);

    // If it was UNSENT, we shouldn't switch back to REQUESTED to prevent stalling.
    if (newch.get_status() != UNSENT)
        newch.set_status(REQUESTED);
    m_changesForReader.insert(hint, newch);

    return true;
}
