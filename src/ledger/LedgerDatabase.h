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
            kLastEntry
        };

        string getState(StoreStateName stateName);
        void setState(StoreStateName stateName, const string &value);

        // transaction helpers
        void beginTransaction();
        void endTransaction(bool commit);
        int getTransactionLevel();

        class ScopedTransaction {
            LedgerDatabase *mDB;
        public:
            ScopedTransaction(LedgerDatabase &db) : mDB(nullptr) { beginTransaction(db); }
            ~ScopedTransaction() { endTransaction(false); }

            void beginTransaction(LedgerDatabase &db) {
                if (mDB != nullptr)
                    throw std::runtime_error("unexpected transaction state");
                mDB = &db;
                db.beginTransaction(); 
            }

            void endTransaction(bool commit) {
                if (mDB) {
                    LedgerDatabase * db = mDB;
                    mDB = nullptr;
                    db->endTransaction(commit);
                }
            }

            bool hasActiveTransaction() { return mDB != nullptr; }
        };

        ripple::DatabaseCon *getDBCon() { return mDBCon; }

        static vector<const char*> getSQLInit();
    private:
        ripple::DatabaseCon *mDBCon;

        const char *getStoreStateName(StoreStateName n);
    };
}

#endif
