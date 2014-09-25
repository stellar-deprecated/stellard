#include "LedgerEntry.h"
#include "LedgerMaster.h"

namespace stellar
{

	// SANITY
	LedgerEntry::pointer LedgerEntry::makeEntry(SLE::pointer sle)
	{

		return(LedgerEntry::pointer());
	}

	uint256 LedgerEntry::getIndex()
	{
		if(!mIndex) calculateIndex();
		return(mIndex);
	}

	// these will do the appropriate thing in the DB and the preimage
	void LedgerEntry::storeDelete()
	{
		deleteFromDB();
		
		gLedgerMaster.getPreimage()->deleteEntry( getHash() );
	}

	void LedgerEntry::storeChange()
	{
		updateInDB();

		gLedgerMaster.getPreimage()->deleteEntry(getHash());
	}

	void LedgerEntry::storeAdd()
	{
		insertIntoDB();

		gLedgerMaster.getPreimage()->deleteEntry(getHash());
	}


	// SANITY
	uint256 LedgerEntry::getHash()
	{
		return(uint256());
		//if(!mSLE) makeSLE();
		//return(mSLE->getHash());
	}
	
}
