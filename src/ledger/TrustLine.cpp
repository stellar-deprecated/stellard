#include "TrustLine.h"
#include "AccountEntry.h"
#include "ripple_data/protocol/LedgerFormats.h"
#include "ripple_data/protocol/Serializer.h"
#include "ripple_data/protocol/TxFlags.h"
#include "ripple_basics/utility/PlatformMacros.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/main/Application.h"
#include "ripple_app/data/DatabaseCon.h"
#include "transactions/TrustSetTx.h"

using namespace std;

namespace stellar {

	TrustLine::TrustLine()
	{

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
			Log(lsTRACE) <<
				"Retry: Auth not required.";
			return tefNO_AUTH_REQUIRED;
		}
		
		return(tesSUCCESS);
	}

	bool TrustLine::loadFromDB(uint256& index)
	{
		std::string sql = "SELECT * FROM TrustLines WHERE trustID='";
		sql.append(to_string(index));
		sql.append("';");

		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

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
		std::string sql = "INSERT INTO TrustLines (lowAccount,highAccount,lowLimit,highLimit,currency,balance,lowAuthSet,highAuthSet) values ('";
		// SANITY
		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// Report error?
			}
		}
	}

	void TrustLine::updateInDB()
	{
		std::string sql = "UPDATE TrustLines set lowLimit= ,highLimit= ,balance= ,lowAuthSet= ,highAuthSet= where trustIndex='";
		// SANITY
		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// Report error?
			}
		}
	}

	void TrustLine::deleteFromDB()
	{
		std::string sql = "DELETE FROM TrustLines where trustIndex='";
		sql.append(to_string(getIndex()));
		sql.append("';");

		// SANITY
		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// Report error?
			}
		}
	}
}
