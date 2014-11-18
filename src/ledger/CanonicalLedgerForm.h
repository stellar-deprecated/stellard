#ifndef __CANONICALLEDGERFORMAT__
#define __CANONICALLEDGERFORMAT__

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
        typedef std::shared_ptr<CanonicalLedgerForm> pointer;

        // save last known version
        virtual void save() = 0;

        virtual void closeLedger() = 0;  // need to call after all the tx have been applied to save that last versions of the ledger entries into the buckets

        // returns current ledger hash (recomputes if necessary)
        virtual uint256 getHash() = 0;

        // computes delta with a particular ledger in the past
        virtual bool getDeltaSince(CanonicalLedgerForm::pointer pastCLF, SHAMap::Delta& retList) = 0;


        // should not depend on this
        virtual ripple::Ledger::pointer getLegacyLedger() = 0;
    };
}

#endif


