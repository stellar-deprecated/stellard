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

    CanonicalLedgerForm::pointer LegacyCLF::load(LedgerMaster *ledgerMaster, uint256 ledgerHash)
    {
        ripple::Ledger::pointer baseLedger;
        CanonicalLedgerForm::pointer res;

        // retrieve blob from SQL DB
        string s = ledgerMaster->getLedgerDatabase().getState(LedgerDatabase::kLastClosedLedgerContent);

        if (!s.empty())
        {
            ripple::Ledger::pointer ledger = boost::make_shared<ripple::Ledger>(s, true);
            if (ledger && ledger->getHash() == ledgerHash) {
                if (ledger->loadMaps(true)) {
                    ledger->setClosed();
                    ledger->setImmutable();

                    if (getApp().getOPs().haveLedger(ledger->getLedgerSeq())) {
                        ledger->setAccepted();
                        ledger->setValidated();
                    }

                    ledger->setFull();
                }
            }
        }

        if (!baseLedger) {
            baseLedger = getApp().getLedgerMaster().getLedgerByHash(ledgerHash);
        }

        if (!baseLedger) {
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " not found in local db";
        }
        else {
            if (!baseLedger->isClosed() || !baseLedger->assertSane() || baseLedger->getHash() != ledgerHash) {
                WriteLog(ripple::lsWARNING, ripple::Ledger) << "ledger " << ledgerHash << " found but not closed/sane in local db";
                baseLedger = ripple::Ledger::pointer();
            }
        }

        if (baseLedger) {
            res.reset(new LegacyCLF(ledgerMaster, baseLedger));
        }

        return res;
    }

    void LegacyCLF::save()
    {
        // saves last ledger info
        {
            Serializer s(128);
            s.add32(HashPrefix::ledgerMaster);
            mLedger->addRaw(s);
            mLedgerMaster->getLedgerDatabase().setState(LedgerDatabase::kLastClosedLedgerContent,
                s.getString());
        }
    }

    bool LegacyCLF::getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList)
    {
        bool res = false;

        assert(mLedger && mLedger->isClosed());

        // too many diffs -> better off to do a full sync
        const int kMaxDiffs = 250000;

        SHAMap::pointer newState = mLedger->peekAccountStateMap();
        ripple::Ledger::pointer pastLedger = pastCLF->getLegacyLedger();
        if (pastLedger)
        {
            SHAMap::pointer oldState = pastLedger->peekAccountStateMap();

            //vector< pair<SLE::pointer, SLE::pointer> > differences;
            res = newState->compare(oldState, retList, kMaxDiffs);
        }
        return res;
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
