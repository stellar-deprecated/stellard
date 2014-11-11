#ifndef __STELLAR_LEDGERMASTER_H
#define __STELLAR_LEDGERMASTER_H

#include "Ledger.h"
#include "ripple_app/ledger/Ledger.h"  // I know I know. It is temporary
#include "CanonicalLedgerForm.h"
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
        uint256 mLastLedgerHash;

        //LedgerHistory mHistory;

    public:

        typedef std::shared_ptr<LedgerMaster>           pointer;

        LedgerMaster();

        // called on startup to get the last CLF we knew about
        void loadLastKnownCLF();

        // legacy interop

        // establishes that our internal representation is in sync with passed ledger
        bool ensureSync(ripple::Ledger::pointer lastClosedLedger, bool checkLocal);

        // called before starting to make changes to the db
        void beginClosingLedger();
        // called every time we successfully closed a ledger
        bool commitLedgerClose(ripple::Ledger::pointer ledger);
        // called when we could not close the ledger
        void abortLedgerClose();

        CanonicalLedgerForm::pointer getCurrentCLF(){ return(mCurrentCLF); }

        LedgerDatabase &getLedgerDatabase() { return mCurrentDB; }

    private:

        // helper methods: returns new value of CLF in database 

        // called when we successfully sync to the network
        CanonicalLedgerForm::pointer catchUp(CanonicalLedgerForm::pointer currentCLF);
        CanonicalLedgerForm::pointer importLedgerState(uint256 ledgerHash);

        void updateDBFromLedger(CanonicalLedgerForm::pointer ledger);

        void setLastClosedLedger(CanonicalLedgerForm::pointer ledger);
        uint256 getLastClosedLedgerHash();

        void reset();
    };

    extern LedgerMaster::pointer gLedgerMaster;
}

#endif
