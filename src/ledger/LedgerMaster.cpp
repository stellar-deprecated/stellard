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

	void LedgerMaster::legacyLedgerClosed(ripple::Ledger::pointer ledger)
	{
		if(!mCaughtUp)
		{
			// see if we are missing any entries from the ledger
			std::vector<uint256> needed=ledger->getNeededAccountStateHashes(1,NULL);
			if(!needed.size())
			{ // we are now caught up
				CanonicalLedgerForm::pointer currentCLF(new LegacyCLF(ledger));
				catchUpToNetowrk(currentCLF);
			}
		}
	}

	

	void LedgerMaster::loadLastKnownCLF()
	{
		mCurrentCLF->load();
	}

	void LedgerMaster::catchUpToNetowrk(CanonicalLedgerForm::pointer currentCLF)
	{
		// new SLE , old SLE
		vector< pair<SLE::pointer, SLE::pointer> > delta;
		currentCLF->getDeltaSince(mCurrentCLF,delta);
		for(int i = 0; i < delta.size(); i++)
		{
			if(delta[i].first)
			{
				LedgerEntry::pointer entry = LedgerEntry::makeEntry(delta[i].first);
				if(delta[i].second)
				{	// SLE updated
					if(entry) entry->storeChange();
				} else
				{	// SLE added
					if(entry) entry->storeAdd();
				}
			} else
			{ // SLE must have been deleted
				LedgerEntry::pointer entry = LedgerEntry::makeEntry(delta[i].second);
				if(entry) entry->storeDelete();
			}			
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
