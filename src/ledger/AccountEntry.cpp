#include <boost/format.hpp>
#include "AccountEntry.h"
#include "LedgerMaster.h"
#include "ripple_app/data/DatabaseCon.h"
#include "ripple_app/main/Application.h"

namespace stellar
{
	
	void AccountEntry::insertIntoDB()
	{
		string sql = str(boost::format("INSERT INTO Accounts (accountID,balance,sequence,owenerCount,transferRate,inflationDest,publicKey,requireDest,requireAuth) values ('%s',%d,%d,%d,%d,'%s','%s',%d,%d);")
			% mAccountID.base58Encode()
			% mBalance
			% mSequence
			% mOwnerCount
			% mTransferRate
			% mInflationDest
			% mPubKey.base58Key()
			% mRequireDest
			% mRequireAuth);

		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// SANITY
			}
		}
	}
	void AccountEntry::updateInDB()
	{
		string sql = str(boost::format("UPDATE Accounts set balance=%d, sequence=%d,owenerCount=%d,transferRate=%d,inflationDest='%s',publicKey='%s',requireDest=%d,requireAuth=%d where accountID='%s';")
			% mBalance
			% mSequence
			% mOwnerCount
			% mTransferRate
			% mInflationDest
			% mPubKey.base58Key()
			% mRequireDest
			% mRequireAuth
			% mAccountID.base58Encode());

		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// SANITY
			}
		}
	}
	void AccountEntry::deleteFromDB()
	{
		string sql = str(boost::format("DELETE from Accounts where accountID='%s';")
			% mAccountID.base58Encode());

		{
			DeprecatedScopedLock sl(getApp().getTxnDB()->getDBLock());
			Database* db = getApp().getTxnDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				// SANITY
			}
		}
	}

	bool AccountEntry::checkFlag(LedgerSpecificFlags flag)
	{
		// SANITY: 
		return(true);
	}

	// will return tesSUCCESS or that this account doesn't have the reserve to do this
	TER AccountEntry::tryToIncreaseOwnerCount()
	{
		// The reserve required to create the line.
		uint64 reserveNeeded = gLedgerMaster.getCurrentLedger()->getReserve(mOwnerCount + 1);

		if(mBalance >= reserveNeeded)
		{
			mOwnerCount++;
			return tesSUCCESS;
		}
		return tecINSUF_RESERVE_LINE;
	}
}

