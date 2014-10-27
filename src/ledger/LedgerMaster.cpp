#include "LedgerMaster.h"
#include "LegacyCLF.h"
#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_basics/ripple_basics.h"
#include "ripple_app/main/LoadManager.h"

using namespace ripple; // needed for logging...

namespace stellar
{
	LedgerMaster::pointer gLedgerMaster;

    LedgerMaster::LedgerMaster() : mCurrentDB(getApp().getWorkingLedgerDB())
	{
		mCaughtUp = false;
        reset();
	}

    void LedgerMaster::reset()
    {
        mCurrentCLF = LegacyCLF::pointer(new LegacyCLF(this)); // change this to BucketList when we are ready
        mLastLedgerHash = uint256();
    }

	Ledger::pointer LedgerMaster::getCurrentLedger()
	{
		return(Ledger::pointer());
	}

    bool LedgerMaster::ensureSync(ripple::Ledger::pointer lastClosedLedger, bool checkLocal)
    {
        bool res = false;
        // first, make sure we're in sync with the world
        if (lastClosedLedger->getHash() != mLastLedgerHash)
        {
            if (checkLocal)
            {
                std::vector<uint256> needed=lastClosedLedger->getNeededAccountStateHashes(1,NULL);
                if(needed.size())
                {
                    // we're missing some nodes
                    return false;
                }
            }

            try
            {
                CanonicalLedgerForm::pointer newCLF, currentCLF(new LegacyCLF(this, lastClosedLedger));
                mCurrentDB.beginTransaction();
                try
                {
                    newCLF = catchUp(currentCLF);
                }
                catch (...)
                {
                    mCurrentDB.endTransaction(true);
                }

                if (newCLF)
                {
                    commitTransaction(newCLF);
                    res = true;
                }
            }
            catch (...)
            {
                // problem applying to the database
                WriteLog(ripple::lsERROR, ripple::Ledger) << "database error";
            }
        }
        else
        { // already tracking proper ledger
            res = true;
        }

        return res;
    }

    void LedgerMaster::beginClosingLedger()
    {
        // ready to make changes
        mCurrentDB.beginTransaction();
        assert(mCurrentDB.getTransactionLevel() == 1); // should be top level transaction
    }

    void LedgerMaster::commitTransaction(CanonicalLedgerForm::pointer newLCL)
    {
        mCurrentDB.endTransaction(false);
        if (mCurrentDB.getTransactionLevel() == 0)
        {
            setLastClosedLedger(newLCL);
        }
        
    }

	bool  LedgerMaster::commitLedgerClose(ripple::Ledger::pointer ledger)
	{
        bool res = false;
        CanonicalLedgerForm::pointer newCLF;

        assert(ledger->getParentHash() == mLastLedgerHash); // should not happen

        try
        {
            CanonicalLedgerForm::pointer nl(new LegacyCLF(this, ledger));
            try
            {
                // only need to update ledger related fields as the account state is already in SQL
                updateDBFromLedger(nl);
                newCLF = nl;
            }
            catch (std::runtime_error const &)
            {
                WriteLog(ripple::lsERROR, ripple::Ledger) << "Ledger close: could not update database";
            }

            if (newCLF != nullptr)
            {
                commitTransaction(newCLF);
                res = true;
            }
            else
            {
                mCurrentDB.endTransaction(true);
            }
        }
        catch (...)
        {
        }
        return res;
	}

    void LedgerMaster::setLastClosedLedger(CanonicalLedgerForm::pointer ledger)
    {
        // should only be done outside of transactions, to guarantee state reflects what is on disk
        assert(mCurrentDB.getTransactionLevel() == 0);
        mCurrentCLF = ledger;
        mLastLedgerHash = ledger->getHash();
        WriteLog(ripple::lsINFO, ripple::Ledger) << "Store at " << mLastLedgerHash;
    }

    void LedgerMaster::abortLedgerClose()
    {
        mCurrentDB.endTransaction(true);
    }

	void LedgerMaster::loadLastKnownCLF()
	{
        bool needreset = true;
        uint256 lkcl = getLastClosedLedgerHash();
        if (lkcl.isNonZero()) {
            // there is a ledger in the database
            if (mCurrentCLF->load(lkcl)) {
                mLastLedgerHash = lkcl;
                needreset = false;
            }
        }
        if (needreset) {
            reset();
        }
	}

	CanonicalLedgerForm::pointer LedgerMaster::catchUp(CanonicalLedgerForm::pointer updatedCurrentCLF)
	{
		// new SLE , old SLE
		SHAMap::Delta delta;
        bool needFull = false;

        WriteLog(ripple::lsINFO, ripple::Ledger) << "catching up from " << mCurrentCLF->getHash() << " to " << updatedCurrentCLF->getHash();

        try
        {
            if (mCurrentCLF->getHash().isZero())
            {
                needFull = true;
            }
            else
            {
		        updatedCurrentCLF->getDeltaSince(mCurrentCLF,delta);
            }
        }
        catch (std::runtime_error const &e)
        {
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not compute delta: " << e.what();
            needFull = true;
        };

        if (needFull){
            return importLedgerState(updatedCurrentCLF->getHash());
        }

        // incremental update

        mCurrentDB.beginTransaction();

        try {
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
            updateDBFromLedger(updatedCurrentCLF);
        }
        catch (...) {
            mCurrentDB.endTransaction(true);
            throw;
        }

        commitTransaction(updatedCurrentCLF);

        WriteLog(ripple::lsINFO, ripple::Ledger) << "done catching up with " << updatedCurrentCLF->getHash();

        return updatedCurrentCLF;
	}


    static void importHelper(SLE::ref curEntry, int &counter, int &totalImports, time_t start) {
        static const int kProgressCount = 100000;
        
        totalImports++;
        
        if (++counter == kProgressCount)
        {
            int elapsed = time(nullptr) - start;
            int rate = totalImports / (elapsed?elapsed:1);
            WriteLog(ripple::lsINFO, ripple::Ledger) << "Imported " << totalImports << " items @" << rate;
            counter = 0;

            // SANITY: should instead work on batching into the DB and not mess with deadlock
            Application& app (getApp ());
            LoadManager& mgr (app.getLoadManager ());
            mgr.resetDeadlockDetector ();
        }

        LedgerEntry::pointer entry = LedgerEntry::makeEntry(curEntry);
        if(entry) {
            entry->storeAdd();
        }
        // else entry type we don't care about
        
    }
    
    CanonicalLedgerForm::pointer LedgerMaster::importLedgerState(uint256 ledgerHash)
    {
        CanonicalLedgerForm::pointer res;

        WriteLog(ripple::lsINFO, ripple::Ledger) << "Importing full ledger " << ledgerHash;

        CanonicalLedgerForm::pointer newLedger = LegacyCLF::pointer(new LegacyCLF(this));

        if (newLedger->load(ledgerHash)) {
            mCurrentDB.beginTransaction();
            try {
                // delete all
                LedgerEntry::dropAll(mCurrentDB);

                int counter = 0, totalImports = 0;
                time_t start = time(nullptr);

                // import all anew
                newLedger->getLegacyLedger()->visitStateItems(std::bind(&importHelper, P_1, std::ref(counter), std::ref(totalImports), start));

                WriteLog(ripple::lsINFO, ripple::Ledger) << "Imported " << totalImports << " items";

                updateDBFromLedger(newLedger);
            }
            catch (...) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not import state";
                mCurrentDB.endTransaction(true);
                return CanonicalLedgerForm::pointer();
            }
            commitTransaction(newLedger);
            res = newLedger;
        }
        return res;
    }

    void LedgerMaster::updateDBFromLedger(CanonicalLedgerForm::pointer ledger)
    {
        uint256 currentHash = ledger->getHash();
        string hex(to_string(currentHash));

        // saves last hash
        mCurrentDB.setState(mCurrentDB.getStoreStateName(LedgerDatabase::kLastClosedLedger),
            hex);

        ledger->save(currentHash);
    }

    uint256 LedgerMaster::getLastClosedLedgerHash()
    {
        string h = mCurrentDB.getState(mCurrentDB.getStoreStateName(LedgerDatabase::kLastClosedLedger));
        return uint256(h); // empty string -> 0
    }

    void LedgerMaster::closeLedger(TransactionSet::pointer txSet)
	{
        mCurrentDB.beginTransaction();
        assert(mCurrentDB.getTransactionLevel() == 1);
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
		    txSet->store();
        }
        catch (...)
        {
            mCurrentDB.endTransaction(true);
            throw;
        }
        mCurrentDB.endTransaction(false);
        // SANITY this code is incomplete
	}

}
