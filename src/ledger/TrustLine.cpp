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

    // SANITY -- code below does not work
    // need to rethink the way we store those lines:
    //     balance, low and high limit share the same currency
    //     should save/load the currency properly
    //     currency's issuer should be also first classed if we're going this direction


    const char *TrustLine::kSQLCreateStatement = "CREATE TABLE IF NOT EXISTS TrustLines (					\
		trustIndex Blob(32),					\
		lowAccount	CHARACTER(35),				\
		highAccount CHARACTER(35),				\
		currency Blob(20),						\
		lowLimit CHARACTER(100),				\
		highLimit CHARACTER(100),				\
		balance CHARACTER(100),					\
		lowAuthSet BOOL,						\
		highAuthSet BOOL						\
	); ";

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

	bool TrustLine::loadFromDB(const uint256& index)
	{
		mIndex = index;
		std::string sql = "SELECT * FROM TrustLines WHERE trustIndex=x'";
		sql.append(to_string(index));
		sql.append("';");

		{
			DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if (!db->executeSQL(sql, false) || !db->startIterRows())
				return false;

			mCurrency = db->getBigInt("currency");
			//mBalance = db->getBigInt("balance");
			mLowAccount = db->getAccountID("lowAccount");
			mHighAccount = db->getAccountID("highAccount");

			//mLowLimit = db->getBigInt("lowLimit");
			//mHighLimit = db->getBigInt("highLimit");
			mLowAuthSet = db->getBool("lowAuthSet");
			mHighAuthSet = db->getBool("highAuthSet");
		}

		return(true);
	}

	void TrustLine::insertIntoDB()
	{
		string sql = str(boost::format("INSERT OR REPLACE INTO TrustLines (trustIndex, lowAccount,highAccount,lowLimit,highLimit,currency,balance,lowAuthSet,highAuthSet) values (x'%s','%s','%s','%s','%s','%s','%s',%d,%d);")
			% to_string(getIndex())
			% mLowAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
			% mHighAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mLowLimit.getText()
			% mHighLimit.getText()
            % mCurrency.base58Encode(RippleAddress::VER_ACCOUNT_ID)
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
		string sql = str(boost::format("UPDATE TrustLines set lowLimit='%s' ,highLimit='%s' ,balance='%s' ,lowAuthSet=%d ,highAuthSet=%d where trustIndex=x'%s';")
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
		std::string sql = "DELETE FROM TrustLines where trustIndex=x'";
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
        if (!db.getDBCon()->getDB()->executeSQL(kSQLCreateStatement))
        {
            throw std::runtime_error("Could not recreate Account data");
		}
    }
}
