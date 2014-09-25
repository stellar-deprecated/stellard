#include "BucketList.h"

namespace stellar
{


	void BucketList::calculateHash()
	{

	}
	/*
	void BucketList::setParentBucketList(BucketList::pointer parent)
	{

	}*/
	void BucketList::addEntry(uint256& newHash, SLE::pointer newEntry)
	{

	}
	void BucketList::updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry)
	{

	}
	void BucketList::deleteEntry(uint256& hash)
	{

	}

	// need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets
	void BucketList::closeLedger()
	{

	}

	uint256 BucketList::getHash()
	{
		return(mHash);
	}

}