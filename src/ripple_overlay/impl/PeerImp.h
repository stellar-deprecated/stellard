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

#ifndef RIPPLE_OVERLAY_PEERIMP_H_INCLUDED
#define RIPPLE_OVERLAY_PEERIMP_H_INCLUDED

#include <boost/thread/mutex.hpp>
#include <boost/make_shared.hpp>
#include "../api/predicates.h"

#include "ripple/common/MultiSocket.h"
#include "ripple_data/protocol/Protocol.h"
#include "ripple/validators/ripple_validators.h"
#include "ripple/peerfinder/ripple_peerfinder.h"
#include "ripple_app/misc/ProofOfWork.h"
#include "ripple_app/misc/ProofOfWorkFactory.h"
#include "ripple_overlay/impl/OverlayImpl.h"
#include "ripple_app/misc/SerializedTransaction.h"
#include "ripple_app/ledger/LedgerProposal.h"
#include "ripple_app/ledger/SerializedValidation.h"
#include "ripple_basics/log/LogPartition.h"
#include "ripple_basics/utility/PlatformMacros.h"

#include <cstdint>

namespace ripple {

typedef boost::asio::ip::tcp::socket NativeSocketType;

class Job;
class PeerImp;

std::string to_string (Peer const& peer);
std::ostream& operator<< (std::ostream& os, Peer const& peer);

std::string to_string (Peer const* peer);
std::ostream& operator<< (std::ostream& os, Peer const* peer);

std::string to_string (PeerImp const& peer);
std::ostream& operator<< (std::ostream& os, PeerImp const& peer);

std::string to_string (PeerImp const* peer);
std::ostream& operator<< (std::ostream& os, PeerImp const* peer);

//------------------------------------------------------------------------------

class PeerImp 
    : public Peer
    , public boost::enable_shared_from_this <PeerImp>
    , private beast::LeakChecked <Peer>
{
private:
    /** Time alloted for a peer to send a HELLO message (DEPRECATED) */
    static const boost::posix_time::seconds nodeVerifySeconds;

    /** The clock drift we allow a remote peer to have */
    static const std::uint32_t clockToleranceDeltaSeconds = 20;

    /** The length of the smallest valid finished message */
    static const size_t sslMinimumFinishedLength = 12;

    //--------------------------------------------------------------------------
    /** We have accepted an inbound connection.

        The connection state transitions from `stateConnect` to `stateConnected`
        as `stateConnect`.
    */
    void accept ()
    {
        m_journal.info << "Accepted " << m_remoteAddress;

        m_socket->set_verify_mode (boost::asio::ssl::verify_none);
        m_socket->async_handshake (
            boost::asio::ssl::stream_base::server,
            m_strand.wrap (boost::bind (
                &PeerImp::handleStart,
                boost::static_pointer_cast <PeerImp> (shared_from_this ()),
                boost::asio::placeholders::error)));
    }

    /** Attempt an outbound connection.

        The connection may fail (for a number of reasons) and we do not know
        what will happen at this point.

        The connection state does not transition with this function and remains
        as `stateConnecting`.
    */
	void connect();

public:
    /** Current state */
    enum State
    {
        /** An connection is being established (outbound) */
         stateConnecting

        /** Connection has been successfully established */
        ,stateConnected

        /** Handshake has been received from this peer */
        ,stateHandshaked

        /** Running the Ripple protocol actively */
        ,stateActive

        /** Gracefully closing */
        ,stateGracefulClose
    };

    typedef boost::shared_ptr <PeerImp> ptr;

    NativeSocketType m_owned_socket;

    beast::Journal m_journal;

    // A unique identifier (up to a restart of stellard) for this particular
    // peer instance. A peer that disconnects will, upon reconnection, get a
    // new ID.
    ShortId m_shortId;

    // Updated at each stage of the connection process to reflect
    // the current conditions as closely as possible. This includes
    // the case where we learn the true IP via a PROXY handshake.
    beast::IP::Endpoint m_remoteAddress;

    // These is up here to prevent warnings about order of initializations
    //
    Resource::Manager& m_resourceManager;
    PeerFinder::Manager& m_peerFinder;
    OverlayImpl& m_overlay;
    bool m_inbound;

    std::unique_ptr <MultiSocket> m_socket;
    boost::asio::io_service::strand m_strand;

    State           m_state;          // Current state
    bool            m_detaching;      // True if detaching.
    bool            m_clusterNode;    // True if peer is a node in our cluster
    RippleAddress   m_nodePublicKey;  // Node public key of peer.
    std::string     m_nodeName;

    // Both sides of the peer calculate this value and verify that it matches
    // to detect/prevent man-in-the-middle attacks.
    //
    uint256 m_secureCookie;

    // The indices of the smallest and largest ledgers this peer has available
    //
    LedgerIndex m_minLedger;
    LedgerIndex m_maxLedger;

    uint256 m_closedLedgerHash;
    uint256 m_previousLedgerHash;

    std::list<uint256>    m_recentLedgers;
    std::list<uint256>    m_recentTxSets;
    mutable boost::mutex  m_recentLock;

    boost::asio::deadline_timer         m_timer;

    std::vector<uint8_t>                m_readBuffer;
    std::list<Message::pointer>   mSendQ;
    Message::pointer              mSendingPacket;
    protocol::TMStatusChange            mLastStatus;
    protocol::TMHello                   mHello;

    Resource::Consumer m_usage;

    // The slot assigned to us by PeerFinder
    PeerFinder::Slot::ptr m_slot;

    // True if close was called
    bool m_was_canceled;

    //--------------------------------------------------------------------------
    /** New incoming peer from the specified socket */
    PeerImp (
        NativeSocketType&& socket,
        beast::IP::Endpoint remoteAddress,
        OverlayImpl& overlay,
        Resource::Manager& resourceManager,
        PeerFinder::Manager& peerFinder,
        PeerFinder::Slot::ptr const& slot,
        boost::asio::ssl::context& ssl_context,
        MultiSocket::Flag flags)
            : m_owned_socket (std::move (socket))
            , m_journal (LogPartition::getJournal <Peer> ())
            , m_shortId (0)
            , m_remoteAddress (remoteAddress)
            , m_resourceManager (resourceManager)
            , m_peerFinder (peerFinder)
            , m_overlay (overlay)
            , m_inbound (true)
            , m_socket (MultiSocket::New (
                m_owned_socket, ssl_context, flags.asBits ()))
            , m_strand (m_owned_socket.get_io_service())
            , m_state (stateConnected)
            , m_detaching (false)
            , m_clusterNode (false)
            , m_minLedger (0)
            , m_maxLedger (0)
            , m_timer (m_owned_socket.get_io_service())
            , m_slot (slot)
            , m_was_canceled (false)
    {
    }
        
    /** New outgoing peer
        @note Construction of outbound peers is a two step process: a second
              call is needed (to connect or accept) but we cannot make it from
              inside the constructor because you cannot call shared_from_this
              from inside constructors.
    */
    PeerImp (
        beast::IP::Endpoint remoteAddress,
        boost::asio::io_service& io_service,
        OverlayImpl& overlay,
        Resource::Manager& resourceManager,
        PeerFinder::Manager& peerFinder,
        PeerFinder::Slot::ptr const& slot,
        boost::asio::ssl::context& ssl_context,
        MultiSocket::Flag flags)
            : m_owned_socket (io_service)
            , m_journal (LogPartition::getJournal <Peer> ())
            , m_shortId (0)
            , m_remoteAddress (remoteAddress)
            , m_resourceManager (resourceManager)
            , m_peerFinder (peerFinder)
            , m_overlay (overlay)
            , m_inbound (false)
            , m_socket (MultiSocket::New (
                io_service, ssl_context, flags.asBits ()))
            , m_strand (io_service)
            , m_state (stateConnecting)
            , m_detaching (false)
            , m_clusterNode (false)
            , m_minLedger (0)
            , m_maxLedger (0)
            , m_timer (io_service)
            , m_slot (slot)
            , m_was_canceled (false)
    {
    }
    
    virtual ~PeerImp ()
    {
        m_overlay.remove (m_slot);
    }

    PeerImp (PeerImp const&) = delete;
    PeerImp& operator= (PeerImp const&) = delete;

    MultiSocket& getStream ()
    {
        return *m_socket;
    }

    static char const* getCountedObjectName () { return "Peer"; }

    //--------------------------------------------------------------------------

    State state() const
    {
        return m_state;
    }

    void state (State new_state)
    {
        m_state = new_state;
    }

    //--------------------------------------------------------------------------
    /** Disconnect a peer

        The peer transitions from its current state into `stateGracefulClose`

        @param rsn a code indicating why the peer was disconnected
        @param onIOStrand true if called on an I/O strand. It if is not, then
               a callback will be queued up.
    */

    void drainAndClose (const char* rsn, bool graceful = true)
    {
        if (m_journal.trace)
            m_journal.trace << "PeerImp::drainAndClose";

        // This function should only ever be called form the strand.
        // If not, the caller is racing on the socket with the strand.
        bassert(m_strand.running_in_this_thread());

        // Setting m_detaching here will inhibit further queueing-up of writes
        // in mSendQ, or other work in m_strand.
        m_detaching = true;

        if (mSendingPacket || !mSendQ.empty()) {
            if (m_journal.trace)
                m_journal.trace << "PeerImp::detach() postponing disconnect, "
                                << "pending writes to " << m_remoteAddress;
            return;
        }

        // Only actually shut the socket down once; multiple drainAndClose()
        // calls on the strand should be idempotent.
        if (m_state != stateGracefulClose)
        {
            if (m_was_canceled)
                m_peerFinder.on_cancel (m_slot);
            else
                m_peerFinder.on_closed (m_slot);

            if (m_state == stateActive)
                m_overlay.onPeerDisconnect (shared_from_this ());

            m_state = stateGracefulClose;

            if (m_clusterNode && m_journal.active(beast::Journal::Severity::kWarning))
                m_journal.warning << "Cluster peer " << m_nodeName <<
                                     " detached: " << rsn;

            bassert (mSendQ.empty ());

            (void) m_timer.cancel ();

            if (graceful)
            {
                if (m_journal.trace)
                    m_journal.trace << "PeerImp::detach() => socket->async_shutdown() "
                                    << m_remoteAddress;
                m_socket->async_shutdown (
                    m_strand.wrap ( boost::bind(
                        &PeerImp::handleShutdown,
                        boost::static_pointer_cast <PeerImp> (shared_from_this ()),
                        boost::asio::placeholders::error)));
            }
            else
            {
                if (m_journal.trace)
                    m_journal.trace << "PeerImp::detach() => socket->cancel() "
                                    << m_remoteAddress;
                m_socket->cancel ();
            }

            // VFALCO TODO Stop doing this.
            if (m_nodePublicKey.isValid ())
                m_nodePublicKey.clear ();       // Be idempotent.
        }
    }

    void detach (const char* rsn, bool graceful = true) {
        if (m_journal.trace)
            m_journal.trace << "PeerImp::detach(): posting drainAndClose() to strand for "
                            << m_remoteAddress;
        m_strand.post (BIND_TYPE (&PeerImp::drainAndClose,
                                  shared_from_this (), rsn, graceful));
    }


    /** Close the connection. */
    void close (bool graceful)
    {
        m_was_canceled = true;
        detach ("stop", graceful);
    }

    /** Outbound connection attempt has completed (not necessarily successfully)

        The connection may fail for a number of reasons. Perhaps we do not have
        a route to the remote endpoint, or there is no server listening at that
        address.

        If the connection succeeded, we transition to the `stateConnected` state
        and move on.

        If the connection failed, we simply disconnect.

        @param ec indicates success or an error code.
    */
    void onConnect (boost::system::error_code ec)
    {
        if (m_detaching)
            return;

        NativeSocketType::endpoint_type local_endpoint;

        if (! ec)
            local_endpoint = m_socket->this_layer <
                NativeSocketType> ().local_endpoint (ec);

        if (ec)
        {
            // VFALCO NOTE This log statement looks like ass
            m_journal.info <<
                "Connect to " << m_remoteAddress <<
                " failed: " << ec.message();
            // This should end up calling onPeerClosed()
            detach ("hc");
            return;
        }

        bassert (m_state == stateConnecting);
        m_state = stateConnected;

        m_peerFinder.on_connected (m_slot,
            beast::IPAddressConversion::from_asio (local_endpoint));

        m_socket->set_verify_mode (boost::asio::ssl::verify_none);
        m_socket->async_handshake (
            boost::asio::ssl::stream_base::client,
            m_strand.wrap (boost::bind (&PeerImp::handleStart,
                boost::static_pointer_cast <PeerImp> (shared_from_this ()),
                    boost::asio::placeholders::error)));
    }

    /** Indicates that the peer must be activated.
        A peer is activated after the handshake is completed and if it is not
        a second connection from a peer that we already have. Once activated
        the peer transitions to `stateActive` and begins operating.
    */
    void activate ()
    {
        bassert (m_state == stateHandshaked);
        m_state = stateActive;
        bassert(m_shortId == 0);
        m_shortId = m_overlay.next_id();
        m_overlay.onPeerActivated(shared_from_this ());
    }

    void start ()
    {
        if (m_inbound)
            accept ();
        else
            connect ();
    }

    //--------------------------------------------------------------------------
    std::string getClusterNodeName() const
    {
        return m_nodeName;
    }

    //--------------------------------------------------------------------------

    void queueOrWritePacket (const Message::pointer& packet)
    {
        unsigned len;
        int type;

        bassert(m_strand.running_in_this_thread());

        if (m_journal.trace) {
            // Only extract the len and type if we're going to be logging them.
            len = Message::getLength(packet->getBuffer());
            type = Message::getType(packet->getBuffer());
        }

        // If we're still sending something, requeue this event
        // in the strand, _not_ mSendQ. That's for packets that haven't even
        // been dispatched to the strand yet.
        if (mSendingPacket) {
            if (m_journal.trace)
                m_journal.trace << "PeerImp::queueOrWritePacket() queueing packet type="
                                << type << ", len=" << len << " to mSendQ";
            mSendQ.push_back (packet);
            return;
        }

        if (m_journal.trace) {
            unsigned len = Message::getLength(packet->getBuffer());
            int type = Message::getType(packet->getBuffer());
            m_journal.trace << "PeerImp::queueOrWritePacket() => async_write "
                            << "type=" << type
                            << ", len=" << len
                            << " to " << m_remoteAddress
                            << ", m_detaching=" << m_detaching;
        }

        mSendingPacket = packet;
        boost::asio::async_write (getStream (),
            boost::asio::buffer (packet->getBuffer ()),
            m_strand.wrap (boost::bind (
                &PeerImp::handleWrite,
                boost::static_pointer_cast <PeerImp> (shared_from_this ()),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred)));
    }

    // External interaface to packet-sending; drops null packets, as well as
    // _all_ packets once shutdown has started. Posts queueOrWritePacket to
    // the strand.
    void sendPacket (const Message::pointer& packet, bool onStrand)
    {
        unsigned len;
        int type;

        if (!packet) {
            if (m_journal.trace)
                m_journal.trace << "PeerImp::sendPacket(null), ignoring";
            return;
        }

        if (m_journal.trace) {
            // Only extract the len and type if we're going to be logging them.
            len = Message::getLength(packet->getBuffer());
            type = Message::getType(packet->getBuffer());
        }

        if (m_detaching) {
            if (m_journal.trace)
                m_journal.trace << "PeerImp::sendPacket() while detaching, dropping packet "
                                << "type=" << type << ", len=" << len;
            return;
        }

        if (m_journal.trace)
            m_journal.trace << "PeerImp::sendPacket() posting packet type="
                            << type << ", len=" << len << " to strand";

        m_strand.post (BIND_TYPE (
                           &PeerImp::queueOrWritePacket, shared_from_this (), packet));
    }

    void sendGetPeers ()
    {
        // Ask peer for known other peers.
        protocol::TMGetPeers msg;

        msg.set_doweneedthis (1);

        Message::pointer packet = boost::make_shared<Message> (
            msg, protocol::mtGET_PEERS);

        sendPacket (packet, true);
    }

    void charge (Resource::Charge const& fee)
    {
         if ((m_usage.charge (fee) == Resource::drop) && m_usage.disconnect ())
             detach ("resource");
    }

    static void charge (boost::weak_ptr <Peer>& peer, Resource::Charge const& fee)
    {
        Peer::ptr p (peer.lock());

        if (p != nullptr)
            p->charge (fee);
    }

	Json::Value json();

    bool isInCluster () const
    {
        return m_clusterNode;
    }

    uint256 const& getClosedLedgerHash () const
    {
        return m_closedLedgerHash;
    }

    bool hasLedger (uint256 const& hash, std::uint32_t seq) const
    {
        boost::mutex::scoped_lock sl(m_recentLock);

        if ((seq != 0) && (seq >= m_minLedger) && (seq <= m_maxLedger))
            return true;

        BOOST_FOREACH (uint256 const & ledger, m_recentLedgers)
        {
            if (ledger == hash)
                return true;
        }

        return false;
    }

    void ledgerRange (std::uint32_t& minSeq, std::uint32_t& maxSeq) const
    {
        boost::mutex::scoped_lock sl(m_recentLock);

        minSeq = m_minLedger;
        maxSeq = m_maxLedger;
    }

    bool hasTxSet (uint256 const& hash) const
    {
        boost::mutex::scoped_lock sl(m_recentLock);
        BOOST_FOREACH (uint256 const & set, m_recentTxSets)

        if (set == hash)
            return true;

        return false;
    }

    Peer::ShortId getShortId () const
    {
        return m_shortId;
    }

    const RippleAddress& getNodePublic () const
    {
        return m_nodePublicKey;
    }

    void cycleStatus ()
    {
        m_previousLedgerHash = m_closedLedgerHash;
        m_closedLedgerHash.zero ();
    }

    bool supportsVersion (int version)
    {
        return mHello.has_protoversion () && (mHello.protoversion () >= version);
    }

    bool hasRange (std::uint32_t uMin, std::uint32_t uMax)
    {
        return (uMin >= m_minLedger) && (uMax <= m_maxLedger);
    }

    beast::IP::Endpoint getRemoteAddress() const
    {
        return m_remoteAddress;
    }

private:
    void handleShutdown (boost::system::error_code const& ec)
    {
        if (m_detaching)
            return;

        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
        {
            m_journal.info << "Shutdown: " << ec.message ();
            detach ("hsd");
            return;
        }
    }

    void handleWrite (boost::system::error_code const& ec, size_t bytes)
    {

        if (m_journal.trace)
            m_journal.trace << "handleWrite() bytes=" << bytes
                            << ", m_detaching=" << m_detaching
                            << ", m_remoteAddress=" << m_remoteAddress;

        // This function should only ever be called form the strand.
        // If not, the caller is racing on the socket with the strand.
        bassert(m_strand.running_in_this_thread());

        mSendingPacket.reset ();

        if (ec == boost::asio::error::operation_aborted) {
            m_journal.trace << "handleWrite() : operation aborted";
            return;
        }

        if (ec)
        {
            m_journal.info << "Write: " << ec.message ();
            detach ("hw");
            return;
        }

        if (mSendQ.empty ())
        {
            if (m_detaching)
            {
                // If we're detaching, and just finished a write, and the queue is empty, it's very
                // likely that a drainAndClose () callback snuck into the strand *before* the final
                // write, and was dropped awaiting the completion of that write. So we re-post the
                // drainAndClose here, to finish it off.  This is idempotent.
                m_strand.post (BIND_TYPE (&PeerImp::drainAndClose,
                                          shared_from_this (), "drained queue", true));
            }
        }
        else
        {
            Message::pointer packet = mSendQ.front ();

            if (packet)
            {
                mSendQ.pop_front ();
                queueOrWritePacket (packet);
            }
        }
    }

    void handleReadHeader (boost::system::error_code const& ec,
                           std::size_t bytes)
    {
        if (m_detaching)
            return;

        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
        {
            m_journal.info << "ReadHeader: " << ec.message ();
            detach ("hrh1");
            return;
        }

        unsigned msg_len = Message::getLength (m_readBuffer);

        // WRITEME: Compare to maximum message length, abort if too large
        if ((msg_len > (32 * 1024 * 1024)) || (msg_len == 0))
        {
            detach ("hrh2");
            return;
        }

        startReadBody (msg_len);
    }

	void handleReadBody(boost::system::error_code const& ec,
		std::size_t bytes);

    // We have an encrypted connection to the peer.
    // Have it say who it is so we know to avoid redundant connections.
    // Establish that it really who we are talking to by having it sign a
    // connection detail. Also need to establish no man in the middle attack
    // is in progress.
    void handleStart (boost::system::error_code const& ec)
    {
        if (m_detaching)
            return;

        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
        {
            m_journal.info << "Handshake: " << ec.message ();
            detach ("hs");
            return;
        }

        if (m_inbound)
            m_usage = m_resourceManager.newInboundEndpoint (m_remoteAddress);
        else
            m_usage = m_resourceManager.newOutboundEndpoint (m_remoteAddress);

        if (m_usage.disconnect ())
        {
            detach ("resource");
            return;
        }

        if(!sendHello ())
        {
            m_journal.error << "Unable to send HELLO to " << m_remoteAddress;
            detach ("hello");
            return;
        }

        startReadHeader ();
    }

    void handleVerifyTimer (boost::system::error_code const& ec)
    {
        if (m_detaching)
            return;

        if (ec == boost::asio::error::operation_aborted)
        {
            // Timer canceled because deadline no longer needed.
        }
        else if (ec)
        {
            m_journal.info << "Peer verify timer error";
        }
        else
        {
            //  m_journal.info << "Verify: Peer failed to verify in time.";

            detach ("hvt");
        }
    }

	void processReadBuffer();

	void startReadHeader();

    void startReadBody (unsigned msg_len)
    {
        // The first Message::kHeaderBytes bytes of m_readbuf already
        // contains the header. Expand it to fit in the body as well, and
        // start async read into the body.

        if (!m_detaching)
        {
            m_readBuffer.resize (Message::kHeaderBytes + msg_len);

            boost::asio::async_read (getStream (),
                boost::asio::buffer (
                    &m_readBuffer [Message::kHeaderBytes], msg_len),
                m_strand.wrap (boost::bind (
                    &PeerImp::handleReadBody,
                    boost::static_pointer_cast <PeerImp> (shared_from_this ()),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred)));
        }
    }


    /** Hashes the latest finished message from an SSL stream

        @param sslSession the session to get the message from.
        @param hash       the buffer into which the hash of the retrieved
                          message will be saved. The buffer MUST be at least
                          64 bytes long.
        @param getMessage a pointer to the function to call to retrieve the
                          finished message. This be either:
                          `SSL_get_finished` or
                          `SSL_get_peer_finished`.

        @return `true` if successful, `false` otherwise.

    */
    bool hashLatestFinishedMessage (const SSL *sslSession, unsigned char *hash,
        size_t (*getFinishedMessage)(const SSL *, void *buf, size_t))
    {
        unsigned char buf[1024];

        // Get our finished message and hash it.
        std::memset(hash, 0, 64);

        size_t len = getFinishedMessage (sslSession, buf, sizeof (buf));

        if(len < sslMinimumFinishedLength)
            return false;

        SHA512 (buf, len, hash);

        return true;
    }

    /** Generates a secure cookie to protect against man-in-the-middle attacks

        This function should never fail under normal circumstances and regular
        server operation.

        A failure prevents the cookie value from being calculated which is an
        important component of connection security. If this function fails, a
        secure connection cannot be established and the link MUST be dropped.

        @return `true` if the cookie was generated, `false` otherwise.

        @note failure is an exceptional situation - it should never happen and
              will almost always indicate an active man-in-the-middle attack is
              taking place.
    */
	bool calculateSessionCookie();

    /** Perform a secure handshake with the peer at the other end.

        If this function returns false then we cannot guarantee that there
        is no active man-in-the-middle attack taking place and the link
        MUST be disconnected.

        @return true if successful, false otherwise.
    */
	bool sendHello();

	void recvHello(protocol::TMHello& packet);

	void recvCluster(protocol::TMCluster& packet);


	void recvTransaction(protocol::TMTransaction& packet);
   

	void recvValidation(const boost::shared_ptr<protocol::TMValidation>& packet);

    void recvGetValidation (protocol::TMGetValidations& packet)
    {
    }

    void recvContact (protocol::TMContact& packet)
    {
    }

    void recvGetContacts (protocol::TMGetContacts& packet)
    {
    }

    // Return a list of your favorite people
    // TODO: filter out all the LAN peers
    void recvGetPeers (protocol::TMGetPeers& packet)
    {
#if 0
        protocol::TMPeers peers;

        // CODEME: This is deprecated because of PeerFinder, but populate the
        // response with some data here anyways, and send if non-empty.

        sendPacket (
            boost::make_shared<Message> (peers, protocol::mtPEERS),
            true);
#endif
    }

    // TODO: filter out all the LAN peers
    void recvPeers (protocol::TMPeers& packet)
    {
        std::vector <beast::IP::Endpoint> list;
        list.reserve (packet.nodes().size());
        for (int i = 0; i < packet.nodes ().size (); ++i)
        {
            in_addr addr;

            addr.s_addr = packet.nodes (i).ipv4 ();

            {
                beast::IP::AddressV4 v4 (ntohl (addr.s_addr));
                beast::IP::Endpoint address (v4, packet.nodes (i).ipv4port ());
                list.push_back (address);
            }
        }

        if (! list.empty())
            m_peerFinder.on_legacy_endpoints (list);
    }

    void recvEndpoints (protocol::TMEndpoints& packet)
    {
        std::vector <PeerFinder::Endpoint> endpoints;

        endpoints.reserve (packet.endpoints().size());

        for (int i = 0; i < packet.endpoints ().size (); ++i)
        {
            PeerFinder::Endpoint endpoint;
            protocol::TMEndpoint const& tm (packet.endpoints(i));

            // hops
            endpoint.hops = tm.hops();

            // ipv4
            if (endpoint.hops > 0)
            {
                in_addr addr;
                addr.s_addr = tm.ipv4().ipv4();
                beast::IP::AddressV4 v4 (ntohl (addr.s_addr));
                endpoint.address = beast::IP::Endpoint (v4, tm.ipv4().ipv4port ());
            }
            else
            {
                // This Endpoint describes the peer we are connected to.
                // We will take the remote address seen on the socket and
                // store that in the IP::Endpoint. If this is the first time,
                // then we'll verify that their listener can receive incoming
                // by performing a connectivity test.
                //
                endpoint.address = m_remoteAddress.at_port (
                    tm.ipv4().ipv4port ());
            }
        
            endpoints.push_back (endpoint);
        }

        if (! endpoints.empty())
            m_peerFinder.on_endpoints (m_slot, endpoints);
    }

	void recvGetObjectByHash(const boost::shared_ptr<protocol::TMGetObjectByHash>& ptr);

	void recvPing(protocol::TMPing& packet);

    void recvErrorMessage (protocol::TMErrorMsg& packet)
    {
    }

    void recvSearchTransaction (protocol::TMSearchTransaction& packet)
    {
    }

    void recvGetAccount (protocol::TMGetAccount& packet)
    {
    }

    void recvAccount (protocol::TMAccount& packet)
    {
    }

	void recvGetLedger(boost::shared_ptr<protocol::TMGetLedger> const& packet);

    /** A peer has sent us transaction set data */
	static void peerTXData(Job&,
		boost::weak_ptr <Peer> wPeer,
		uint256 const& hash,
		boost::shared_ptr <protocol::TMLedgerData> pPacket,
		beast::Journal journal);

	void recvLedger(boost::shared_ptr<protocol::TMLedgerData> const& packet_ptr);

	void recvStatus(protocol::TMStatusChange& packet);

	void recvPropose(const boost::shared_ptr<protocol::TMProposeSet>& packet);

	void recvHaveTxSet(protocol::TMHaveTransactionSet& packet);

	void recvProofWork(protocol::TMProofWork& packet);

    void addLedger (uint256 const& hash)
    {
        boost::mutex::scoped_lock sl(m_recentLock);
        BOOST_FOREACH (uint256 const & ledger, m_recentLedgers)

        if (ledger == hash)
            return;

        if (m_recentLedgers.size () == 128)
            m_recentLedgers.pop_front ();

        m_recentLedgers.push_back (hash);
    }
    
	void getLedger(protocol::TMGetLedger& packet);
    
    // This is dispatched by the job queue
    static void sGetLedger (boost::weak_ptr<PeerImp> wPeer, 
        boost::shared_ptr <protocol::TMGetLedger> packet)
    {
        boost::shared_ptr<PeerImp> peer = wPeer.lock ();
        
        if (peer)
            peer->getLedger (*packet);
    }
    
    void addTxSet (uint256 const& hash)
    {
        boost::mutex::scoped_lock sl(m_recentLock);

        if(std::find (m_recentTxSets.begin (), m_recentTxSets.end (), hash) != m_recentTxSets.end ())
        	return;
        
        if (m_recentTxSets.size () == 128)
            m_recentTxSets.pop_front ();

        m_recentTxSets.push_back (hash);
    }

	void doFetchPack(const boost::shared_ptr<protocol::TMGetObjectByHash>& packet);

	void doProofOfWork(Job&, boost::weak_ptr <Peer> peer, ProofOfWork::pointer pow);

	static void checkTransaction(Job&, int flags, SerializedTransaction::pointer stx, boost::weak_ptr<Peer> peer);

    // Called from our JobQueue
	static void checkPropose(Job& job, Overlay* pPeers, boost::shared_ptr<protocol::TMProposeSet> packet,
		LedgerProposal::pointer proposal, uint256 consensusLCL, RippleAddress nodePublic,
		boost::weak_ptr<Peer> peer, bool fromCluster);

	static void checkValidation(Job&, Overlay* pPeers, SerializedValidation::pointer val, bool isTrusted, bool isCluster,
		boost::shared_ptr<protocol::TMValidation> packet, boost::weak_ptr<Peer> peer);
};

//------------------------------------------------------------------------------



//------------------------------------------------------------------------------

// to_string should not be used we should just use lexical_cast maybe

inline std::string to_string (PeerImp const& peer)
{
    if (peer.isInCluster())
        return peer.getClusterNodeName();

    return peer.getRemoteAddress().to_string();
}

inline std::string to_string (PeerImp const* peer)
{
    return to_string (*peer);
}

inline std::ostream& operator<< (std::ostream& os, PeerImp const& peer)
{
    os << to_string (peer);

    return os;
}

inline std::ostream& operator<< (std::ostream& os, PeerImp const* peer)
{
    os << to_string (peer);

    return os;
}

//------------------------------------------------------------------------------

inline std::string to_string (Peer const& peer)
{
    if (peer.isInCluster())
        return peer.getClusterNodeName();

    return peer.getRemoteAddress().to_string();
}

inline std::string to_string (Peer const* peer)
{
    return to_string (*peer);
}

inline std::ostream& operator<< (std::ostream& os, Peer const& peer)
{
    os << to_string (peer);

    return os;
}

inline std::ostream& operator<< (std::ostream& os, Peer const* peer)
{
    os << to_string (peer);

    return os;
}

}

#endif
