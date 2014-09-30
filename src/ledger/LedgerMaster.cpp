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
				catchUpToNetwork(currentCLF);
                mCaughtUp = true;
			}
		}
	}

	

	void LedgerMaster::loadLastKnownCLF()
	{
        // TODO: need to use separate field for tracking hash of current
		mCurrentCLF->load();
	}

	void LedgerMaster::catchUpToNetwork(CanonicalLedgerForm::pointer updatedCurrentCLF)
	{
		// new SLE , old SLE
		SHAMap::Delta delta;
        try
        {
		    updatedCurrentCLF->getDeltaSince(mCurrentCLF,delta);
        }
        catch (...)
        {
            // TODO: fall back to recreate full (delta not efficient)
        };

        BOOST_FOREACH(SHAMap::Delta::value_type it, delta)
		{
            SLE::pointer newEntry = updatedCurrentCLF->getLegacyLedger()->getSLEi(it.first);
            SLE::pointer oldEntry = mCurrentCLF->getLegacyLedger()->getSLEi(it.first);

			if(newEntry)
			{
				LedgerEntry::pointer entry = LedgerEntry::makeEntry(newEntry);
				if(oldEntry)
				{	// SLE updated
					if(entry) entry->storeChange();
				} else
				{	// SLE added
					if(entry) entry->storeAdd();
				}
			} else
			{ // SLE must have been deleted
                assert(oldEntry);
				LedgerEntry::pointer entry = LedgerEntry::makeEntry(oldEntry);
				if(entry) entry->storeDelete();
			}			
		}
        // TODO: need to use separate field for tracking hash of
        //       current (that gets committed with the set of transactions)
        mCurrentCLF = updatedCurrentCLF;
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
        bool success = db->executeSQL(sql, true);
        
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
