#include "LegacyCLF.h"


namespace stellar
{
	LegacyCLF::LegacyCLF()
	{

	}

	LegacyCLF::LegacyCLF(ripple::Ledger::pointer ledger)
	{
		mLedger = ledger;
	}

	// SANITY how do we know what our last full ledger is?
	bool LegacyCLF::load(uint256 ledgerHash)
	{
		
		return(true);
	}

	void LegacyCLF::getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList)
	{
        const int kMaxDiffs = 10000000;

		SHAMap::pointer newState=mLedger->peekAccountStateMap();
        ripple::Ledger::pointer pastLedger = pastCLF->getLegacyLedger();
		SHAMap::pointer oldState = pastLedger->peekAccountStateMap();

		//vector< pair<SLE::pointer, SLE::pointer> > differences;
        if (!newState->compare(oldState, retList, kMaxDiffs))
        {
            throw std::runtime_error("too many differences");
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