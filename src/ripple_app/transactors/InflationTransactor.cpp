//------------------------------------------------------------------------------
/*
This file is part of stellard: https://github.com/stellar/stellard
Copyright (c) 2014 Stellar Development Foundation.

Permission to use, copy, modify, and/or distribute this software for any
purpose  with  or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================
#include "InflationTransactor.h"
#include <boost/multiprecision/cpp_int.hpp>

#define INFLATION_FREQUENCY			(60*60*24*7)  // every 7 days
//inflation is .000190721 per 7 days, or 1% a year
#define INFLATION_RATE_TRILLIONTHS      190721000L
#define TRILLION                    1000000000000L
#define INFLATION_WIN_MIN_PERCENT	  15000000000L   // 1.5%
#define INFLATION_NUM_WINNERS		50
#define INFLATION_START_TIME		(1403900503-946684800) // seconds since 1/1/2000  start july 1st
#ifndef MIN
#define MIN(x,y)  ((x)<(y) ? (x) : (y))
#endif

/*
What about when an account that wins a dole is now gone?
*/


namespace ripple {

	bool voteSorter(const std::pair<uint160, uint64>  &p1, const std::pair<uint160, uint64> &p2)
	{
		return p1.second > p2.second;
	}

	SETUP_LOG(InflationTransactor)


	TER InflationTransactor::checkSig()
	{
		return tesSUCCESS;
	}

	TER InflationTransactor::payFee()
	{
		STAmount saPaid = mTxn.getTransactionFee();
		if (saPaid == zero)
			return tesSUCCESS;
		return temBAD_FEE;
	}

	TER InflationTransactor::doApply()
	{
		WriteLog(lsWARNING, InflationTransactor) << "InflationTransactor::doApply() ****************************** ";

		TER terResult = tesSUCCESS;
		
		// make sure it is time to apply inflation
		// make sure the seq number of this inflation transaction is correct

		uint32 seq = mTxn.getFieldU32(sfInflateSeq);
		

		if (seq != mEngine->getLedger()->getInflationSeq())
		{
			WriteLog(lsINFO, InflationTransactor) << "doInflation: Invalid Seq number.";

			return telNOT_TIME;
		}

		uint32 closeTime=mEngine->getLedger()->getParentCloseTimeNC();
		uint32 nextTime = (INFLATION_START_TIME + seq*INFLATION_FREQUENCY);
		if (closeTime < nextTime)
		{
			WriteLog(lsINFO, InflationTransactor) << "doInflation: Too early.";

			return telNOT_TIME;
		}

		if (!mEngine->getLedger()->getInflationAllowed())
		{
			WriteLog(lsINFO, InflationTransactor) << "doInflation: cannot allow multiple inflations in a round.";

			return telNOT_TIME;
		}

		// check previous ledger if this should be applied now
		// loop through all accounts and tally votes
		// dole out the inflation amount to the winners
		uint256 parentHash=mEngine->getLedger()->getParentHash();
		Ledger::pointer votingLedger=getApp().getLedgerMaster().getLedgerByHash(parentHash);
		if (votingLedger)
		{
			std::map< uint160, uint64 > voteTally;
			
			// TODO: is there a better way to do this than walk every element in the ledger?
			const SHAMap::pointer votingLedgerItems = votingLedger->peekAccountStateMap();
			SHAMapItem::pointer item = votingLedgerItems->peekFirstItem();
			while (item)
			{
				
				SLE::pointer s=boost::make_shared<SLE>(item->peekSerializer(), item->getTag());

				if (s->getType() == ltACCOUNT_ROOT)
				{
					if (s->isFieldPresent(sfInflationDest))
					{
						uint160 addr=s->getFieldAccount160(sfInflationDest);
						STAmount balance = s->getFieldAmount(sfBalance);
						if (voteTally.find(addr) == voteTally.end()) voteTally[addr] = balance.getNValue();
						else voteTally[addr] = voteTally[addr] + balance.getNValue();
					}
					
				}

				item = votingLedgerItems->peekNextItem(item->getTag());
			}

			
			// sort the votes
			std::vector< std::pair<uint160, uint64> > sortedVotes;
			copy(voteTally.begin(), voteTally.end(), back_inserter(sortedVotes));

			// TEMP: debug
			typedef std::pair< uint160, uint64 > vote_pair;
			BOOST_FOREACH(vote_pair& vote, sortedVotes)
			{
				RippleAddress tempAddr;
				tempAddr.setAccountID(vote.first);
				WriteLog(lsWARNING, InflationTransactor) << "votesGotten: " << vote.second << " addr: " << tempAddr.ToString();
			}

			sort(sortedVotes.begin(), sortedVotes.end(), voteSorter);

			boost::multiprecision::cpp_int minBalance( mEngine->getLedger()->getTotalCoins());
			boost::multiprecision::cpp_int minWinMultiplier( INFLATION_WIN_MIN_PERCENT );
			boost::multiprecision::cpp_int inflRateDivider( TRILLION );
			
			minBalance *= minWinMultiplier;
			minBalance /= inflRateDivider;

			WriteLog(lsWARNING, InflationTransactor) << "minBalance: " << minBalance ;

			uint64 totalVoted = 0;
			int maxIndex = MIN(INFLATION_NUM_WINNERS, sortedVotes.size());
			for (int n = 0; n < maxIndex; n++)
			{
				boost::multiprecision::cpp_int votesGotten( sortedVotes[n].second );

				WriteLog(lsWARNING, InflationTransactor) << "votesGotten: " << votesGotten;

				if (votesGotten > minBalance)
				{
					totalVoted += sortedVotes[n].second;
				}else 
				{
					if (totalVoted)
						maxIndex = n;
					break;
				}
			}

			if (!totalVoted)
			{  // no one crossed the threshold so just take top N
				for (int n = 0; n < maxIndex; n++)
				{
					RippleAddress tempAddr;
					tempAddr.setAccountID(sortedVotes[n].first);
					WriteLog(lsWARNING, InflationTransactor) << "votes: " << sortedVotes[n].second << " address: " << tempAddr.ToString();
					totalVoted += sortedVotes[n].second;
				}
			}


			boost::multiprecision::cpp_int biCoinsToDole( mEngine->getLedger()->getTotalCoins() );
			boost::multiprecision::cpp_int inflRateMultiplier( INFLATION_RATE_TRILLIONTHS );
			boost::multiprecision::cpp_int poolFee( mEngine->getLedger()->getFeePool() );

			/// coinsToDole = totalCoins * INFLATION_RATE + feePool
			biCoinsToDole *= inflRateMultiplier;
			biCoinsToDole /= inflRateDivider;
			biCoinsToDole += poolFee;


			boost::multiprecision::cpp_int biTotalVoted( totalVoted );

			WriteLog(lsWARNING, InflationTransactor) << "totalVoted: " << totalVoted << " bi:" << biTotalVoted << " coinsToDole: " << biCoinsToDole;

			
			for (int n = 0; n < maxIndex; n++)
			{
				/// coinsDoled = coinToDole * ( votes / totalVoted )
				boost::multiprecision::cpp_int biCoinsDoled( sortedVotes[n].second );
				biCoinsDoled *= biCoinsToDole;
				biCoinsDoled /= biTotalVoted;

				uint64 coinsDoled = static_cast<uint64>(biCoinsDoled);

				WriteLog(lsWARNING, InflationTransactor) << "coinsDoled: " << coinsDoled;

				SLE::pointer account = mEngine->entryCache(ltACCOUNT_ROOT, Ledger::getAccountRootIndex(sortedVotes[n].first));
				
				if (account)
				{
					mEngine->entryModify(account);
					account->setFieldAmount(sfBalance, account->getFieldAmount(sfBalance) + coinsDoled);
					mEngine->getLedger()->inflateCoins(coinsDoled);
				}
				else
				{
					RippleAddress tempAddr;
					tempAddr.setAccountID(sortedVotes[n].first);
					WriteLog(lsERROR, InflationTransactor) << "Inflation dest account not found: " << tempAddr.ToString();
				}
			}

			mEngine->getLedger()->incrementInflationSeq();
			
		}
		else
		{
			WriteLog(lsINFO, InflationTransactor) << "doInflation: Ledger not found?";

			return temUNKNOWN;
		}

		

		return terResult;
	}

}

