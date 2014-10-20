#ifndef __TRUSTLINE__
#define __TRUSTLINE__

#include "ripple_app/misc/SerializedTransaction.h"
#include "LedgerEntry.h"
#include "ripple_data/protocol/TER.h"

namespace stellar {

	class TrustSetTx;
	class AccountEntry;

	//index is: 
	class TrustLine : public LedgerEntry
	{
		void calculateIndex();
		
		void insertIntoDB();
		void updateInDB();
		void deleteFromDB();

		void serialize(uint256& hash, SLE::pointer& ret);

	public:
		uint160 mLowAccount;
		uint160 mHighAccount;
		uint160 mCurrency;

		STAmount mLowLimit;
		STAmount mHighLimit;
		STAmount mBalance;	// SANITY: check this  positive balance means credit is held by high account

		bool mLowAuthSet;  // if the high account has authorized the low account to hold its credit
		bool mHighAuthSet;

		TrustLine();
		TrustLine(SLE::pointer sle);

		bool loadFromDB(const uint256& index);
		TER fromTx(AccountEntry& signingAccount, TrustSetTx* tx);

        void setFromCurrentRow(Database *db);
		
        static void dropAll(LedgerDatabase &db);
        static void appendSQLInit(vector<const char*> &init);
	};
}

#endif
