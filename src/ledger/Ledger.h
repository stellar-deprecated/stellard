#ifndef __LEDGER__
#define __LEDGER__

#include <boost/enable_shared_from_this.hpp>
#include "ripple/types/api/base_uint.h"

using namespace ripple;

namespace stellar
{
	/*
		Ledger headers + ?
	*/

	class Ledger
	{
		uint256     mHash;
		uint256     mParentHash;
		uint256     mTransHash;
		uint256     mAccountHash;
		std::uint64_t      mTotCoins;
		std::uint64_t      mFeePool;		// All the fees collected since last inflation spend
		std::uint32_t		mLedgerSeq;
		std::uint32_t		mInflationSeq;	// the last inflation that was applied 
		// Ripple times are seconds since 1/1/2000 00:00 UTC. You can add 946684800 to a Ripple time to convert it to a UNIX time
		std::uint32_t      mCloseTime;         // when this ledger closed
		std::uint32_t      mParentCloseTime;   // when the previous ledger closed
		int         mCloseResolution;   // the resolution for this ledger close time (2-120 seconds)
		std::uint32_t      mCloseFlags;        // flags indicating how this ledger close took place
		bool        mClosed, mValidated, mValidHash, mAccepted, mImmutable;

		std::uint32_t      mReferenceFeeUnits;                 // Fee units for the reference transaction
		std::uint32_t      mReserveBase, mReserveIncrement;    // Reserve base and increment in fee units
		std::uint64_t      mBaseFee;                           // Ripple cost of the reference transaction

	public:
		typedef std::shared_ptr<Ledger>           pointer;

		Ledger();

		std::uint64_t scaleFeeBase(std::uint64_t fee);
		std::uint64_t getReserve(int increments);

		void updateFees();
	};
}

#endif