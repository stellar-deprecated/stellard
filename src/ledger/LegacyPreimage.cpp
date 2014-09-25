#include "LegacyPreimage.h"

namespace stellar
{

	void LegacyPreimage::addEntry(uint256& newHash, SLE::pointer newEntry)
	{

	}
	void LegacyPreimage::updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry)
	{

	}
	void LegacyPreimage::deleteEntry(uint256& hash)
	{

	}
	// need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets
	void LegacyPreimage::closeLedger()
	{

	}

	uint256 LegacyPreimage::getHash()
	{
		return(mHash);
	}
}