#ifndef __LEDGERPREIMAGE__
#define __LEDGERPREIMAGE__

#include <vector>
#include <boost/shared_ptr.hpp>
#include "ripple/types/api/base_uint.h"
#include "ripple_app/misc/SerializedLedger.h"
#include "ripple_app/ledger/Ledger.h"

/*
This is the form of the ledger that is hashed to create the ledger id.
*/
using namespace ripple;
using namespace std;

namespace stellar
{
	class CanonicalLedgerForm
	{
	public:
		typedef boost::shared_ptr<CanonicalLedgerForm> pointer;

		// load up our last known version of this
		virtual bool load(uint256 ledgerHash) = 0;

		// gives us a list of the LedgerEntries that have changed since a particular Ledger in the past
		// vector is new SLE,oldSLE
		virtual void getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList) = 0;

		virtual void addEntry(uint256& newHash, SLE::pointer newEntry)=0;
		virtual void updateEntry(uint256& oldHash, uint256& newHash, SLE::pointer updatedEntry)=0;
		virtual void deleteEntry(uint256& hash)=0;
		virtual void closeLedger()=0;  // need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets

		virtual uint256 getHash()=0;

        virtual ripple::Ledger::pointer getLegacyLedger() = 0;
	};
}

#endif


