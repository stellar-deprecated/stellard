
#include "Ledger.h"
#include "ripple_app/ledger/Ledger.h"  // I know I know. It is temporary
#include "CanonicalLedgerForm.h"
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
		bool mCaughtUp;
		CanonicalLedgerForm::pointer mCurrentCLF;
		
		//LedgerHistory mHistory;


		// called when we successfully sync to the network
		void catchUpToNetwork(CanonicalLedgerForm::pointer currentCLF);

        // SQL helper commands
        int mTransactionLevel; // how many levels of transactions are currently active
        void beginTransaction();
        void endTransaction(bool rollback);

	public:
		LedgerMaster();

		// called on startup to get the last CLF we knew about
		void loadLastKnownCLF();

		// called every time we close a ledger
		void legacyLedgerClosed(ripple::Ledger::pointer ledger);

		Ledger::pointer getCurrentLedger();

		void closeLedger(TransactionSet::pointer txSet);
		CanonicalLedgerForm::pointer getCurrentCLF(){ return(mCurrentCLF); }
	};

	extern LedgerMaster gLedgerMaster;
}




