#include "LedgerEntry.h"
#include "LedgerMaster.h"
#include "TrustLine.h"
#include "OfferEntry.h"
#include "AccountEntry.h"

namespace stellar
{
    LedgerEntry::pointer LedgerEntry::makeEntry(SLE::pointer sle)
    {
        switch (sle->getType())
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
        if (!mIndex) calculateIndex();
        return(mIndex);
    }

    // these will do the appropriate thing in the DB and the Canonical Ledger form
    void LedgerEntry::storeDelete()
    {
        deleteFromDB();
    }

    void LedgerEntry::storeChange()
    {
        updateInDB();
    }

    void LedgerEntry::storeAdd()
    {
        insertIntoDB();
    }


    // SANITY use a registration pattern instead as we also need factories
    void LedgerEntry::dropAll(LedgerDatabase &db)
    {
        // SANITY implement this for the actual ledger entry ~~ the ledger class seems to conflict with this
        AccountEntry::dropAll(db);
        TrustLine::dropAll(db);
        OfferEntry::dropAll(db);

        vector<const char *> createAll;
        appendSQLInit(createAll);
        for(const char * const &sql: createAll) {
            if (!db.getDBCon()->getDB()->executeSQL(sql)) {
                throw std::runtime_error("could not re-create table ");
            }
        }
    }

    void LedgerEntry::appendSQLInit(vector<const char*> &init)
    {
        AccountEntry::appendSQLInit(init);
        OfferEntry::appendSQLInit(init);
        TrustLine::appendSQLInit(init);
    }


}
