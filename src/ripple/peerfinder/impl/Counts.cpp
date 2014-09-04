
#include "ripple_core/functional/Config.h"
#include "Counts.h"

namespace ripple {
	namespace PeerFinder {

		bool Counts::can_activate(Slot const& s) const
		{
			// Must be handshaked and in the right state
			assert(s.state() == Slot::connected || s.state() == Slot::accept);

			if (s.fixed() || s.cluster())
				return true;

			if (s.inbound())
				return m_in_active < m_in_max;

			return m_out_active < getConfig().PEERS_MAX;
		}

		void Counts::onConfig()
		{
			if (getConfig().PEER_PRIVATE)
			{
				m_in_max = 0;
			}
			else
			{
				m_in_max = getConfig().PEERS_MAX - getConfig().PEERS_RESERVE_OUT;
			}
		}

		int Counts::outboundSlotsFree() const
		{
			if (m_out_active < getConfig().PEERS_MAX)
				return getConfig().PEERS_MAX - m_out_active;
			return 0;
		}

		std::size_t Counts::attempts_needed() const
		{
			if (m_attempts >= 20) // TODO: add this to the .cfg
				return 0;
			return 20 - m_attempts;
		}




	}
}