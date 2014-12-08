#ifndef __LEGACYCLF__
#define __LEGACYCLF__
#include "CanonicalLedgerForm.h"
#include "ripple_app/ledger/Ledger.h"
#include "LedgerMaster.h"
/*
SHAMap way of calculating the ledger hash
*/
using namespace ripple;

namespace stellar
{
    class LegacyCLF : public CanonicalLedgerForm
    {
        ripple::Ledger::pointer mLedger;
        LedgerMaster *mLedgerMaster;

    public:
        static CanonicalLedgerForm::pointer load(LedgerMaster *ledgerMaster, uint256 ledgerHash);
        static ripple::Ledger::pointer loadLegacyLedger(LedgerMaster *ledgerMaster, bool lastClosed);
        static void saveLedgerForImport(LedgerMaster *ledgerMaster, CanonicalLedgerForm::pointer ledgerToImport);

        LegacyCLF(LedgerMaster *ledgerMaster);
        LegacyCLF(LedgerMaster *ledgerMaster, ripple::Ledger::pointer ledger);

        void save();

        bool getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList);

        void closeLedger();  // need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets

        uint256 getHash();

        ripple::Ledger::pointer getLegacyLedger(){ return mLedger; }
    };
}

#endif
