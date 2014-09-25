
#include "Ledger.h"
#include "LedgerPreimage.h"

#include "transactions/TransactionSet.h"

/*
Holds the current ledger
Applies the tx set to the last ledger to get the next one
Hands the old ledger off to the history
*/

namespace stellar
{
	class LedgerMaster
	{
		LedgerPreimage::pointer mLedgerPreimage;
		
		//LedgerHistory mHistory;

	public:
		LedgerMaster();

		Ledger::pointer getCurrentLedger();

		void closeLedger(TransactionSet::pointer txSet);
		LedgerPreimage::pointer getPreimage(){ return(mLedgerPreimage); }
	};

	extern LedgerMaster gLedgerMaster;
}




