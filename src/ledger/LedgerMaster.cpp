#include "LedgerMaster.h"
#include "LegacyCLF.h"


namespace stellar
{
	LedgerMaster gLedgerMaster;

	LedgerMaster::LedgerMaster()
	{
		mCaughtUp = false;
		mCurrentCLF = LegacyCLF::pointer(new LegacyCLF()); // change this to BucketList when we are ready
	}

	Ledger::pointer LedgerMaster::getCurrentLedger()
	{
		return(Ledger::pointer());
	}

	void LedgerMaster::ledgerClosed()
	{
		if(!mCaughtUp)
		{
			mCaughtUp = true;
		}
	}

	void LedgerMaster::loadLastKnownCLF()
	{
		mCurrentCLF->load();
	}

	void LedgerMaster::catchUpToNetowrk(CanonicalLedgerForm::pointer currentCLF)
	{
		vector<SLE::pointer> delta;
		currentCLF->getDeltaSince(mCurrentCLF,delta);
		for(int i = 0; i < delta.size(); i++)
		{
			// SANITY we need to add a tombstone LedgerEntry for handling deletes
			LedgerEntry::pointer entry = LedgerEntry::makeEntry(delta[i]);
			if(entry) entry->storeAdd();
		}
	}

	void LedgerMaster::closeLedger(TransactionSet::pointer txSet)
	{
		// apply tx to the last ledger
		for(int n = 0; n < txSet->mTransactions.size(); n++)
		{
			txSet->mTransactions[n].apply();
		}

		// save collected changes to the bucket list
		mCurrentCLF->closeLedger();

		// save set to the history
		txSet->saveHistory();
	}
}
