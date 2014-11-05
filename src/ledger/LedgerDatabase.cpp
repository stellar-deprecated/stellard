#include "LedgerDatabase.h"

#include "ripple/types/api/base_uint.h"
#include <boost/format.hpp>
#include "ripple_basics/log/Log.h"

#include "ripple_basics/utility/PlatformMacros.h"

#include "ripple_app/data/SqliteDatabase.h"

#include "LedgerEntry.h"

using namespace ripple;

namespace stellar
{

    vector<const char*> LedgerDatabase::getSQLInit(){ 
        vector<const char*> res;

        const char* base[] =
        {
            "PRAGMA synchronous=NORMAL;",
            "PRAGMA journal_size_limit=-1;",
            "PRAGMA journal_mode=WAL;",

            "BEGIN TRANSACTION;",

            "CREATE TABLE IF NOT EXISTS StoreState (        \
                    StateName   CHARACTER(32) PRIMARY KEY,  \
                    State       BLOB                        \
            );",
        };

        res.insert(res.end(), &base[0], &base[NUMBER(base)]);

        LedgerEntry::appendSQLInit(res);

        res.push_back("END TRANSACTION;");

        return res;
    }


    LedgerDatabase::LedgerDatabase(ripple::DatabaseCon *dbCon) : mDBCon(dbCon) {
    }

    const char *LedgerDatabase::getStoreStateName(StoreStateName n) {
        static const char *mapping[kLastEntry] = { "lastClosedLedger", "lastClosedLedgerContent" };
        if (n < 0 || n >= kLastEntry) {
            throw out_of_range("unknown entry");
        }
        return mapping[n];
    }

    string LedgerDatabase::getState(const char *stateName) {
        string res;
        string sql = str(boost::format("SELECT State FROM StoreState WHERE StateName = '%s';")
            % stateName
            );
        SqliteStatement stmt(mDBCon->getDB()->getSqliteDB(), sql);

        int iRet = stmt.step();
        if (stmt.isDone(iRet) || !stmt.isRow(iRet)) {
            res = "";
        }
        else
        {
            Blob b = stmt.getBlob(0);
            res.assign(reinterpret_cast<char*>(b.data()), b.size());
        }
        return res;
    }

    void LedgerDatabase::setState(const char *stateName, const string &value) {
        string sql = str(boost::format("INSERT OR REPLACE INTO StoreState (StateName, State) VALUES ('%s', ? );")
            % stateName
            );

        SqliteStatement stmt(mDBCon->getDB()->getSqliteDB(), sql);

        stmt.bindStatic (1, value.data(), value.size());

        int iRet = stmt.step();
        if (!stmt.isDone(iRet)) {
            WriteLog(ripple::lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
            throw std::runtime_error("could not update state in database");
        }
    }

    void LedgerDatabase::beginTransaction() {
        mDBCon->getDBLock().lock();
        try {
            mDBCon->getDB()->beginTransaction();
        }
        catch (...) {
            mDBCon->getDBLock().unlock();
            throw;
        }
    }

    void LedgerDatabase::endTransaction(bool rollback) {
        try {
            mDBCon->getDB()->endTransaction(rollback);
        }
        catch (...) {
            mDBCon->getDBLock().unlock();
            throw;
        }
        mDBCon->getDBLock().unlock();
    }

    int LedgerDatabase::getTransactionLevel() {
        return mDBCon->getDB()->getTransactionLevel();
    }
}; 

