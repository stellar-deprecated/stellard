
#include "ripple_core/functional/Config.h"
#include "PeerSlotLogic.h"
#include "ripple/peerfinder/impl/iosformat.h"

namespace ripple {
	namespace PeerFinder {


		void PeerSlotLogic::addFixedPeer(std::string const& name,
			std::vector <beast::IP::Endpoint> const& addresses)
		{
			SharedState::Access state(m_state);

			if (addresses.empty())
			{
				if (m_journal.info) m_journal.info <<
					"Could not resolve fixed slot '" << name << "'";
				return;
			}

			for (auto const& remote_address : addresses)
			{
				auto result(state->fixed.emplace(std::piecewise_construct,
					std::forward_as_tuple(remote_address),
					std::make_tuple(std::ref(m_clock))));

				if (result.second)
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic add fixed" << "'" << name <<
						"' at " << remote_address;
					return;
				}
			}
		}


		void PeerSlotLogic::on_handshake(SlotImp::ptr const& slot, RipplePublicKey const& key, bool cluster)
		{
			if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
				"Logic handshake " << slot->remote_endpoint() <<
				" with " << (cluster ? "clustered " : "") << "key " << key;

			SharedState::Access state(m_state);

			// The object must exist in our table
			assert(state->slots.find(slot->remote_endpoint()) !=
				state->slots.end());
			// Must be accepted or connected
			assert(slot->state() == Slot::accept ||
				slot->state() == Slot::connected);

			// Check for duplicate connection by key
			if (state->keys.find(key) != state->keys.end())
			{
				if (m_journal.info) m_journal.info << beast::leftw(18) <<
					"Duplicate Connection: " << key;

				m_callback.disconnect(slot, true);
				return;
			}

			// See if we have an open space for this slot
			if (state->counts.can_activate(*slot))
			{
				// Set key and cluster right before adding to the map
				// otherwise we could assert later when erasing the key.
				state->counts.remove(*slot);
				slot->public_key(key);
				slot->cluster(cluster);
				state->counts.add(*slot);

				// Add the public key to the active set
				std::pair <Keys::iterator, bool> const result(
					state->keys.insert(key));
				// Public key must not already exist
				assert(result.second);

				// Change state and update counts
				state->counts.remove(*slot);
				slot->activate(m_clock.now());
				state->counts.add(*slot);

				if (!slot->inbound())
					state->bootcache.on_success(
					slot->remote_endpoint());

				// Mark fixed slot success
				if (slot->fixed() && !slot->inbound())
				{
					auto iter(state->fixed.find(slot->remote_endpoint()));
					assert(iter != state->fixed.end());
					iter->second.success(m_clock.now());
					if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
						"Logic fixed " << slot->remote_endpoint() << " success";
				}

				m_callback.activate(slot);
			}
			else
			{
				if (m_journal.info) m_journal.info << beast::leftw(18) <<
					"Full dropping peer: " << key;

				if (!slot->inbound())
					state->bootcache.on_success(slot->remote_endpoint());

				// Maybe give them some addresses to try
				if (slot->inbound())
					redirect(slot, state);

				m_callback.disconnect(slot, true);
			}
		}

		void PeerSlotLogic::preprocess(SlotImp::ptr const& slot, Endpoints& list,
			SharedState::Access& state)
		{
			bool neighbor(false);
			for (auto iter(list.begin()); iter != list.end();)
			{
				Endpoint& ep(*iter);

				// Enforce hop limit
				if (ep.hops > Tuning::maxHops)
				{
					if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
						"Endpoints drop " << ep.address <<
						" for excess hops " << ep.hops;
					iter = list.erase(iter);
					continue;
				}

				// See if we are directly connected
				if (ep.hops == 0)
				{
					if (!neighbor)
					{
						// Fill in our neighbors remote address
						neighbor = true;
						ep.address = slot->remote_endpoint().at_port(
							ep.address.port());
					}
					else
					{
						if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
							"Endpoints drop " << ep.address <<
							" for extra self";
						iter = list.erase(iter);
						continue;
					}
				}

				// Discard invalid addresses
				if (!is_valid_address(ep.address))
				{
					if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
						"Endpoints drop " << ep.address <<
						" as invalid";
					iter = list.erase(iter);
					continue;
				}

				// Filter duplicates
				if (std::any_of(list.begin(), iter,
					[ep](Endpoints::value_type const& other)
				{
					return ep.address == other.address;
				}))
				{
					if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
						"Endpoints drop " << ep.address <<
						" as duplicate";
					iter = list.erase(iter);
					continue;
				}

				// Increment hop count on the incoming message, so
				// we store it at the hop count we will send it at.
				//
				++ep.hops;

				++iter;
			}
		}


		void PeerSlotLogic::on_endpoints(SlotImp::ptr const& slot, Endpoints list)
		{
			if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
				"Endpoints from " << slot->remote_endpoint() <<
				" contained " << list.size() <<
				((list.size() > 1) ? " entries" : " entry");

			SharedState::Access state(m_state);

			// The object must exist in our table
			assert(state->slots.find(slot->remote_endpoint()) !=
				state->slots.end());

			// Must be handshaked!
			assert(slot->state() == Slot::active);

			preprocess(slot, list, state);

			clock_type::time_point const now(m_clock.now());

			for (auto iter(list.cbegin()); iter != list.cend(); ++iter)
			{
				Endpoint const& ep(*iter);

				assert(ep.hops != 0);

				slot->recent.insert(ep.address, ep.hops);

				// Note hops has been incremented, so 1
				// means a directly connected neighbor.
				//
				if (ep.hops == 1)
				{
					if (slot->connectivityCheckInProgress)
					{
						if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
							"Logic testing " << ep.address << " already in progress";
						continue;
					}

					if (!slot->checked)
					{
						// Mark that a check for this slot is now in progress.
						slot->connectivityCheckInProgress = true;

						// Test the slot's listening port before
						// adding it to the livecache for the first time.
						//                     
						m_checker.async_test(ep.address, std::bind(
							&PeerSlotLogic::checkComplete, this, slot->remote_endpoint(),
							ep.address, std::placeholders::_1));

						// Note that we simply discard the first Endpoint
						// that the neighbor sends when we perform the
						// listening test. They will just send us another
						// one in a few seconds.

						continue;
					}

					// If they failed the test then skip the address
					if (!slot->canAccept)
						continue;
				}

				// We only add to the livecache if the neighbor passed the
				// listening test, else we silently drop their messsage
				// since their listening port is misconfigured.
				//
				state->livecache.insert(ep);
				state->bootcache.insert(ep.address);
			}

			slot->whenAcceptEndpoints = now + Tuning::secondsPerMessage;
		}


		void PeerSlotLogic::on_cancel(SlotImp::ptr const& slot)
		{
			SharedState::Access state(m_state);

			remove(slot, state);

			if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
				"Logic cancel " << slot->remote_endpoint();
		}


		void PeerSlotLogic::broadcast()
		{
			SharedState::Access state(m_state);

			std::vector <SlotHandouts> targets;

			{
				// build list of active slots
				std::vector <SlotImp::ptr> slots;
				slots.reserve(state->slots.size());
				std::for_each(state->slots.cbegin(), state->slots.cend(),
					[&slots](Slots::value_type const& value)
				{
					if (value.second->state() == Slot::active)
						slots.emplace_back(value.second);
				});
				std::random_shuffle(slots.begin(), slots.end());

				// build target vector
				targets.reserve(slots.size());
				std::for_each(slots.cbegin(), slots.cend(),
					[&targets](SlotImp::ptr const& slot)
				{
					targets.emplace_back(slot);
				});
			}

			/* VFALCO NOTE
			This is a temporary measure. Once we know our own IP
			address, the correct solution is to put it into the Livecache
			at hops 0, and go through the regular handout path. This way
			we avoid handing our address out too frequently, which this code
			suffers from.
			*/
			// Add an entry for ourselves if:
			// 1. We want incoming
			// 2. We have slots
			// 3. We haven't failed the firewalled test
			//
			if ((!getConfig().PEER_PRIVATE) &&
				state->counts.inboundSlots() > 0)
			{
				Endpoint ep;
				ep.hops = 0;
				ep.address = beast::IP::Endpoint(
					beast::IP::AddressV4()).at_port(getConfig().peerListeningPort);
				for (auto& t : targets)
					t.insert(ep);
			}

			// build sequence of endpoints by hops
			state->livecache.hops.shuffle();
			handout(targets.begin(), targets.end(),
				state->livecache.hops.begin(),
				state->livecache.hops.end());

			// broadcast
			for (auto const& t : targets)
			{
				SlotImp::ptr const& slot(t.slot());
				auto const& list(t.list());
				if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
					"Logic sending " << slot->remote_endpoint() <<
					" with " << list.size() <<
					((list.size() == 1) ? " endpoint" : " endpoints");
				m_callback.send(slot, list);
			}
		}


		void PeerSlotLogic::redirect(SlotImp::ptr const& slot, SharedState::Access& state)
		{
			RedirectHandouts h(slot);
			state->livecache.hops.shuffle();
			handout(&h, (&h) + 1,
				state->livecache.hops.begin(),
				state->livecache.hops.end());

			if (!h.list().empty())
			{
				if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
					"Logic redirect " << slot->remote_endpoint() <<
					" with " << h.list().size() <<
					((h.list().size() == 1) ? " address" : " addresses");
				m_callback.send(slot, h.list());
			}
			else
			{
				if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
					"Logic deferred " << slot->remote_endpoint();
			}
		}

		void PeerSlotLogic::fetch(beast::SharedPtr <Source> const& source)
		{
			Source::Results results;

			{
				{
					SharedState::Access state(m_state);
					if (state->stopping)
						return;
					state->fetchSource = source;
				}

				// VFALCO NOTE The fetch is synchronous,
				//             not sure if that's a good thing.
				//
				source->fetch(results, m_journal);

				{
					SharedState::Access state(m_state);
					if (state->stopping)
						return;
					state->fetchSource = nullptr;
				}
			}

			if (!results.error)
			{
				int const count(addBootcacheAddresses(results.addresses));
				if (m_journal.info) m_journal.info << beast::leftw(18) <<
					"Logic added " << count <<
					" new " << ((count == 1) ? "address" : "addresses") <<
					" from " << source->name();
			}
			else
			{
				if (m_journal.error) m_journal.error << beast::leftw(18) <<
					"Logic failed " << "'" << source->name() << "' fetch, " <<
					results.error.message();
			}
		}


		void PeerSlotLogic::on_closed(SlotImp::ptr const& slot)
		{
			SharedState::Access state(m_state);

			remove(slot, state);

			// Mark fixed slot failure
			if (slot->fixed() && !slot->inbound() && slot->state() != Slot::active)
			{
				auto iter(state->fixed.find(slot->remote_endpoint()));
				assert(iter != state->fixed.end());
				iter->second.failure(m_clock.now());
				if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
					"Logic fixed " << slot->remote_endpoint() << " failed";
			}

			// Do state specific bookkeeping
			switch (slot->state())
			{
			case Slot::accept:
				if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
					"Logic accept " << slot->remote_endpoint() << " failed";
				break;

			case Slot::connect:
			case Slot::connected:
				state->bootcache.on_failure(slot->remote_endpoint());
				// VFALCO TODO If the address exists in the ephemeral/live
				//             endpoint livecache then we should mark the failure
				// as if it didn't pass the listening test. We should also
				// avoid propagating the address.
				break;

			case Slot::active:
				if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
					"Logic close " << slot->remote_endpoint();
				break;

			case Slot::closing:
				if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
					"Logic finished " << slot->remote_endpoint();
				break;

			default:
				assert(false);
				break;
			}
		}

		void PeerSlotLogic::on_connected(SlotImp::ptr const& slot, beast::IP::Endpoint const& local_endpoint)
		{
			if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
				"Logic connected" << slot->remote_endpoint() <<
				" on local " << local_endpoint;

			SharedState::Access state(m_state);

			// The object must exist in our table
			assert(state->slots.find(slot->remote_endpoint()) !=
				state->slots.end());
			// Assign the local endpoint now that it's known
			slot->local_endpoint(local_endpoint);

			// Check for self-connect by address
			{
				auto const iter(state->slots.find(local_endpoint));
				if (iter != state->slots.end())
				{
					Slot::ptr const& self(iter->second);
					assert(self->local_endpoint() == slot->remote_endpoint());
					if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
						"Logic dropping " << slot->remote_endpoint() <<
						" as self connect";
					m_callback.disconnect(slot, false);
					return;
				}
			}

			// Update counts
			state->counts.remove(*slot);
			slot->state(Slot::connected);
			state->counts.add(*slot);
		}

		SlotImp::ptr PeerSlotLogic::new_outbound_slot(beast::IP::Endpoint const& remote_endpoint)
		{
			if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
				"Logic connect " << remote_endpoint;

			SharedState::Access state(m_state);

			// Check for duplicate connection
			if (state->slots.find(remote_endpoint) !=
				state->slots.end())
			{
				if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
					"Logic dropping " << remote_endpoint <<
					" as duplicate connect";
				return SlotImp::ptr();
			}

			// Create the slot
			SlotImp::ptr const slot(std::make_shared <SlotImp>(
				remote_endpoint, fixed(remote_endpoint, state), m_clock));

			// Add slot to table
			std::pair <Slots::iterator, bool> result(
				state->slots.emplace(slot->remote_endpoint(),
				slot));
			// Remote address must not already exist
			assert(result.second);

			// Add to the connected address list
			state->connected_addresses.emplace(remote_endpoint.at_port(0));

			// Update counts
			state->counts.add(*slot);

			return result.first->second;
		}


		SlotImp::ptr PeerSlotLogic::new_inbound_slot(beast::IP::Endpoint const& local_endpoint,
			beast::IP::Endpoint const& remote_endpoint)
		{
			if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
				"Logic accept" << remote_endpoint <<
				" on local " << local_endpoint;

			SharedState::Access state(m_state);

			// Check for self-connect by address
			{
				auto const iter(state->slots.find(local_endpoint));
				if (iter != state->slots.end())
				{
					Slot::ptr const& self(iter->second);
					assert((self->local_endpoint() == boost::none) ||
						(self->local_endpoint() == remote_endpoint));
					if (m_journal.warning) m_journal.warning << beast::leftw(18) <<
						"Logic dropping " << remote_endpoint <<
						" as self connect";
					return SlotImp::ptr();
				}
			}

			// Create the slot
			SlotImp::ptr const slot(std::make_shared <SlotImp>(local_endpoint,
				remote_endpoint, fixed(remote_endpoint.address(), state),
				m_clock));
			// Add slot to table
			auto const result(state->slots.emplace(
				slot->remote_endpoint(), slot));
			// Remote address must not already exist
			assert(result.second);
			// Add to the connected address list
			state->connected_addresses.emplace(remote_endpoint.at_port(0));

			// Update counts
			state->counts.add(*slot);

			return result.first->second;
		}

		void PeerSlotLogic::checkComplete(beast::IP::Endpoint const& address,
			beast::IP::Endpoint const & checkedAddress, Checker::Result const& result)
		{
			if (result.error == boost::asio::error::operation_aborted)
				return;

			SharedState::Access state(m_state);
			Slots::iterator const iter(state->slots.find(address));
			SlotImp& slot(*iter->second);

			if (iter == state->slots.end())
			{
				// The slot disconnected before we finished the check
				if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
					"Logic tested " << address <<
					" but the connection was closed";
				return;
			}

			// Mark that a check for this slot is finished.
			slot.connectivityCheckInProgress = false;

			if (!result.error)
			{
				slot.checked = true;
				slot.canAccept = result.canAccept;

				if (slot.canAccept)
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic testing " << address << " succeeded";
				}
				else
				{
					if (m_journal.info) m_journal.info << beast::leftw(18) <<
						"Logic testing " << address << " failed";
				}
			}
			else
			{
				// VFALCO TODO Should we retry depending on the error?
				slot.checked = true;
				slot.canAccept = false;

				if (m_journal.error) m_journal.error << beast::leftw(18) <<
					"Logic testing " << iter->first << " with error, " <<
					result.error.message();
			}

			if (!slot.canAccept)
				state->bootcache.on_failure(address);
		}



		void PeerSlotLogic::autoconnect()
		{
			SharedState::Access state(m_state);

			// Count how many more outbound attempts to make
			//
			auto needed(state->counts.attempts_needed());
			if (needed == 0)
				return;

			ConnectHandouts h(needed, m_squelches);

			// Make sure we don't connect to already-connected entries.
			squelch_slots(state);

			// 1. Use Fixed if:
			//    Fixed active count is below fixed count AND
			//      ( There are eligible fixed addresses to try OR
			//        Any outbound attempts are in progress)
			//
			if (state->counts.fixed_active() < state->fixed.size())
			{
				get_fixed(needed, h.list(), state);

				if (!h.list().empty())
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic connect " << h.list().size() << " fixed";
					m_callback.connect(h.list());
					return;
				}

				if (state->counts.attempts() > 0)
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic waiting on " <<
						state->counts.attempts() << " attempts";
					return;
				}
			}


			if (getConfig().RUN_STANDALONE ||
				getConfig().PEER_PRIVATE ||
				state->counts.out_active() >= getConfig().PEERS_MAX)
				return;

			// 2. Use Livecache if:
			//    There are any entries in the cache OR
			//    Any outbound attempts are in progress
			//
			{
				state->livecache.hops.shuffle();
				handout(&h, (&h) + 1,
					state->livecache.hops.rbegin(),
					state->livecache.hops.rend());
				if (!h.list().empty())
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic connect " << h.list().size() << " live " <<
						((h.list().size() > 1) ? "endpoints" : "endpoint");
					m_callback.connect(h.list());
					return;
				}
				else if (state->counts.attempts() > 0)
				{
					if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
						"Logic waiting on " <<
						state->counts.attempts() << " attempts";
					return;
				}
			}

			/*  3. Bootcache refill
			If the Bootcache is empty, try to get addresses from the current
			set of Sources and add them into the Bootstrap cache.

			Pseudocode:
			If (    domainNames.count() > 0 AND (
			unusedBootstrapIPs.count() == 0
			OR activeNameResolutions.count() > 0) )
			ForOneOrMore (DomainName that hasn't been resolved recently)
			Contact DomainName and add entries to the unusedBootstrapIPs
			return;
			*/

			// 4. Use Bootcache if:
			//    There are any entries we haven't tried lately
			//
			for (auto iter(state->bootcache.begin());
				!h.full() && iter != state->bootcache.end(); ++iter)
				h.try_insert(*iter);

			if (!h.list().empty())
			{
				if (m_journal.debug) m_journal.debug << beast::leftw(18) <<
					"Logic connect " << h.list().size() << " boot " <<
					((h.list().size() > 1) ? "addresses" : "address");
				m_callback.connect(h.list());
				return;
			}

			// If we get here we are stuck
		}

	}
}
