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

#ifndef RIPPLE_TAGGEDCACHE_H_INCLUDED
#define RIPPLE_TAGGEDCACHE_H_INCLUDED

#include "../../beast/beast/chrono/abstract_clock.h"
#include "../../beast/beast/chrono/chrono_io.h"
#include "../../beast/beast/Insight.h"
#include "../../beast/beast/container/hardened_hash.h"

#include <boost/smart_ptr.hpp>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <list>

namespace ripple {

// VFALCO NOTE Deprecated
struct TaggedCacheLog;

/** Map/cache combination.
    This class implements a cache and a map. The cache keeps objects alive
    in the map. The map allows multiple code paths that reference objects
    with the same tag to get the same actual object.

    So long as data is in the cache, it will stay in memory.
    If it stays in memory even after it is ejected from the cache,
    the map will track it.

    @note Callers must not modify data objects that are stored in the cache
          unless they hold their own lock over all cache operations.
*/
// VFALCO TODO Figure out how to pass through the allocator
template <
    class Key,
    class T,
    class Hash = beast::hardened_hash <Key>,
    class KeyEqual = std::equal_to <Key>,
    //class Allocator = std::allocator <std::pair <Key const, Entry>>,
    class Mutex = std::recursive_mutex
>
class TaggedCache
{
public:
    typedef Mutex mutex_type;
    // VFALCO DEPRECATED The caller can just use std::unique_lock <type>
    typedef std::unique_lock <mutex_type> ScopedLockType;
    typedef std::lock_guard <mutex_type> lock_guard;
    typedef Key key_type;
    typedef T mapped_type;
    // VFALCO TODO Use std::shared_ptr, std::weak_ptr
    typedef boost::weak_ptr <mapped_type> weak_mapped_ptr;
    typedef boost::shared_ptr <mapped_type> mapped_ptr;
    typedef beast::abstract_clock <std::chrono::seconds> clock_type;

public:
    // VFALCO TODO Change expiration_seconds to clock_type::duration
    TaggedCache (std::string const& name, int size,
        clock_type::rep expiration_seconds, clock_type& clock, beast::Journal journal,
            beast::insight::Collector::ptr const& collector = beast::insight::NullCollector::New ())
        : m_journal (journal)
        , m_clock (clock)
        , m_stats (name,
            std::bind (&TaggedCache::collect_metrics, this),
                collector)
        , m_name (name)
        , m_target_size (size)
        , m_target_age (std::chrono::seconds (expiration_seconds))
        , m_cache_count (0)
        , m_hits (0)
        , m_misses (0)
    {
        if (size <= 0)
            m_target_size = 1000;
    }

public:
    /** Return the clock associated with the cache. */
    clock_type& clock ()
    {
        return m_clock;
    }

    int getTargetSize () const
    {
        lock_guard lock (m_mutex);
        return m_target_size;
    }

    void setTargetSize (int s)
    {
        lock_guard lock (m_mutex);
        if (s <= 0)
            s = 1000;
        m_target_size = s;

        if (s > 0)
            m_cache.rehash (static_cast<std::size_t> ((s + (s >> 2)) / m_cache.max_load_factor () + 1));

        if (m_journal.debug) m_journal.debug <<
            m_name << " target size set to " << s;
    }

    clock_type::rep getTargetAge () const
    {
        lock_guard lock (m_mutex);
        return m_target_age.count();
    }

    void setTargetAge (clock_type::rep s)
    {
        lock_guard lock (m_mutex);
        m_target_age = std::chrono::seconds (s);
        if (m_journal.debug) m_journal.debug <<
            m_name << " target age set to " << m_target_age;
    }

    int getCacheSize ()
    {
        lock_guard lock (m_mutex);
        return m_cache_count;
    }

    int getTrackSize ()
    {
        lock_guard lock (m_mutex);
        return m_cache.size ();
    }

    float getHitRate ()
    {
        lock_guard lock (m_mutex);
        return (static_cast<float> (m_hits) * 100) / (1.0f + m_hits + m_misses);
    }

    void clearStats ()
    {
        lock_guard lock (m_mutex);
        m_hits = 0;
        m_misses = 0;
    }

    void clear ()
    {
        lock_guard lock (m_mutex);
        m_cache.clear ();
        m_cache_count = 0;
        mCacheData.clear();
    }

    void sweep ()
    {
        // Keep references to all the stuff we sweep
        // so that we can destroy them outside the lock.
        //
        std::vector <mapped_ptr> stuffToSweep;
    
        {
            clock_type::time_point const now (m_clock.now());
            clock_type::time_point when_expire;

            lock_guard lock (m_mutex);

            when_expire = now - m_target_age;
            stuffToSweep.reserve (m_cache.size ());

            if (m_cache_count > 0)
            {
                auto oldest = mCacheData.end();

                while (m_cache_count != 0)
                {
                    --oldest;
                    if (oldest->last_access > when_expire)
                        break;
                    stuffToSweep.push_back(oldest->ptr);
                    m_cache.erase(oldest->mKey);
                    oldest = mCacheData.erase(oldest);
                    --m_cache_count;
                }
                if (m_journal.trace && (stuffToSweep.size() != 0))
                    m_journal.trace << m_name << ": cache = " << m_cache.size() << "-" << stuffToSweep.size();
            }
        }

        // At this point stuffToSweep will go out of scope outside the lock
        // and decrement the reference count on each strong pointer.
    }

    bool del (const key_type& key, bool valid)
    {
        // Remove from cache, if !valid, remove from map too. Returns true if removed from cache
        lock_guard lock (m_mutex);
        auto cit = m_cache.find (key);
        if (cit == m_cache.end())
            return false;

        mCacheData.erase(cit->second);
        m_cache.erase(cit);
        --m_cache_count;
        return true;
    }

    /** Replace aliased objects with originals.

        Due to concurrency it is possible for two separate objects with
        the same content and referring to the same unique "thing" to exist.
        This routine eliminates the duplicate and performs a replacement
        on the callers shared pointer if needed.

        @param key The key corresponding to the object
        @param data A shared pointer to the data corresponding to the object.
        @param replace `true` if `data` is the up to date version of the object.

        @return `true` If the key already existed.
    */
    bool canonicalize (const key_type& key, boost::shared_ptr<T>& data, bool replace = false)
    {
        // Return canonical value, store if needed, refresh in cache
        // Return values: true=we had the data already
        lock_guard lock (m_mutex);
        bool res = false;
        auto cit = m_cache.find (key);
        if (cit == m_cache.end())
        {
            mCacheData.push_front(Entry(key, m_clock.now(), data));
            m_cache.emplace(key, mCacheData.begin());
            ++m_cache_count;
            auto oldest = --(mCacheData.end());
            while (m_cache_count > m_target_size)
            {
                m_cache.erase(oldest->mKey);
                oldest = mCacheData.erase(oldest);
                --m_cache_count;
            }
        }
        else
        {
            // move to the front
            cit->second->touch(m_clock.now());
            mCacheData.splice(mCacheData.begin(), mCacheData, cit->second);
            res = true;
            if (replace)
            {
                cit->second->ptr = data;
            }
            else
            {
                data = cit->second->ptr;
            }
        }
        return res;
    }

    boost::shared_ptr<T> fetch (const key_type& key)
    {
        // fetch us a shared pointer to the stored data object
        lock_guard lock (m_mutex);
        auto cit = m_cache.find (key);
        if (cit == m_cache.end())
        {
            m_misses++;
            return mapped_ptr ();
        }
        m_hits++;
        cit->second->touch(m_clock.now());
        // move to the front of the list
        mCacheData.splice(mCacheData.begin(), mCacheData, cit->second);
        return cit->second->ptr;
    }

    /** Insert the element into the container.
        If the key already exists, nothing happens.
        @return `true` If the element was inserted
    */
    bool insert (key_type const& key, T const& value)
    {
        mapped_ptr p (boost::make_shared <T> (
            std::cref (value)));
        return canonicalize (key, p);
    }

    // VFALCO NOTE It looks like this returns a copy of the data in
    //             the output parameter 'data'. This could be expensive.
    //             Perhaps it should work like standard containers, which
    //             simply return an iterator.
    //
    bool retrieve (const key_type& key, T& data)
    {
        // retrieve the value of the stored data
        mapped_ptr entry = fetch (key);

        if (!entry)
            return false;

        data = *entry;
        return true;
    }

    /** Refresh the expiration time on a key.

        @param key The key to refresh.
        @return `true` if the key was found and the object is cached.
    */
    bool refreshIfPresent (const key_type& key)
    {
        return fetch(key);
    }

    mutex_type& peekMutex ()
    {
        return m_mutex;
    }

private:
    void collect_metrics ()
    {
        m_stats.size.set (getCacheSize ());

        {
            beast::insight::Gauge::value_type hit_rate (0);
            {
                lock_guard lock (m_mutex);
                auto const total (m_hits + m_misses);
                if (total != 0)
                    hit_rate = (m_hits * 100) / total;
            }
            m_stats.hit_rate.set (hit_rate);
        }
    }

    struct Stats
    {
        template <class Handler>
        Stats (std::string const& prefix, Handler const& handler,
            beast::insight::Collector::ptr const& collector)
            : hook (collector->make_hook (handler))
            , size (collector->make_gauge (prefix, "size"))
            , hit_rate (collector->make_gauge (prefix, "hit_rate"))
            { }

        beast::insight::Hook hook;
        beast::insight::Gauge size;
        beast::insight::Gauge hit_rate;
    };

    class Entry
    {
    public:
        mapped_ptr ptr;
        clock_type::time_point last_access;
        Key mKey;

        Entry (Key const& key, clock_type::time_point const& last_access_,
            mapped_ptr const& ptr_)
            : ptr (ptr_)
            //, weak_ptr (ptr_)
            , last_access (last_access_)
            , mKey(key)
        {
        }

        void touch (clock_type::time_point const& now) { last_access = now; }
    };

    typedef std::list<Entry> LRUList;
    typedef typename LRUList::iterator LRUIterator;
    typedef ripple::unordered_map <key_type, LRUIterator, Hash, KeyEqual> cache_type;

    typedef typename cache_type::iterator cache_iterator;

    LRUList mCacheData; // actually holds the data, hash is indirection to this

    beast::Journal m_journal;
    clock_type& m_clock;
    Stats m_stats;

    mutex_type mutable m_mutex;

    // Used for logging
    std::string m_name;

    // Desired number of cache entries (0 = ignore)
    int m_target_size;

    // Desired maximum cache age
    clock_type::duration m_target_age;

    // Number of items cached
    int m_cache_count;
    cache_type m_cache;  // Hold strong reference to recent objects
    std::uint64_t m_hits;
    std::uint64_t m_misses;
};

}

#endif
