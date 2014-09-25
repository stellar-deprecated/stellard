#ifndef __ACCOUNTENTRY__
#define __ACCOUNTENTRY__

#include "LedgerEntry.h"
#include "ripple_data/protocol/LedgerFormats.h"
#include "ripple_data/protocol/TER.h"
#include "ripple_data/crypto/StellarPublicKey.h"

namespace stellar
{
	class AccountEntry : public LedgerEntry
	{
		void calculateIndex();

		void insertIntoDB();
		void updateInDB();
		void deleteFromDB();

		void serialize(uint256& hash, SLE::pointer& ret);
	public:
		uint160 mAccountID;
		uint64 mBalance;
		uint32 mSequence;
		uint32 mOwnerCount;
		uint32 mTransferRate;
		uint160 mInflationDest;
		StellarPublicKey mPubKey;
		bool mRequireDest;
		bool mRequireAuth;


		AccountEntry();
		AccountEntry(uint160 id);

		bool loadFromDB(uint256& index);
		bool loadFromDB(); // load by accountID

		bool checkFlag(LedgerSpecificFlags flag);

		// will return tesSUCCESS or that this account doesn't have the reserve to do this
		TER tryToIncreaseOwnerCount();

		
	};
}

#endif

