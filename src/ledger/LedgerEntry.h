#ifndef __LEDGERENTRY__
#define __LEDGERENTRY__

#include "ripple/types/api/base_uint.h"
#include "ripple_app/misc/SerializedLedger.h"

using namespace ripple;
/*
LedgerEntry
Parent of AccountEntry, TrustLine, OfferEntry
*/
namespace stellar
{
	class LedgerEntry
	{
	protected:
		uint256 mIndex;

		SLE::pointer mSLE; 


		virtual void insertIntoDB() = 0;
		virtual void updateInDB() = 0;
		virtual void deleteFromDB() = 0;

		virtual void calculateIndex() = 0;
	public:
		typedef boost::shared_ptr<LedgerEntry> pointer;

		// calculate the index if you don't have it already
		uint256 getIndex();

		// calculate the hash if you don't have it already
		uint256 getHash();

		static LedgerEntry::pointer makeEntry(SLE::pointer sle);

		// these will do the appropriate thing in the DB and the preimage
		void storeDelete();
		void storeChange();
		void storeAdd();
	};
}

#endif
