#ifndef __TRANSACTION__
#define __TRANSACTION__
#include <boost/shared_ptr.hpp>
#include "ledger/AccountEntry.h"
#include "ripple_data/protocol/TER.h"

/*
A transaction in its exploded form.
We can get it in from the DB or from the wire
*/
namespace stellar
{

	class Transaction
	{
	protected:
		uint16 mType;
		uint160 mAccountID;
		uint32 mSequence;
		uint32 mFee;
		uint32 mMinLedger;  // window where this tx is valid
		uint32 mMaxLedger;
		// mSignature;

		AccountEntry mSigningAccount;	

		virtual TER doApply() = 0;
	public:
		typedef boost::shared_ptr<Transaction> pointer;

		static Transaction::pointer makeTransactionFromDB();
		static Transaction::pointer makeTransactionFromWire();

		// apply this transaction to the current ledger
		// SANITY: how will applying historical txs work?
		TER apply();

		void serialize(uint256& hash, Blob& blobRet);
	};

}
#endif