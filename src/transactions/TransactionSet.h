
#include "transactions/Transaction.h"

using namespace std;

namespace stellar
{
	class TransactionSet
	{
	public:
		vector<Transaction> mTransactions;
		typedef boost::shared_ptr<TransactionSet>           pointer;

		// returns the hash of this tx set
		uint256 getHash();

		void serialize();

		void store();

	};
}