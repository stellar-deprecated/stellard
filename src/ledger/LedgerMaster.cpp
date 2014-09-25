#include "LedgerMaster.h"
#include "LegacyPreimage.h"


namespace stellar
{
	LedgerMaster gLedgerMaster;

	LedgerMaster::LedgerMaster()
	{
		mLedgerPreimage = LedgerPreimage::pointer(new LegacyPreimage()); // change this to BucketList when we are ready
	}

	Ledger::pointer LedgerMaster::getCurrentLedger()
	{
		return(Ledger::pointer());
	}

	void LedgerMaster::closeLedger(TransactionSet::pointer txSet)
	{
		// apply tx to the last ledger
		for(int n = 0; n < txSet->mTransactions.size(); n++)
		{
			txSet->mTransactions[n].apply();
		}

		// save collected changes to the bucket list
		mLedgerPreimage->closeLedger();

		// save set to the history
		txSet->saveHistory();
	}
}
