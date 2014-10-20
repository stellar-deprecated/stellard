#include "LegacyCLF.h"

#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/ledger/LedgerMaster.h"
#include "ripple_core/nodestore/api/Database.h"
#include "ripple_data/protocol/HashPrefix.h"
#include "ripple_app/misc/NetworkOPs.h"

using namespace ripple; // needed for logging...


namespace stellar
{
    LegacyCLF::LegacyCLF(LedgerMaster *ledgerMaster) : mLedgerMaster(ledgerMaster)
	{
	}

	LegacyCLF::LegacyCLF(LedgerMaster *ledgerMaster, ripple::Ledger::pointer ledger) :
        mLedger(ledger),
        mLedgerMaster(ledgerMaster)
    {
	}

	bool LegacyCLF::load(uint256 ledgerHash)
	{
        // retrieve blob from SQL DB
        string s = mLedgerMaster->getLedgerDatabase().getState(
            mLedgerMaster->getLedgerDatabase().getStoreStateName(LedgerDatabase::kLastClosedLedgerContent));

        if (!s.empty())
        {
            ripple::Ledger::pointer ledger = boost::make_shared<ripple::Ledger> (s, true);
            if (ledger && ledger->getHash() == ledgerHash) {
                if (ledger->loadMaps()) {
                    mLedger = ledger;

                    mLedger->setClosed ();
                    mLedger->setImmutable ();

                    if (getApp().getOPs().haveLedger(mLedger->getLedgerSeq())) {
                        mLedger->setAccepted ();
                        mLedger->setValidated();
                    }

                    mLedger->setFull();
                }
            }
        }

        if (!mLedger) {
            mLedger = getApp().getLedgerMaster().getLedgerByHash(ledgerHash);
        }

        if (!mLedger) {
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " not found in local db";
        }
        else {
            if (!mLedger->isClosed() || !mLedger->assertSane() || mLedger->getHash() != ledgerHash) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " found but not closed/sane in local db";
                mLedger = ripple::Ledger::pointer();
            }
        }
		return(mLedger != nullptr);
	}

    void LegacyCLF::save(uint256 ledgerHash)
    {
        // saves last ledger info
        {
            Serializer s(128);
            s.add32 (HashPrefix::ledgerMaster);
            mLedger->addRaw(s);
            mLedgerMaster->getLedgerDatabase().setState(
                mLedgerMaster->getLedgerDatabase().getStoreStateName(LedgerDatabase::kLastClosedLedgerContent),
                s.getString());
        }
    }

	void LegacyCLF::getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList)
	{
        assert(mLedger && mLedger->isClosed());

        // too many diffs -> better off to do a full sync
        const int kMaxDiffs = 250000;

		SHAMap::pointer newState=mLedger->peekAccountStateMap();
        ripple::Ledger::pointer pastLedger = pastCLF->getLegacyLedger();
		if(pastLedger)
		{
			SHAMap::pointer oldState = pastLedger->peekAccountStateMap();

			//vector< pair<SLE::pointer, SLE::pointer> > differences;
			if(!newState->compare(oldState, retList, kMaxDiffs))
			{
				throw std::runtime_error("too many differences");
			}
		}
	}

	void LegacyCLF::addEntry(uint256& newHash, SLE::pointer newEntry)
	{

	}
	void LegacyCLF::updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry)
	{

	}
	void LegacyCLF::deleteEntry(const uint256& hash)
	{

	}
	// need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets
	void LegacyCLF::closeLedger()
	{

	}

	uint256 LegacyCLF::getHash()
	{
		return (mLedger == nullptr) ? uint256() : mLedger->getHash();
	}
}
