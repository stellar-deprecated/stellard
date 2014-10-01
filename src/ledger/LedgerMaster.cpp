#include "LedgerMaster.h"
#include "LegacyCLF.h"
#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_basics/ripple_basics.h"

using namespace ripple; // needed for logging...

namespace stellar
{
	LedgerMaster gLedgerMaster;

    LedgerMaster::LedgerMaster() : mCurrentDB(getApp().getWorkingLedgerDB())
	{
		mCaughtUp = false;
        reset();
        
	}

    void LedgerMaster::reset()
    {
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
			// see if we are missing any entries in the local ledger
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
        uint256 lkcl = getLastClosedLedgerHash();
        if (lkcl.isNonZero()) {
            // there is a ledger in the database
		    mCurrentCLF->load(lkcl);
        }
        else {
            reset();
        }
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
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not compute delta: defaulting to full import";

            importLedgerState(updatedCurrentCLF->getHash());
            return;
        };

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
            // TODO: need to use separate field for tracking hash of
            //       current (that gets committed with the set of transactions)


            updateDBFromLedger(updatedCurrentCLF);
        }
        catch (...) {
            mCurrentDB.endTransaction(true);
            throw;
        }

        mCurrentDB.endTransaction(false);

        // all committed to the db

        mCurrentCLF = updatedCurrentCLF;
	}


    static void importHelper(SLE::ref curEntry, LedgerMaster &lm) {
        LedgerEntry::pointer entry = LedgerEntry::makeEntry(curEntry);
        if(entry) {
            entry->storeAdd();
        }
        else {
            throw std::runtime_error("could not add entry");
        }
    }
    
    void LedgerMaster::importLedgerState(uint256 ledgerHash)
    {
        CanonicalLedgerForm::pointer newLedger = LegacyCLF::pointer(new LegacyCLF());

        if (newLedger->load(ledgerHash)) {
            mCurrentDB.beginTransaction();
            try {
                // delete all
                LedgerEntry::dropAll(mCurrentDB);

                // import all anew
                newLedger->getLegacyLedger()->visitStateItems(BIND_TYPE (&importHelper, P_1, boost::ref (*this)));

                updateDBFromLedger(newLedger);
            }
            catch (...) {
                mCurrentDB.endTransaction(false);
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "Could not import state";
                return;
            }
            mCurrentDB.endTransaction(true);
            mCurrentCLF = newLedger;
        }
    }

    void LedgerMaster::updateDBFromLedger(CanonicalLedgerForm::pointer ledger)
    {
        uint256 currentHash = ledger->getHash();
        string hex(to_string(currentHash));

        mCurrentDB.setState(mCurrentDB.getStoreStateName(LedgerDatabase::kLastClosedLedger), hex.c_str());
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
		    txSet->saveHistory();
        }
        catch (...)
        {
            mCurrentDB.endTransaction(true);
            throw;
        }
        mCurrentDB.endTransaction(false);
	}

}
