#ifndef __OFFERENTRY__
#define __OFFERENTRY__

#include "LedgerEntry.h"


namespace stellar
{
	class OfferEntry : public LedgerEntry
	{
		void insertIntoDB();
		void updateInDB();
		void deleteFromDB();

		void calculateIndex();
	public:
		uint160 mAccountID;
		uint32	mSequence;
		STAmount mTakerPays;
		STAmount mTakerGets;
		bool mPassive;
		uint32 mExpiration;


 		OfferEntry();
		OfferEntry(SLE::pointer sle);

        static void dropAll(LedgerDatabase &db);
        static const char *kSQLCreateStatement;
	};
}

#endif