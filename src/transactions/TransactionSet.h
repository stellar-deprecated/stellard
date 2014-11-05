#ifndef __TRANSACTIONSET_H__
#define __TRANSACTIONSET_H__

#include "transactions/Transaction.h"

using namespace std;

namespace stellar
{
	class TransactionSet
	{
	public:
		vector<Transaction> mTransactions;
		//uint256 mPreviousLedgerHash;

		typedef boost::shared_ptr<TransactionSet>           pointer;

		// returns the hash of this tx set
		uint256 getHash();

		void serialize();

		void store();

	};
}

#endif
