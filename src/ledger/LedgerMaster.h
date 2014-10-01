
#include "Ledger.h"
#include "ripple_app/ledger/Ledger.h"  // I know I know. It is temporary
#include "CanonicalLedgerForm.h"
#include "transactions/TransactionSet.h"
#include "ledger/LedgerDatabase.h"

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
        LedgerDatabase mCurrentDB;
		
		//LedgerHistory mHistory;


		// called when we successfully sync to the network
		void catchUpToNetwork(CanonicalLedgerForm::pointer currentCLF);

	public:

        typedef boost::shared_ptr<LedgerMaster>           pointer;
        typedef const boost::shared_ptr<LedgerMaster>&    ref;

		LedgerMaster();

		// called on startup to get the last CLF we knew about
		void loadLastKnownCLF();

		// called every time we close a ledger
		void legacyLedgerClosed(ripple::Ledger::pointer ledger);

		Ledger::pointer getCurrentLedger();

		void closeLedger(TransactionSet::pointer txSet);
		CanonicalLedgerForm::pointer getCurrentCLF(){ return(mCurrentCLF); }

        void importLedgerState(uint256 ledgerHash);

    private:
        void updateDBFromLedger(CanonicalLedgerForm::pointer ledger);
        uint256 getLastClosedLedgerHash();
        void reset();
	};

	extern LedgerMaster::pointer gLedgerMaster;
}




