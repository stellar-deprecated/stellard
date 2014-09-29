#include "LedgerMaster.h"
#include "LegacyCLF.h"
#include "ripple_app/main/Application.h"
#include "ripple_app/data/DatabaseCon.h"

namespace stellar
{
	LedgerMaster gLedgerMaster;

	LedgerMaster::LedgerMaster()
	{
		mCaughtUp = false;
		mCurrentCLF = LegacyCLF::pointer(new LegacyCLF()); // change this to BucketList when we are ready
        mTransactionLevel = 0;
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
                mCaughtUp = true;
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
        assert(mTransactionLevel == 0);
        beginTransaction();
        try {
		// apply tx set to the last ledger
            // todo: needs the logic to deal with partial failure
		    for(int n = 0; n < txSet->mTransactions.size(); n++)
		    {
			    txSet->mTransactions[n].apply();
		    }

		    // save collected changes to the bucket list
		    mCurrentCLF->closeLedger();

		    // save set to the history
		    txSet->saveHistory();
        }
        catch (...)
        {
            endTransaction(true);
            throw;
        }
        endTransaction(false);
	}

    void LedgerMaster::beginTransaction()
    {
        const char *sql = nullptr;
        bool tooklock = false;
        if (mTransactionLevel++ == 0) {
            sql = "BEGIN;";
            getApp().getWorkingLedgerDB()->getDBLock().lock();
            tooklock = true;
        }
        else {
            assert(mTransactionLevel <= 2); // no need for more levels for now
            sql = "SAVEPOINT L1;";
        }

        Database* db = getApp().getWorkingLedgerDB()->getDB();
        if(!db->executeSQL(sql, true))
        {
            if (tooklock) {
                getApp().getWorkingLedgerDB()->getDBLock().unlock();
            }
            throw std::runtime_error("Could not perform transaction");
        }
    }

    void LedgerMaster::endTransaction(bool rollback)
    {
        const char *sql = nullptr;
        bool needunlock = false;
        assert(mTransactionLevel > 0);
        if (--mTransactionLevel == 0) {
            sql = rollback ? "ROLLBACK;" : "COMMIT;";
            needunlock = true;
        }
        else {
            sql = rollback ? "ROLLBACK TO SAVEPOINT L1;" : "RELEASE SAVEPOINT L1";
        }

        Database* db = getApp().getWorkingLedgerDB()->getDB();
        bool success = db->executeSQL(sql, true));
        
        if (needunlock)
        {
            getApp().getWorkingLedgerDB()->getDBLock().unlock();
        }

        if (!success)
        {
            throw std::runtime_error("Could not commit transaction");
        }
    }
}
