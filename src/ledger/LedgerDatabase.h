#ifndef __LEDGERDATABASE__
#define __LEDGERDATABASE__

#include <string>
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
            kLastClosedLedgerContent,
            kLastEntry };

        const char *getStoreStateName(StoreStateName n);
        string getState(const char *stateName);
        void setState(const char *stateName, const string &value);

        // transaction helpers
        void beginTransaction();
        void endTransaction(bool rollback);
        int getTransactionLevel();

        ripple::DatabaseCon *getDBCon() { return mDBCon; }

        static vector<const char*> getSQLInit();
    private:
        ripple::DatabaseCon *mDBCon;
    };
}

#endif
