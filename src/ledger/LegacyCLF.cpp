#include "LegacyCLF.h"

namespace stellar
{

	// SANITY how do we know what our last full ledger is?
	bool LegacyCLF::load()
	{
		
		return(true);
	}

	void LegacyCLF::getDeltaSince(CanonicalLedgerForm::pointer pastCLF, vector<SLE::pointer>& retList)
	{

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