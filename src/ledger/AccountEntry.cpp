#include <boost/format.hpp>
#include "AccountEntry.h"
#include "LedgerMaster.h"
#include "ripple_app/data/DatabaseCon.h"
#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/ledger/Ledger.h"

namespace stellar
{
	
	AccountEntry::AccountEntry(SLE::pointer sle)
	{
		mAccountID=sle->getFieldAccount160(sfAccount);
		mBalance = sle->getFieldAmount(sfBalance).getNValue();
		mSequence=sle->getFieldU32(sfSequence);
		mOwnerCount=sle->getFieldU32(sfOwnerCount);
		if(sle->isFieldPresent(sfTransferRate))
			mTransferRate=sle->getFieldU32(sfTransferRate);
		else mTransferRate = 0;

		if(sle->isFieldPresent(sfInflationDest))
			mInflationDest=sle->getFieldAccount160(sfInflationDest);
		
		uint32 flags = sle->getFlags();

		mRequireDest = flags & lsfRequireDestTag;
		mRequireAuth = flags & lsfRequireAuth;

		// if(sle->isFieldPresent(sfPublicKey)) SANITY
		//	mPubKey=
	}

	void AccountEntry::calculateIndex()
	{
		Serializer  s(22);

		s.add16(spaceAccount); //  2
		s.add160(mAccountID);  // 20

		mIndex= s.getSHA512Half();
	}

	void  AccountEntry::insertIntoDB()
	{
		//make sure it isn't already in DB
		deleteFromDB();

		string sql = str(boost::format("INSERT INTO Accounts (accountID,balance,sequence,owenerCount,transferRate,inflationDest,publicKey,requireDest,requireAuth) values ('%s',%d,%d,%d,%d,'%s','%s',%d,%d);")
			% mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID)
			% mBalance
			% mSequence
			% mOwnerCount
			% mTransferRate
			% mInflationDest
			% mPubKey.base58Key()
			% mRequireDest
			% mRequireAuth);

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
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
			% mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID));

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
			}
		}
	}
	void AccountEntry::deleteFromDB()
	{
		string sql = str(boost::format("DELETE from Accounts where accountID='%s';")
			% mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID));

		{
			DeprecatedScopedLock sl(getApp().getLedgerDB()->getDBLock());
			Database* db = getApp().getLedgerDB()->getDB();

			if(!db->executeSQL(sql, true))
			{
				WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
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

