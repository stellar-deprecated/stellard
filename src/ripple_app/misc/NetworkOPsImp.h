#include "../../ripple_overlay/api/predicates.h"
#include "../../ripple/common/jsonrpc_fields.h"

#include "../../beast/modules/beast_core/thread/DeadlineTimer.h"
#include "../../beast/modules/beast_core/system/SystemStats.h"
#include <tuple>

namespace ripple {

	class FeeVoteLog;
	template <>
	char const*
		LogPartition::getPartitionName <FeeVoteLog>() { return "FeeVote"; }

	class NetworkOPsImp
		: public NetworkOPs
		, public beast::DeadlineTimer::Listener
		, public beast::LeakChecked <NetworkOPsImp>
	{
	public:
		enum Fault
		{
			// exceptions these functions can throw
			IO_ERROR = 1,
			NO_NETWORK = 2,
		};

	public:
		// VFALCO TODO Make LedgerMaster a SharedPtr or a reference.
		//
		NetworkOPsImp(clock_type& clock, LedgerMaster& ledgerMaster,
			Stoppable& parent, beast::Journal journal)
			: NetworkOPs(parent)
			, m_clock(clock)
			, m_journal(journal)
			, m_localTX(LocalTxs::New())
			, m_feeVote(make_FeeVote(10, 20 * SYSTEM_CURRENCY_PARTS,
			5 * SYSTEM_CURRENCY_PARTS, LogPartition::getJournal <FeeVoteLog>()))
			, mMode(omDISCONNECTED)
			, mNeedNetworkLedger(false)
			, mProposing(false)
			, mValidating(false)
			, m_amendmentBlocked(false)
			, m_heartbeatTimer(this)
			, m_clusterTimer(this)
			, m_ledgerMaster(ledgerMaster)
			, mCloseTimeOffset(0)
			, mLastCloseProposers(0)
			, mLastCloseConvergeTime(1000 * LEDGER_IDLE_INTERVAL)
			, mLastCloseTime(0)
			, mLastValidationTime(0)
			, mFetchPack("FetchPack", 65536, 45, clock,
			LogPartition::getJournal <TaggedCacheLog>())
			, mFetchSeq(0)
			, mLastLoadBase(256)
			, mLastLoadFactor(256)
		{
		}

		~NetworkOPsImp()
		{
		}

		// network information
		std::uint32_t getNetworkTimeNC();                 // Our best estimate of wall time in seconds from 1/1/2000
		std::uint32_t getCloseTimeNC();                   // Our best estimate of current ledger close time
		std::uint32_t getValidationTimeNC();              // Use *only* to timestamp our own validation
		void closeTimeOffset(int);
		boost::posix_time::ptime getNetworkTimePT();
		std::uint32_t getLedgerID(uint256 const& hash);
		std::uint32_t getCurrentLedgerID();
		OperatingMode getOperatingMode()
		{
			return mMode;
		}
		std::string strOperatingMode();

		Ledger::pointer     getClosedLedger()
		{
			return m_ledgerMaster.getClosedLedger();
		}
		Ledger::pointer     getValidatedLedger()
		{
			return m_ledgerMaster.getValidatedLedger();
		}
		Ledger::pointer     getPublishedLedger()
		{
			return m_ledgerMaster.getPublishedLedger();
		}
		Ledger::pointer     getCurrentLedger()
		{
			return m_ledgerMaster.getCurrentLedger();
		}
		Ledger::pointer getLedgerByHash(uint256 const& hash)
		{
			return m_ledgerMaster.getLedgerByHash(hash);
		}
		Ledger::pointer getLedgerBySeq(const std::uint32_t seq);
		void            missingNodeInLedger(const std::uint32_t seq);

		uint256         getClosedLedgerHash()
		{
			return m_ledgerMaster.getClosedLedger()->getHash();
		}

		// Do we have this inclusive range of ledgers in our database
		bool haveLedgerRange(std::uint32_t from, std::uint32_t to);
		bool haveLedger(std::uint32_t seq);
		std::uint32_t getValidatedSeq();
		bool isValidated(std::uint32_t seq);
		bool isValidated(std::uint32_t seq, uint256 const& hash);
		bool isValidated(Ledger::ref l)
		{
			return isValidated(l->getLedgerSeq(), l->getHash());
		}
		bool getValidatedRange(std::uint32_t& minVal, std::uint32_t& maxVal)
		{
			return m_ledgerMaster.getValidatedRange(minVal, maxVal);
		}
		bool getFullValidatedRange(std::uint32_t& minVal, std::uint32_t& maxVal)
		{
			return m_ledgerMaster.getFullValidatedRange(minVal, maxVal);
		}

		SerializedValidation::ref getLastValidation()
		{
			return mLastValidation;
		}
		void setLastValidation(SerializedValidation::ref v)
		{
			mLastValidation = v;
		}

		SLE::pointer getSLE(Ledger::pointer lpLedger, uint256 const& uHash)
		{
			return lpLedger->getSLE(uHash);
		}
		SLE::pointer getSLEi(Ledger::pointer lpLedger, uint256 const& uHash)
		{
			return lpLedger->getSLEi(uHash);
		}

		//
		// Transaction operations
		//
		typedef std::function<void(Transaction::pointer, TER)> stCallback; // must complete immediately
		void submitTransaction(Job&, SerializedTransaction::pointer);
		Transaction::pointer submitTransactionSync(Transaction::ref tpTrans, bool bAdmin, bool bLocal, bool bFailHard, bool bSubmit);

		void runTransactionQueue();
		Transaction::pointer processTransactionCb(Transaction::pointer, bool bAdmin, bool bLocal, bool bFailHard, stCallback);
		Transaction::pointer processTransaction(Transaction::pointer transaction, bool bAdmin, bool bLocal, bool bFailHard)
		{
			return processTransactionCb(transaction, bAdmin, bLocal, bFailHard, stCallback());
		}

		// VFALCO Workaround for MSVC std::function which doesn't swallow return types.
		//
		void processTransactionCbVoid(Transaction::pointer p, bool bAdmin, bool bLocal, bool bFailHard)
		{
			processTransactionCb(p, bAdmin, bLocal, bFailHard, stCallback());
		}

		Transaction::pointer findTransactionByID(uint256 const& transactionID);
#if 0
		int findTransactionsBySource(uint256 const& uLedger, std::list<Transaction::pointer>&, const RippleAddress& sourceAccount,
			std::uint32_t minSeq, std::uint32_t maxSeq);
#endif
		int findTransactionsByDestination(std::list<Transaction::pointer>&, const RippleAddress& destinationAccount,
			std::uint32_t startLedgerSeq, std::uint32_t endLedgerSeq, int maxTransactions);

		//
		// Account functions
		//

		AccountState::pointer   getAccountState(Ledger::ref lrLedger, const RippleAddress& accountID);

		//
		// Directory functions
		//

		STVector256             getDirNodeInfo(Ledger::ref lrLedger, uint256 const& uRootIndex,
			std::uint64_t& uNodePrevious, std::uint64_t& uNodeNext);

#if 0
		//
		// Nickname functions
		//

		NicknameState::pointer  getNicknameState(uint256 const& uLedger, const std::string& strNickname);
#endif

		//
		// Owner functions
		//

		Json::Value getOwnerInfo(Ledger::pointer lpLedger, const RippleAddress& naAccount);

		//
		// Book functions
		//

		void getBookPage(Ledger::pointer lpLedger,
			const uint160& uTakerPaysCurrencyID,
			const uint160& uTakerPaysIssuerID,
			const uint160& uTakerGetsCurrencyID,
			const uint160& uTakerGetsIssuerID,
			const uint160& uTakerID,
			const bool bProof,
			const bool verbose,
			const unsigned int iLimit,
			const Json::Value& jvMarker,
			Json::Value& jvResult);

		// ledger proposal/close functions
		void processTrustedProposal(LedgerProposal::pointer proposal, boost::shared_ptr<protocol::TMProposeSet> set,
			RippleAddress nodePublic, uint256 checkLedger, bool sigGood);
		SHAMapAddNode gotTXData(const boost::shared_ptr<Peer>& peer, uint256 const& hash,
			const std::list<SHAMapNodeID>& nodeIDs, const std::list< Blob >& nodeData);
		bool recvValidation(SerializedValidation::ref val, const std::string& source);
		void takePosition(int seq, SHAMap::ref position);
		SHAMap::pointer getTXMap(uint256 const& hash);
		bool hasTXSet(const boost::shared_ptr<Peer>& peer, uint256 const& set, protocol::TxSetStatus status);
		void mapComplete(uint256 const& hash, SHAMap::ref map);
		bool stillNeedTXSet(uint256 const& hash);
		void makeFetchPack(Job&, boost::weak_ptr<Peer> peer, boost::shared_ptr<protocol::TMGetObjectByHash> request,
			uint256 haveLedger, std::uint32_t uUptime);
		bool shouldFetchPack(std::uint32_t seq);
		void gotFetchPack(bool progress, std::uint32_t seq);
		void addFetchPack(uint256 const& hash, boost::shared_ptr< Blob >& data);
		bool getFetchPack(uint256 const& hash, Blob& data);
		int getFetchSize();
		void sweepFetchPack();

		// network state machine

		// VFALCO TODO Try to make all these private since they seem to be...private
		//
		void switchLastClosedLedger(Ledger::pointer newLedger, bool duringConsensus); // Used for the "jump" case
		bool checkLastClosedLedger(const Overlay::PeerSequence&, uint256& networkClosed);
		int beginConsensus(uint256 const& networkClosed, Ledger::pointer closingLedger);
		void tryStartConsensus();
		void endConsensus(bool correctLCL);
		void setStandAlone()
		{
			setMode(omFULL);
		}

		/** Called to initially start our timers.
		Not called for stand-alone mode.
		*/
		void setStateTimer();

		void newLCL(int proposers, int convergeTime, uint256 const& ledgerHash);
		void needNetworkLedger()
		{
			mNeedNetworkLedger = true;
		}
		void clearNeedNetworkLedger()
		{
			mNeedNetworkLedger = false;
		}
		bool isNeedNetworkLedger()
		{
			return mNeedNetworkLedger;
		}
		bool isFull()
		{
			return !mNeedNetworkLedger && (mMode == omFULL);
		}
		void setProposing(bool p, bool v)
		{
			mProposing = p;
			mValidating = v;
		}
		bool isProposing()
		{
			return mProposing;
		}
		bool isValidating()
		{
			return mValidating;
		}
		bool isAmendmentBlocked()
		{
			return m_amendmentBlocked;
		}
		void setAmendmentBlocked();
		void consensusViewChange();
		int getPreviousProposers()
		{
			return mLastCloseProposers;
		}
		int getPreviousConvergeTime()
		{
			return mLastCloseConvergeTime;
		}
		std::uint32_t getLastCloseTime()
		{
			return mLastCloseTime;
		}
		void setLastCloseTime(std::uint32_t t)
		{
			mLastCloseTime = t;
		}
		Json::Value getConsensusInfo();
		Json::Value getServerInfo(bool human, bool admin);
		void clearLedgerFetch();
		Json::Value getLedgerFetchInfo();
		std::uint32_t acceptLedger();
		ripple::unordered_map < uint160,
			std::list<LedgerProposal::pointer> > & peekStoredProposals()
		{
				return mStoredProposals;
			}
		void storeProposal(LedgerProposal::ref proposal, const RippleAddress& peerPublic);
		uint256 getConsensusLCL();
		void reportFeeChange();

		void updateLocalTx(Ledger::ref newValidLedger) override
		{
			m_localTX->sweep(newValidLedger);
		}
		void addLocalTx(Ledger::ref openLedger, SerializedTransaction::ref txn) override
		{
			m_localTX->push_back(openLedger->getLedgerSeq(), txn);
		}
		std::size_t getLocalTxCount() override
		{
			return m_localTX->size();
		}

		//Helper function to generate SQL query to get transactions
		std::string transactionsSQL(std::string selection, const RippleAddress& account,
			std::int32_t minLedger, std::int32_t maxLedger,
			bool descending, std::uint32_t offset, int limit,
			bool binary, bool count, bool bAdmin);


		// client information retrieval functions
		std::vector< std::pair<Transaction::pointer, TransactionMetaSet::pointer> >
			getAccountTxs(const RippleAddress& account, std::int32_t minLedger, std::int32_t maxLedger,
			bool descending, std::uint32_t offset, int limit, bool bAdmin);

		std::vector< std::pair<Transaction::pointer, TransactionMetaSet::pointer> >
			getTxsAccount(const RippleAddress& account, std::int32_t minLedger, std::int32_t maxLedger,
			bool forward, Json::Value& token, int limit, bool bAdmin);

		typedef std::tuple<std::string, std::string, std::uint32_t> txnMetaLedgerType;

		std::vector<txnMetaLedgerType>
			getAccountTxsB(const RippleAddress& account, std::int32_t minLedger,
			std::int32_t maxLedger, bool descending, std::uint32_t offset,
			int limit, bool bAdmin);

		std::vector<txnMetaLedgerType>
			getTxsAccountB(const RippleAddress& account, std::int32_t minLedger,
			std::int32_t maxLedger, bool forward, Json::Value& token, int limit, bool bAdmin);

		std::vector<RippleAddress> getLedgerAffectedAccounts(std::uint32_t ledgerSeq);

		//
		// Monitoring: publisher side
		//
		void pubLedger(Ledger::ref lpAccepted);
		void pubProposedTransaction(Ledger::ref lpCurrent, SerializedTransaction::ref stTxn, TER terResult);

		//--------------------------------------------------------------------------
		//
		// InfoSub::Source
		//
		void subAccount(InfoSub::ref ispListener,
			const boost::unordered_set<RippleAddress>& vnaAccountIDs,
			std::uint32_t uLedgerIndex, bool rt);
		void unsubAccount(std::uint64_t uListener,
			const boost::unordered_set<RippleAddress>& vnaAccountIDs,
			bool rt);

		bool subLedger(InfoSub::ref ispListener, Json::Value& jvResult);
		bool unsubLedger(std::uint64_t uListener);

		bool subServer(InfoSub::ref ispListener, Json::Value& jvResult);
		bool unsubServer(std::uint64_t uListener);

		bool subBook(InfoSub::ref ispListener, RippleCurrency const& currencyPays, RippleCurrency const& currencyGets,
			RippleIssuer const& issuerPays, RippleIssuer const& issuerGets);
		bool unsubBook(std::uint64_t uListener, RippleCurrency const& currencyPays,
			RippleCurrency const& currencyGets,
			RippleIssuer const& issuerPays, RippleIssuer const& issuerGets);

		bool subTransactions(InfoSub::ref ispListener);
		bool unsubTransactions(std::uint64_t uListener);

		bool subRTTransactions(InfoSub::ref ispListener);
		bool unsubRTTransactions(std::uint64_t uListener);

		InfoSub::pointer    findRpcSub(const std::string& strUrl);
		InfoSub::pointer    addRpcSub(const std::string& strUrl, InfoSub::ref rspEntry);

		//--------------------------------------------------------------------------
		//
		// Stoppable

		void onStop()
		{
			m_heartbeatTimer.cancel();
			m_clusterTimer.cancel();

			stopped();
		}

	private:
		void setHeartbeatTimer();
		void setClusterTimer();
		void onDeadlineTimer(beast::DeadlineTimer& timer);
		void processHeartbeatTimer();
		void processClusterTimer();

		void setMode(OperatingMode);

		Json::Value transJson(const SerializedTransaction& stTxn, TER terResult, bool bValidated, Ledger::ref lpCurrent);
		bool haveConsensusObject();

		Json::Value pubBootstrapAccountInfo(Ledger::ref lpAccepted, const RippleAddress& naAccountID);

		void pubValidatedTransaction(Ledger::ref alAccepted, const AcceptedLedgerTx& alTransaction);
		void pubAccountTransaction(Ledger::ref lpCurrent, const AcceptedLedgerTx& alTransaction, bool isAccepted);

		void pubServer();

	private:
		clock_type& m_clock;

		typedef ripple::unordered_map <uint160, SubMapType>               SubInfoMapType;
		typedef ripple::unordered_map <uint160, SubMapType>::iterator     SubInfoMapIterator;

		typedef ripple::unordered_map<std::string, InfoSub::pointer>     subRpcMapType;

		// XXX Split into more locks.
		typedef RippleRecursiveMutex LockType;
		typedef std::lock_guard <LockType> ScopedLockType;

		beast::Journal m_journal;

		std::unique_ptr <LocalTxs> m_localTX;

		std::unique_ptr <FeeVote> m_feeVote;

		LockType mLock;

		OperatingMode                       mMode;
		bool                                mNeedNetworkLedger;
		bool                                mProposing, mValidating;
		bool                                m_amendmentBlocked;
		boost::posix_time::ptime            mConnectTime;
		beast::DeadlineTimer                m_heartbeatTimer;
		beast::DeadlineTimer                m_clusterTimer;
		boost::shared_ptr<LedgerConsensus>  mConsensus;
		ripple::unordered_map < uint160,
			std::list<LedgerProposal::pointer> > mStoredProposals;

		LedgerMaster&                       m_ledgerMaster;
		InboundLedger::pointer              mAcquiringLedger;

		int                                 mCloseTimeOffset;

		// last ledger close
		int                                 mLastCloseProposers, mLastCloseConvergeTime;
		uint256                             mLastCloseHash;
		std::uint32_t                       mLastCloseTime;
		std::uint32_t                       mLastValidationTime;
		SerializedValidation::pointer       mLastValidation;

		// Recent positions taken
		std::map<uint256, std::pair<int, SHAMap::pointer> > mRecentPositions;

		SubInfoMapType                                      mSubAccount;
		SubInfoMapType                                      mSubRTAccount;

		subRpcMapType                                       mRpcSubMap;

		SubMapType                                          mSubLedger;             // accepted ledgers
		SubMapType                                          mSubServer;             // when server changes connectivity state
		SubMapType                                          mSubTransactions;       // all accepted transactions
		SubMapType                                          mSubRTTransactions;     // all proposed and accepted transactions

		TaggedCache< uint256, Blob>                         mFetchPack;
		std::uint32_t                                       mFetchSeq;

		std::uint32_t                                       mLastLoadBase;
		std::uint32_t                                       mLastLoadFactor;
	};

}
