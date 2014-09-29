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

	TrustLine::TrustLine()
	{

	}

	TrustLine::TrustLine(SLE::pointer sle)
	{
		STAmount lowLimit = sle->getFieldAmount(sfLowLimit);
		STAmount highLimit = sle->getFieldAmount(sfHighLimit);
		mBalance = sle->getFieldU64(sfBalance);

		mLowAccount = lowLimit.getIssuer();
		mHighAccount = highLimit.getIssuer();
		mCurrency = lowLimit.getCurrency();


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

	bool TrustLine::loadFromDB(uint256& index)
	{
		mIndex = index;
		std::string sql = "SELECT * FROM TrustLines WHERE trustIndex=x'";
		sql.append(to_string(index));
		sql.append("';");

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if (!db->executeSQL(sql, true) || !db->startIterRows())
				return false;

			mCurrency = db->getBigInt("currency");
			mBalance = db->getBigInt("balance");
			mLowAccount = db->getAccountID("lowAccount");
			mHighAccount = db->getAccountID("highAccount");

			mLowLimit = db->getBigInt("lowLimit");
			mHighLimit = db->getBigInt("highLimit");
			mLowAuthSet = db->getBool("lowAuthSet");
			mHighAuthSet = db->getBool("highAuthSet");
		}

		return(true);
	}

	void TrustLine::insertIntoDB()
	{
		//make sure it isn't already in DB
		deleteFromDB();

		string sql = str(boost::format("INSERT INTO TrustLines (trustIndex, lowAccount,highAccount,lowLimit,highLimit,currency,balance,lowAuthSet,highAuthSet) values (x'%s','%s','%s',%d,%d,%d,%d,%d,%d);")
			% to_string(getIndex())
			% mLowAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
			% mHighAccount.base58Encode(RippleAddress::VER_ACCOUNT_ID)
			% mLowLimit
			% mHighLimit
			% mCurrency
			% mBalance
			% mLowAuthSet
			% mHighAuthSet);

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}

	void TrustLine::updateInDB()
	{
		string sql = str(boost::format("UPDATE TrustLines set lowLimit=%d ,highLimit=%d ,balance=%d ,lowAuthSet=%d ,highAuthSet=%d where trustIndex=x'%s';")
			% mLowLimit
			% mHighLimit
			% mBalance
			% mLowAuthSet
			% mHighAuthSet
			% to_string(getIndex()));

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
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
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}
}
