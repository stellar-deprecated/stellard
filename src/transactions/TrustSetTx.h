#include "Transaction.h"

namespace stellar
{
	class TrustSetTx : public Transaction
	{
		


		TER doApply();
	public:
		uint160 mCurrency;
		uint160 mOtherAccount;
		uint64 mYourlimt;
		bool mAuthSet;




		
	};
}