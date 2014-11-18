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
    public:
        uint160 mAccountID;
        uint64 mBalance;
        uint32 mSequence;
        uint32 mOwnerCount;
        uint32 mTransferRate;
        uint160 mInflationDest;
        StellarPublicKey mPubKey; // SANITY make this optional and map to nullable in SQL
        bool mRequireDest;
        bool mRequireAuth;

        AccountEntry(SLE::pointer sle);

        bool loadFromDB(uint256& index);
        bool loadFromDB(); // load by accountID

        static void dropAll(LedgerDatabase &db);
        static void appendSQLInit(vector<const char*> &init);
    };
}

#endif

