#include <boost/format.hpp>
#include "TrustLine.h"
#include "AccountEntry.h"
#include "LedgerMaster.h"
#include "ripple_data/protocol/LedgerFormats.h"
#include "ripple_data/protocol/Serializer.h"
#include "ripple_data/protocol/TxFlags.h"
#include "ripple_basics/utility/PlatformMacros.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/main/Application.h"
#include "ripple_app/data/DatabaseCon.h"
#include "transactions/TrustSetTx.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/ledger/Ledger.h"

using namespace std;

namespace stellar {

    void TrustLine::appendSQLInit(vector<const char*> &init)
    {
        init.push_back("CREATE TABLE IF NOT EXISTS TrustLines ( \
            trustIndex CHARACTER(32),       \
            lowAccount	CHARACTER(35),      \
            highAccount CHARACTER(35),      \
            currency CHARACTER(40),         \
            lowLimit CHARACTER(39),         \
            highLimit CHARACTER(39),        \
            balance CHARACTER(39),          \
            lowAuthSet BOOL,                \
            highAuthSet BOOL,               \
            PRIMARY KEY ( trustIndex )      \
	    );");

        init.push_back("CREATE INDEX IF NOT EXISTS TrustLinesIndex1 ON TrustLines ( lowAccount );");
        init.push_back("CREATE INDEX IF NOT EXISTS TrustLinesIndex2 ON TrustLines ( highAccount );");
    }

	TrustLine::TrustLine()
	{

	}

	TrustLine::TrustLine(SLE::pointer sle)
	{
		mLowLimit = sle->getFieldAmount(sfLowLimit);
		mHighLimit = sle->getFieldAmount(sfHighLimit);

        mBalance = sle->getFieldAmount(sfBalance);

		mLowAccount = mLowLimit.getIssuer();
		mHighAccount = mHighLimit.getIssuer();
		mCurrency = mLowLimit.getCurrency();

        assert(mHighLimit.getCurrency() == mCurrency);

		uint32 flags = sle->getFlags();

		mLowAuthSet = flags & lsfLowAuth;  // if the high account has authorized the low account to hold its credit?
		mHighAuthSet = flags & lsfHighAuth;
	}

	void TrustLine::calculateIndex()
	{
		Serializer  s(62);

		s.add16(spaceRipple);          //  2
		s.add160(mLowAccount); // 20
		s.add160(mHighAccount); // 20
		s.add160(mCurrency);           // 20
		mIndex = s.getSHA512Half();
	}


	// 
	TER TrustLine::fromTx(AccountEntry& signingAccount, TrustSetTx* tx)
	{	
		mCurrency=tx->mCurrency;
				
		if(signingAccount.mAccountID > tx->mOtherAccount)
		{
			mHighAccount = signingAccount.mAccountID;
			mLowAccount = tx->mOtherAccount;
		}else
		{
			mLowAccount = signingAccount.mAccountID;
			mHighAccount = tx->mOtherAccount;
		}
		
		if(tx->mAuthSet && !signingAccount.checkFlag(lsfRequireAuth))
		{
			Log(lsTRACE) <<	"Retry: Auth not required.";
			return tefNO_AUTH_REQUIRED;
		}
		
		return(tesSUCCESS);
	}

    void TrustLine::setFromCurrentRow(Database *db)
    {
        std::string currency, amount;

        mLowAccount = db->getAccountID("lowAccount");
        mHighAccount = db->getAccountID("highAccount");


        db->getStr("currency", currency);
        STAmount::currencyFromString(mCurrency, currency);

        db->getStr("balance", amount);
        mBalance.setFullValue(amount, currency);


        db->getStr("lowLimit", amount);
        mLowLimit.setFullValue(amount, currency);

        db->getStr("highLimit", amount);
        mHighLimit.setFullValue(amount, currency);

        mLowAuthSet = db->getBool("lowAuthSet");
        mHighAuthSet = db->getBool("highAuthSet");
    }

	bool TrustLine::loadFromDB(const uint256& index)
	{
		mIndex = index;
		std::string sql = "SELECT * FROM TrustLines WHERE trustIndex='";
		sql.append(to_string(index));
		sql.append("';");

		{
			DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if (!db->executeSQL(sql, false) || !db->startIterRows())
				return false;

            setFromCurrentRow(db);
		}

		return(true);
	}

	void TrustLine::insertIntoDB()
	{
		string sql = str(boost::format("INSERT OR REPLACE INTO TrustLines (trustIndex, lowAccount,highAccount,currency,lowLimit,highLimit,balance,lowAuthSet,highAuthSet) values ('%s','%s','%s','%s','%s','%s','%s',%d,%d);")
			% to_string(getIndex())
			% mLowAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
			% mHighAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % STAmount::createHumanCurrency(mCurrency)
            % mLowLimit.getText()
			% mHighLimit.getText()
			% mBalance.getText()
			% mLowAuthSet
			% mHighAuthSet);

		{
			DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if(!db->executeSQL(sql, false))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}

	void TrustLine::updateInDB()
	{
		string sql = str(boost::format("UPDATE TrustLines set lowLimit='%s' ,highLimit='%s' ,balance='%s' ,lowAuthSet=%d ,highAuthSet=%d where trustIndex='%s';")
            % mLowLimit.getText()
            % mHighLimit.getText()
            % mBalance.getText()
			% mLowAuthSet
			% mHighAuthSet
			% to_string(getIndex()));

		{
			DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if(!db->executeSQL(sql, false))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}

	void TrustLine::deleteFromDB()
	{
		std::string sql = "DELETE FROM TrustLines where trustIndex='";
		sql.append(to_string(getIndex()));
		sql.append("';");

		{
			DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if(!db->executeSQL(sql, false))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}

    void TrustLine::dropAll(LedgerDatabase &db)
    {
        if (!db.getDBCon()->getDB()->executeSQL("DROP TABLE IF EXISTS TrustLines;"))
		{
            throw std::runtime_error("Could not drop TrustLines data");
		}
    }
}
