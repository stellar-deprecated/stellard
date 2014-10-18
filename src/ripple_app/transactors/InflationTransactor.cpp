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
#include "ripple_data/protocol/TER.h"
#include "ripple_basics/log/LogPartition.h"
#include "ripple_basics/log/Log.h"
#include "ripple_app/tx/TransactionEngine.h"
#include "ripple_basics/types/BasicTypes.h"
#include "ripple_app/main/Application.h"
#include "ripple_app/data/DatabaseCon.h"

using namespace std;

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

		// don't bother to process on the first apply
		if(!mEngine->mClosingLedger) return tesSUCCESS;

		boost::multiprecision::cpp_int minBalance(mEngine->getLedger()->getTotalCoins());
		boost::multiprecision::cpp_int minWinMultiplier(INFLATION_WIN_MIN_PERCENT);
		boost::multiprecision::cpp_int inflRateDivider(TRILLION);
		boost::multiprecision::cpp_int totalVoted;

		minBalance *= minWinMultiplier;
		minBalance /= inflRateDivider;

		WriteLog(lsDEBUG, InflationTransactor) << "minBalance: " << minBalance;

        // limit to large accounts for now, will need different logic to perform well
		string sql("SELECT sum(balance) as votes,inflationDest from Accounts where inflationDest is not NULL AND inflationDest is NOT 'ggggggggggggggggggggghoLvTs' AND balance > 1000000000 group by inflationDest order by votes desc limit 50");
		
		vector< pair<uint160, boost::multiprecision::cpp_int> > winners;

		{
			Database* db = getApp().getWorkingLedgerDB()->getDB();

			if(db->executeSQL(sql, true) && db->startIterRows())
			{
				totalVoted = db->getBigInt("votes");
				uint160 destAccount=db->getAccountID("inflationDest");
				winners.push_back(pair<uint160, boost::multiprecision::cpp_int>(destAccount, totalVoted));

				WriteLog(lsWARNING, InflationTransactor) << "totalVoted: " << totalVoted << " minBalance: " << minBalance;

				if(totalVoted <= minBalance)
				{  // need to take top 50
					minBalance = 0;
				} 
				
				while(db->getNextRow())
				{
					boost::multiprecision::cpp_int votes = db->getBigInt("votes");
					destAccount = db->getAccountID("inflationDest");
					if(votes>minBalance)
					{
						totalVoted += votes;
						winners.push_back(pair<uint160, boost::multiprecision::cpp_int>(destAccount, votes));
					}else break;
				};
				db->endIterRows();
			}else
			{
				WriteLog(lsERROR, InflationTransactor) << "SELECT failed";
			}
		}

	

		boost::multiprecision::cpp_int biCoinsToDole( mEngine->getLedger()->getTotalCoins() );
		boost::multiprecision::cpp_int inflRateMultiplier( INFLATION_RATE_TRILLIONTHS );
		boost::multiprecision::cpp_int poolFee( mEngine->getLedger()->getFeePool() );

		/// coinsToDole = totalCoins * INFLATION_RATE + feePool
		biCoinsToDole *= inflRateMultiplier;
		biCoinsToDole /= inflRateDivider;
		biCoinsToDole += poolFee;

		WriteLog(lsWARNING, InflationTransactor) << "totalVoted: " << totalVoted << " coinsToDole: " << biCoinsToDole;

			
		for (int n = 0; n < winners.size(); n++)
		{
			/// coinsDoled = coinToDole * ( votes / totalVoted )
			boost::multiprecision::cpp_int biCoinsDoled=winners[n].second;
			biCoinsDoled *= biCoinsToDole;
			biCoinsDoled /= totalVoted;

			uint64 coinsDoled = static_cast<uint64>(biCoinsDoled);

			WriteLog(lsWARNING, InflationTransactor) << "coinsDoled: " << coinsDoled;

			SLE::pointer account = mEngine->entryCache(ltACCOUNT_ROOT, Ledger::getAccountRootIndex(winners[n].first));
				
			if (account)
			{
				mEngine->entryModify(account);
				account->setFieldAmount(sfBalance, account->getFieldAmount(sfBalance) + coinsDoled);
				mEngine->getLedger()->inflateCoins(coinsDoled);
			}
			else
			{
				RippleAddress tempAddr;
				tempAddr.setAccountID(winners[n].first);
				WriteLog(lsERROR, InflationTransactor) << "Inflation dest account not found: " << tempAddr.ToString();
			}
		}

		mEngine->getLedger()->incrementInflationSeq();
			
		return terResult;
	}

}

