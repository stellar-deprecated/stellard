#include "LedgerEntry.h"
#include "LedgerMaster.h"
#include "TrustLine.h"
#include "OfferEntry.h"

namespace stellar
{

	// SANITY
	LedgerEntry::pointer LedgerEntry::makeEntry(SLE::pointer sle)
	{
		switch(sle->getType())
		{
		case ltACCOUNT_ROOT:
			return LedgerEntry::pointer(new AccountEntry(sle));

		case ltRIPPLE_STATE:
			return LedgerEntry::pointer(new TrustLine(sle));

		case ltOFFER:
			return LedgerEntry::pointer(new OfferEntry(sle));
		}
		return(LedgerEntry::pointer());
	}

	uint256 LedgerEntry::getIndex()
	{
		if(!mIndex) calculateIndex();
		return(mIndex);
	}

	// these will do the appropriate thing in the DB and the Canonical Ledger form
	void LedgerEntry::storeDelete()
	{
		deleteFromDB();
		
		gLedgerMaster.getCurrentCLF()->deleteEntry( getHash() );
	}

	void LedgerEntry::storeChange()
	{
		updateInDB();

		gLedgerMaster.getCurrentCLF()->deleteEntry(getHash());
	}

	void LedgerEntry::storeAdd()
	{
		insertIntoDB();

		gLedgerMaster.getCurrentCLF()->deleteEntry(getHash());
	}


	// SANITY
	uint256 LedgerEntry::getHash()
	{
		return(uint256());
		//if(!mSLE) makeSLE();
		//return(mSLE->getHash());
	}
	
}
