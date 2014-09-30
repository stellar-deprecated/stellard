#ifndef __BUCKETLIST__
#define __BUCKETLIST__

#include <map>
#include "ripple/types/api/base_uint.h"
#include "ripple_app/misc/SerializedLedger.h"
#include "CanonicalLedgerForm.h"

using namespace ripple;

/*
bucket list will be a collection of temporal buckets
It stores the hashes of the ledger entries

Do we need the SLE in the bucket list?

*/
namespace stellar
{

	class BucketList : public CanonicalLedgerForm
	{
		// index , SLE
		std::map<uint256, SLE::pointer> mPendingAdds;
		std::map<uint256, SLE::pointer> mPendingUpdates;
		std::map<uint256, SLE::pointer> mPendingDeletes;
		uint256 mHash;
		void calculateHash();
	public:

		bool load(uint256 ledgerHash);

		//void setParentBucketList(BucketList::pointer parent);
		void addEntry(uint256& newHash, SLE::pointer newEntry);
		void updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry);
		void deleteEntry(uint256& hash);
		void closeLedger();  // need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets

		uint256 getHash();
	};
}
#endif