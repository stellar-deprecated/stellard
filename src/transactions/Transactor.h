#ifndef __TRANSACTOR__
#define __TRANSACTOR__

#include "ripple_app/misc/SerializedTransaction.h"
#include "ripple_data/protocol/TER.h"

using namespace ripple;

namespace stellar {

	class Transactor
	{
	public:
		static std::unique_ptr<Transactor> makeTransactor(
			SerializedTransaction const& txn,
			TransactionEngineParams params,
			TransactionEngine* engine);

		TER apply();

	protected:
		Transaction::pointer    mTxn;
		AccountEntry	mSigningAccount;


		
		STAmount                        mFeeDue;
		STAmount                        mPriorBalance;  // Balance before fees.
		STAmount                        mSourceBalance; // Balance after fees.
		SLE::pointer                    mTxnAccount;
		bool                            mHasAuthKey;
		bool                            mSigMaster;
		RippleAddress                   mSigningPubKey;

		virtual TER preCheck();
		virtual TER checkSeq();
		virtual TER payFee();

		void calculateFee();

		// Returns the fee, not scaled for load (Should be in fee units. FIXME)
		virtual uint64_t calculateBaseFee();

		virtual TER checkSig();
		virtual TER doApply() = 0;

		Transactor(
			const SerializedTransaction& txn,
			TransactionEngine* engine,
			beast::Journal journal = beast::Journal());

		virtual bool mustHaveValidAccount()
		{
			return true;
		}
	};

}

#endif
