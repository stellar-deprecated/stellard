//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_PEERFINDER_LOGIC_H_INCLUDED
#define RIPPLE_PEERFINDER_LOGIC_H_INCLUDED

#include "Fixed.h"
#include "SlotImp.h"

#include "handout.h"
#include "ConnectHandouts.h"
#include "RedirectHandouts.h"
#include "SlotHandouts.h"
#include "Store.h"
#include "Source.h"
#include "Counts.h"
#include "Bootcache.h"
#include "Livecache.h"
#include "ripple/peerfinder/api/Callback.h"
#include "Checker.h"
#include "Reporting.h"

#include "beast/beast/container/aged_container_utility.h"
#include "beast/beast/utility/Journal.h"
#include "beast/beast/smart_ptr/SharedPtr.h"

#include <set>
#include <map>
#include <unordered_map>

namespace ripple {
namespace PeerFinder {


	

/** The Logic for maintaining the list of Slot addresses.
    We keep this in a separate class so it can be instantiated
    for unit tests.
*/
class PeerSlotLogic
{
public:
    // Maps remote endpoints to slots. Since a slot has a
    // remote endpoint upon construction, this holds all counts.
    // 
    typedef std::map <beast::IP::Endpoint,
        std::shared_ptr <SlotImp>> Slots;

    typedef std::map <beast::IP::Endpoint, Fixed> FixedSlots;

    // A set of unique Ripple public keys
    typedef std::set <RipplePublicKey> Keys;

    // A set of non-unique IPAddresses without ports, used
    // to filter duplicates when making outgoing connections.
    typedef std::multiset <beast::IP::Endpoint> ConnectedAddresses;

    struct State
    {
        State (
            Store* store,
            clock_type& clock,
            beast::Journal journal)
            : stopping (false)
            , counts ()
            , livecache (clock, beast::Journal (
                journal, Reporting::livecache))
            , bootcache (*store, clock, beast::Journal (
                journal, Reporting::bootcache))
        {
        }

        // True if we are stopping.
        bool stopping;

        // The source we are currently fetching.
        // This is used to cancel I/O during program exit.
        beast::SharedPtr <Source> fetchSource;

       
        // Slot counts and other aggregate statistics.
        Counts counts;

        // A list of slots that should always be connected
        FixedSlots fixed;

        // Live livecache from mtENDPOINTS messages
        Livecache <> livecache;

        // LiveCache of addresses suitable for gaining initial connections
        Bootcache bootcache;

        // Holds all counts
        Slots slots;

        // The addresses (but not port) we are connected to. This includes
        // outgoing connection attempts. Note that this set can contain
        // duplicates (since the port is not set)
        ConnectedAddresses connected_addresses; 

        // Set of public keys belonging to active peers
        Keys keys;
    };

    typedef beast::SharedData <State> SharedState;

    beast::Journal m_journal;
    SharedState m_state;
    clock_type& m_clock;
    Callback& m_callback;
    Store& m_store;
    Checker& m_checker;

    // A list of dynamic sources to consult as a fallback
    std::vector <beast::SharedPtr <Source>> m_sources;

    clock_type::time_point m_whenBroadcast;

    ConnectHandouts::Squelches m_squelches;

    //--------------------------------------------------------------------------

	PeerSlotLogic(
        clock_type& clock,
        Callback& callback,
        Store& store,
        Checker& checker,
        beast::Journal journal)
        : m_journal (journal, Reporting::logic)
        , m_state (&store, std::ref (clock), journal)
        , m_clock (clock)
        , m_callback (callback)
        , m_store (store)
        , m_checker (checker)
        , m_whenBroadcast (m_clock.now())
        , m_squelches (m_clock)
    {
        setConfig ();
    }

    // Load persistent state information from the Store
    //
    void load ()
    {
        SharedState::Access state (m_state);

        state->bootcache.load ();
    }

    /** Stop the logic.
        This will cancel the current fetch and set the stopping flag
        to `true` to prevent further fetches.
        Thread safety:
            Safe to call from any thread.
    */
    void stop ()
    {
        SharedState::Access state (m_state);
        state->stopping = true;
        if (state->fetchSource != nullptr)
            state->fetchSource->cancel ();
    }

    //--------------------------------------------------------------------------
    //
    // Manager
    //
    //--------------------------------------------------------------------------

    void setConfig ()
    {
        SharedState::Access state (m_state);
        state->counts.onConfig ();
    }

	void addFixedPeer(std::string const& name,
		std::vector <beast::IP::Endpoint> const& addresses);

    //--------------------------------------------------------------------------

    // Called when the Checker completes a connectivity test
	void checkComplete(beast::IP::Endpoint const& address,
		beast::IP::Endpoint const & checkedAddress, Checker::Result const& result);

    //--------------------------------------------------------------------------

	SlotImp::ptr new_inbound_slot(beast::IP::Endpoint const& local_endpoint,
		beast::IP::Endpoint const& remote_endpoint);

	SlotImp::ptr new_outbound_slot(beast::IP::Endpoint const& remote_endpoint);

	void on_connected(SlotImp::ptr const& slot, beast::IP::Endpoint const& local_endpoint);

	void on_handshake(SlotImp::ptr const& slot, RipplePublicKey const& key, bool cluster);

    //--------------------------------------------------------------------------

    // Validate and clean up the list that we received from the slot.
	void preprocess(SlotImp::ptr const& slot, Endpoints& list,
		SharedState::Access& state);

	void on_endpoints(SlotImp::ptr const& slot, Endpoints list);

    //--------------------------------------------------------------------------

    void on_legacy_endpoints (IPAddresses const& list)
    {
        // Ignoring them also seems a valid choice.
        SharedState::Access state (m_state);
        for (IPAddresses::const_iterator iter (list.begin());
            iter != list.end(); ++iter)
            state->bootcache.insert (*iter);
    }

    void remove (SlotImp::ptr const& slot, SharedState::Access& state)
    {
        Slots::iterator const iter (state->slots.find (
            slot->remote_endpoint ()));
        // The slot must exist in the table
        assert (iter != state->slots.end ());
        // Remove from slot by IP table
        state->slots.erase (iter);
        // Remove the key if present
        if (slot->public_key () != boost::none)
        {
            Keys::iterator const iter (state->keys.find (*slot->public_key()));
            // Key must exist
            assert (iter != state->keys.end ());
            state->keys.erase (iter);
        }
        // Remove from connected address table
        {
            auto const iter (state->connected_addresses.find (
                slot->remote_endpoint().at_port (0)));
            // Address must exist
            assert (iter != state->connected_addresses.end ());
            state->connected_addresses.erase (iter);
        }

        // Update counts
        state->counts.remove (*slot);
    }

	void on_closed(SlotImp::ptr const& slot);

	void on_cancel(SlotImp::ptr const& slot);

    //--------------------------------------------------------------------------

    // Returns `true` if the address matches a fixed slot address
    bool fixed (beast::IP::Endpoint const& endpoint, SharedState::Access& state) const
    {
        for (auto const& entry : state->fixed)
            if (entry.first == endpoint)
                return true;
        return false;
    }

    // Returns `true` if the address matches a fixed slot address
    // Note that this does not use the port information in the IP::Endpoint
    bool fixed (beast::IP::Address const& address, SharedState::Access& state) const
    {
        for (auto const& entry : state->fixed)
            if (entry.first.address () == address)
                return true;
        return false;
    }

    //--------------------------------------------------------------------------
    //
    // Connection Strategy
    //
    //--------------------------------------------------------------------------

    /** Adds eligible Fixed addresses for outbound attempts. */
    template <class Container>
    void get_fixed (std::size_t needed, Container& c,
        SharedState::Access& state)
    {
        auto const now (m_clock.now());
        for (auto iter = state->fixed.begin ();
            needed && iter != state->fixed.end (); ++iter)
        {
            auto const& address (iter->first.address());
            if (iter->second.when() <= now && std::none_of (
                state->slots.cbegin(), state->slots.cend(),
                    [address](Slots::value_type const& v)
                    {
                        return address == v.first.address();
                    }))
            {
                c.push_back (iter->first);
                --needed;
            }
        }
    }

    //--------------------------------------------------------------------------

    // Adds slot addresses to the squelched set
    void squelch_slots (SharedState::Access& state)
    {
        for (auto const& s : state->slots)
        {
            auto const result (m_squelches.insert (
                s.second->remote_endpoint().address()));
            if (! result.second)
                m_squelches.touch (result.first);
        }
    }

    /** Create new outbound connection attempts as needed.
        This implements PeerFinder's "Outbound Connection Strategy"
    */
	void autoconnect();

    //--------------------------------------------------------------------------

    void addStaticSource (beast::SharedPtr <Source> const& source)
    {
        fetch (source);
    }

    void addSource (beast::SharedPtr <Source> const& source)
    {
        m_sources.push_back (source);
    }

    //--------------------------------------------------------------------------

    // Called periodically to sweep the livecache and remove aged out items.
    void expire ()
    {
        SharedState::Access state (m_state);

        // Expire the Livecache
        state->livecache.expire ();

        // Expire the recent cache in each slot
        for (auto const& entry : state->slots)
            entry.second->expire();

        // Expire the recent attempts table
        beast::expire (m_squelches,
            Tuning::recentAttemptDuration);
    }

    // Called every so often to perform periodic tasks.
    void periodicActivity ()
    {
        SharedState::Access state (m_state);

        clock_type::time_point const now (m_clock.now());

        autoconnect ();
        expire ();
        state->bootcache.periodicActivity ();

        if (m_whenBroadcast <= now)
        {
            broadcast ();
            m_whenBroadcast = now + Tuning::secondsPerMessage;
        }
    }

    //--------------------------------------------------------------------------
    //
    // Bootcache livecache sources
    //
    //--------------------------------------------------------------------------

    // Add one address.
    // Returns `true` if the address is new.
    //
    bool addBootcacheAddress (beast::IP::Endpoint const& address,
        SharedState::Access& state)
    {
        return state->bootcache.insert (address);
    }

    // Add a set of addresses.
    // Returns the number of addresses added.
    //
    int addBootcacheAddresses (IPAddresses const& list)
    {
        int count (0);
        SharedState::Access state (m_state);        
        for (auto addr : list)
        {
            if (addBootcacheAddress (addr, state))
                ++count;
        }
        return count;
    }

    // Fetch bootcache addresses from the specified source.
	void fetch(beast::SharedPtr <Source> const& source);

    //--------------------------------------------------------------------------
    //
    // Endpoint message handling
    //
    //--------------------------------------------------------------------------

    // Returns true if the IP::Endpoint contains no invalid data.
    bool is_valid_address (beast::IP::Endpoint const& address)
    {
        if (is_unspecified (address))
            return false;
        if (! is_public (address))
            return false;
        if (address.port() == 0)
            return false;
        return true;
    }

    //--------------------------------------------------------------------------

    // Gives a slot a set of addresses to try next since we're full
	void redirect(SlotImp::ptr const& slot, SharedState::Access& state);

    // Send mtENDPOINTS for each slot as needed
	void broadcast();

    //--------------------------------------------------------------------------
    //
    // PropertyStream
    //
    //--------------------------------------------------------------------------

    void writeSlots (beast::PropertyStream::Set& set, Slots const& slots)
    {
        for (auto const& entry : slots)
        {
            beast::PropertyStream::Map item (set);
            SlotImp const& slot (*entry.second);
            if (slot.local_endpoint () != boost::none)
                item ["local_address"] = to_string (*slot.local_endpoint ());
            item ["remote_address"]   = to_string (slot.remote_endpoint ());
            if (slot.inbound())
                item ["inbound"]    = "yes";
            if (slot.fixed())
                item ["fixed"]      = "yes";
            if (slot.cluster())
                item ["cluster"]    = "yes";
            
            item ["state"] = stateString (slot.state());
        }
    }

    void onWrite (beast::PropertyStream::Map& map)
    {
        SharedState::Access state (m_state);

        // VFALCO NOTE These ugly casts are needed because
        //             of how std::size_t is declared on some linuxes
        //
        map ["bootcache"]   = std::uint32_t (state->bootcache.size());
        map ["fixed"]       = std::uint32_t (state->fixed.size());

        {
            beast::PropertyStream::Set child ("peers", map);
            writeSlots (child, state->slots);
        }

        {
            beast::PropertyStream::Map child ("counts", map);
            state->counts.onWrite (child);
        }

        {
            beast::PropertyStream::Map child ("livecache", map);
            state->livecache.onWrite (child);
        }

        {
            beast::PropertyStream::Map child ("bootcache", map);
            state->bootcache.onWrite (child);
        }
    }

    //--------------------------------------------------------------------------
    //
    // Diagnostics
    //
    //--------------------------------------------------------------------------

    State const& state () const
    {
        return *SharedState::ConstAccess (m_state);
    }

    Counts const& counts () const
    {
        return SharedState::ConstAccess (m_state)->counts;
    }

    static std::string stateString (Slot::State state)
    {
        switch (state)
        {
        case Slot::accept:     return "accept";
        case Slot::connect:    return "connect";
        case Slot::connected:  return "connected";
        case Slot::active:     return "active";
        case Slot::closing:    return "closing";
        default:
            break;
        };
        return "?";
    }
};

}
}

#endif

/*

- recent tables entries should last equal to the cache time to live
- never send a slot a message thats in its recent table at a lower hop count
- when sending a message to a slot, add it to its recent table at one lower hop count

Giveaway logic

When do we give away?

- To one inbound connection when we redirect due to full

- To all slots at every broadcast

*/
