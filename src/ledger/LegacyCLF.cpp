#include "LegacyCLF.h"

#include "ripple_app/main/Application.h"


#include "ripple_basics/log/Log.h"

#include "ripple_app/ledger/LedgerMaster.h"

using namespace ripple; // needed for logging...


namespace stellar
{
	LegacyCLF::LegacyCLF()
	{

	}

	LegacyCLF::LegacyCLF(ripple::Ledger::pointer ledger)
	{
		mLedger = ledger;
	}

	bool LegacyCLF::load(uint256 ledgerHash)
	{
        // SANITY - should load from sql when we're ready
        mLedger = getApp().getLedgerMaster ().getLedgerByHash (ledgerHash);
        if (!mLedger) {
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " not found in local db";
        }
        else {
            if (!mLedger->isClosed()) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " found but not closed in local db";
                mLedger = Ledger::pointer();
            }
        }
		return(mLedger != nullptr);
	}

	void LegacyCLF::getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList)
	{
        assert(mLedger && mLedger->isClosed());

        const int kMaxDiffs = 10000000;

		SHAMap::pointer newState=mLedger->peekAccountStateMap();
        ripple::Ledger::pointer pastLedger = pastCLF->getLegacyLedger();
		if(pastLedger)
		{
			SHAMap::pointer oldState = pastLedger->peekAccountStateMap();

			//vector< pair<SLE::pointer, SLE::pointer> > differences;
			if(!newState->compare(oldState, retList, kMaxDiffs))
			{
				throw std::runtime_error("too many differences");
			}
		}
	}

	void LegacyCLF::addEntry(uint256& newHash, SLE::pointer newEntry)
	{

	}
	void LegacyCLF::updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry)
	{

	}
	void LegacyCLF::deleteEntry(uint256& hash)
	{

	}
	// need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets
	void LegacyCLF::closeLedger()
	{

	}

	uint256 LegacyCLF::getHash()
	{
		return(mHash);
	}
}