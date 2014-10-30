#ifndef __LEGACYPREIMAGE__
#define __LEGACYPREIMAGE__
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
		LegacyCLF(LedgerMaster *ledgerMaster);
		LegacyCLF(LedgerMaster *ledgerMaster, ripple::Ledger::pointer ledger);

		bool load(uint256 ledgerHash);
        void save(uint256 ledgerHash);

		void getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList);

		void addEntry(uint256& newHash, SLE::pointer newEntry);
		void updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry);
		void deleteEntry(const uint256& hash);
		void closeLedger();  // need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets

		uint256 getHash();

		ripple::Ledger::pointer getLegacyLedger(){ return mLedger;  }
	};
}

#endif