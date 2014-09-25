#ifndef __OFFERENTRY__
#define __OFFERENTRY__

#include "LedgerEntry.h"

namespace stellar
{
	class OfferEntry : public LedgerEntry
	{
	public:
		OfferEntry();

		// these will do the appropriate thing in the DB and the preimage
		void storeDelete();
		void storeChange();
		void storeAdd();
	};
}

#endif