#ifndef __LEDGERDATABASE__
#define __LEDGERDATABASE__

#include "ripple_app/ledger/Ledger.h"
#include "ripple_app/data/DatabaseCon.h"

namespace stellar
{
    class LedgerDatabase
    {
    public:

        LedgerDatabase(ripple::DatabaseCon *dbCon);

        // state store
        enum StoreStateName {
            kLastClosedLedger = 0,
            kLastEntry };

        const char *getStoreStateName(StoreStateName n);
        string getState(const char *stateName);
        void setState(const char *stateName, const char *value);

        // transaction helpers
        void beginTransaction();
        void endTransaction(bool rollback);
        int getTransactionLevel();

        ripple::DatabaseCon *getDBCon() { return mDBCon; }

    private:
        ripple::DatabaseCon *mDBCon;
    };
}

#endif
