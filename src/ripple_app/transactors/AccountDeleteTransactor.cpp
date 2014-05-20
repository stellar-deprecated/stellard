//------------------------------------------------------------------------------
/*
    This file is part of stellard: https://github.com/stellar/stellard
    Copyright (c) 2014 Stellar Development Foundation Inc.

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

namespace ripple {

SETUP_LOG(AccountDeleteTransactor)


static void offerAdder(std::list<const uint256>& offersList, SLE::ref offer)
{
	if (offer->getType() == ltOFFER)
	{
		//offersList.push_front(offer->getIndex());
	}
}

static void rippleStateAdder(std::list<const uint256>& stateList, SLE::ref rippleState)
{
	if (rippleState->getType() == ltRIPPLE_STATE)
	{
		//stateList.push_front(rippleState->getIndex());
	}
}

TER AccountDeleteTransactor::doApply ()
{
	
    WriteLog (lsINFO, AccountDeleteTransactor) << "AccountDelete>";


	if (!mSigMaster)
	{
		WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Invalid: Not authorized to delete account.";

		return temBAD_AUTH_MASTER;
	}

	if (!mTxn.isFieldPresent(sfDestination))
	{
		WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Malformed transaction: Destination is not set.";

		return temDST_NEEDED;
	}

	const RippleAddress aDestination	= mTxn.getFieldAccount(sfDestination);
	const uint160		uDestinationID	= aDestination.getAccountID();

	if (uDestinationID == mTxnAccountID)
	{
		WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Malformed transaction: Destination is source account.";

		return temDST_IS_SRC;
	}


	if (!mEngine->getLedger()->hasAccount(aDestination))
	{
		WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Malformed transaction: Destination account doesn't exist.";

		// TODO: allow sending to nonexistent account? Should we create new one?
		return tecNO_DST;
	}
	
	SLE::pointer    sleDst = mEngine->entryCache(ltACCOUNT_ROOT, Ledger::getAccountRootIndex(aDestination));

	if ((sleDst->getFlags() & lsfRequireDestTag) && !mTxn.isFieldPresent(sfDestinationTag))
	{
		WriteLog(lsINFO, AccountDeleteTransactor) << "Payment: Malformed transaction: DestinationTag required.";

		return tefDST_TAG_NEEDED;
	}

	

	/// Manage account IOUs

	std::list<const uint256> rippleLinesList;
	mEngine->getLedger()->visitAccountItems(mTxnAccountID, BIND_TYPE(&rippleStateAdder, boost::ref(rippleLinesList), P_1));

	auto pAccItem = AccountItem::pointer(new RippleState());

	// Transfer IOUs
	BOOST_FOREACH(const uint256 itemIndex, rippleLinesList)
	{
		SLE::pointer			sleLine		  = mEngine->entryCache(ltRIPPLE_STATE, itemIndex);
		AccountItem*			pAi			  = pAccItem->makeItem(mTxnAccountID, sleLine).get();
		RippleState*			line		  = (RippleState*)pAi;
		const STAmount&			saLineBalance = line->getBalance();


		if (saLineBalance.isNegative()){
			WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Invalid: Not authorized to delete account.";

			return temBAD_AMOUNT;
		}

		if (saLineBalance.isPositive())
		{
			const uint160 iouIssuerId = line->getAccountIDPeer();
			const uint160 uCurrencyId = saLineBalance.getCurrency();
			const uint256 uTrustLineIndex = Ledger::getRippleStateIndex(uDestinationID, iouIssuerId, uCurrencyId);


			SLE::pointer    sleDestinationTrustLine = mEngine->entryCache(ltRIPPLE_STATE, uTrustLineIndex);

			if (sleDestinationTrustLine)
			{	// trust line exists, increase balance
				RippleState* destTrustLine = (RippleState*)pAccItem->makeItem(uDestinationID, sleDestinationTrustLine).get();

				if (line->getAuthPeer() == true && destTrustLine->getAuthPeer() == false)
				{
					WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Invalid: Destination not authorized to hold IOUs.";

					//TODO right return code?
					return terNO_AUTH;
				}

				const STAmount& destBalance = sleDestinationTrustLine->getFieldAmount(sfBalance);

				auto finalBalance = destBalance + saLineBalance;
				sleDestinationTrustLine->setFieldAmount(sfBalance, finalBalance);

#if !NDEBUG
				auto testBalance = sleDestinationTrustLine->getFieldAmount(sfBalance);
				assert(finalBalance == testBalance);
#endif

				mEngine->entryModify(sleDestinationTrustLine);

			}
			else
			{	// create new trustline with no trust and set balance

				const bool      bHigh = uDestinationID > iouIssuerId;
				const STAmount	trustAmount{ uCurrencyId, uDestinationID, 0 }; // Don't set trust for (unassuming) destination

				mEngine->getNodes().trustCreate(bHigh,
					uDestinationID,
					iouIssuerId,
					uTrustLineIndex,
					sleDst,
					false,
					false,
					saLineBalance,
					trustAmount);

			}
		}

		uint160 uAccountID = sleLine->getFirstOwner().getAccountID();
		uint160 uAccountPeerID = sleLine->getSecondOwner().getAccountID();

		uint160& uHighAccountID = uAccountID > uAccountPeerID ? uAccountID : uAccountPeerID;
		uint160& uLowAccountID = uAccountID < uAccountPeerID ? uAccountID : uAccountPeerID;


		TER terResult = mEngine->getNodes().trustDelete(sleLine, uLowAccountID, uHighAccountID);

		if (terResult != tesSUCCESS){
			WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Deleting trust line failed: " << transHuman(terResult);
		}

	}

	mEngine->entryModify(sleDst);


	// Transfer stellars

	STAmount saMoveBalance = mSourceBalance;
	mSourceBalance.zero();
	mTxnAccount->setFieldAmount(sfBalance, mSourceBalance);
	sleDst->setFieldAmount(sfBalance, sleDst->getFieldAmount(sfBalance) + saMoveBalance);

	assert(mTxnAccount->getFieldAmount(sfBalance).isZero() == true);


	// Delete account offers

	std::list<const uint256> offersList;

	mEngine->getLedger()->visitAccountItems(mTxnAccountID, BIND_TYPE(&offerAdder, boost::ref(offersList), P_1));

	WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Deleting " << offersList.size() << " account offers";

	BOOST_FOREACH(const uint256 offerIndex, offersList)
	{
		auto terResult = mEngine->getNodes().offerDelete(offerIndex);

		if (terResult != tesSUCCESS){
			WriteLog(lsINFO, AccountDeleteTransactor) << "AccountDelete: Deleting offer failed: " << transHuman(terResult);
		}
	}

	offersList.clear();


	// Delete account itself

	mEngine->getNodes().entryDelete(mTxnAccount);


    return tesSUCCESS;
}

}
