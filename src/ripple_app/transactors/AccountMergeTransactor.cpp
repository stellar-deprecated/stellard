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

SETUP_LOG(AccountMergeTransactor)


static void offerAdder(std::list<uint256>& offersList, SLE::ref offer)
{
	if (offer->getType() == ltOFFER)
	{
		offersList.push_front(offer->getIndex());
	}
}

static void rippleStateAdder(std::list<uint256>& stateList, SLE::ref rippleState)
{
	if (rippleState->getType() == ltRIPPLE_STATE)
	{
		stateList.push_front(rippleState->getIndex());
	}
}

TER AccountMergeTransactor::doApply ()
{
	
    WriteLog (lsINFO, AccountMergeTransactor) << "AccountMerge>";


	if (!mSigMaster)
	{
		WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Invalid: Not authorized to merge account. (sig)";

		return temBAD_AUTH_MASTER;
	}

	if (!mTxn.isFieldPresent(sfDestination))
	{
		WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Malformed transaction: Destination is not set.";

		return temDST_NEEDED;
	}

	const RippleAddress aDestination	= mTxn.getFieldAccount(sfDestination);
	const uint160		uDestinationID	= aDestination.getAccountID();

	if (uDestinationID == mTxnAccountID)
	{
		WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Malformed transaction: Destination is source account.";

		return temDST_IS_SRC;
	}


	if (!mEngine->getLedger()->hasAccount(aDestination))
	{
		WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Malformed transaction: Destination account doesn't exist.";

		// TODO: allow sending to nonexistent account? Should we create new one?
		return tecNO_DST;
	}
	
	SLE::pointer    sleDst = mEngine->entryCache(ltACCOUNT_ROOT, Ledger::getAccountRootIndex(aDestination));

	if ((sleDst->getFlags() & lsfRequireDestTag) && !mTxn.isFieldPresent(sfDestinationTag))
	{
		WriteLog(lsINFO, AccountMergeTransactor) << "Payment: Malformed transaction: DestinationTag required.";

		return tefDST_TAG_NEEDED;
	}

	

	/// Manage account IOUs

	std::list<uint256> rippleLinesList;
	mEngine->getLedger()->visitAccountItems(mTxnAccountID, BIND_TYPE(&rippleStateAdder, boost::ref(rippleLinesList), P_1));

	auto pAccItem = AccountItem::pointer(new RippleState());

	// Transfer IOUs
	BOOST_FOREACH(const uint256 itemIndex, rippleLinesList)
	{
		SLE::pointer			sleLine		  = mEngine->entryCache(ltRIPPLE_STATE, itemIndex);
        auto                    pCurrAccItem  = pAccItem->makeItem(mTxnAccountID, sleLine);
        RippleState*			pLine         = (RippleState*) pCurrAccItem.get();

		const STAmount&			saLineBalance = pLine->getBalance();

		assert(pLine->getAccountID() == mTxnAccountID);

		if (saLineBalance < zero){
			WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Invalid: Not authorized to merge account. (balance)";

			return temBAD_AMOUNT;
		}

		if (saLineBalance > zero)
		{
			const uint160 iouIssuerId = pLine->getAccountIDPeer();
			const uint160 uCurrencyId = saLineBalance.getCurrency();
			const uint256 uTrustLineIndex = Ledger::getRippleStateIndex(uDestinationID, iouIssuerId, uCurrencyId);


			SLE::pointer    sleDestinationTrustLine = mEngine->entryCache(ltRIPPLE_STATE, uTrustLineIndex);

			if (sleDestinationTrustLine)
			{	// trust line exists, increase balance
                auto pDestAccItem = pAccItem->makeItem(uDestinationID, sleDestinationTrustLine);
				RippleState* destTrustLine = (RippleState*) pDestAccItem.get();

				assert(destTrustLine->getAccountID() == uDestinationID);

				// if auth was enabled on the account, we need to ensure that the new account can auth as well
				// to avoid being locked out
				if (pLine->getAuthPeer() == true && destTrustLine->getAuthPeer() == false)
				{
					WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Invalid: Destination not authorized to hold IOUs.";

					return terNO_AUTH;
				}
                
                mEngine->entryModify(sleDestinationTrustLine);

                const bool bHigh = uDestinationID > iouIssuerId;
                const STAmount& destBalance = sleDestinationTrustLine->getFieldAmount(sfBalance);
                
                auto finalBalance = bHigh ? destBalance - saLineBalance : destBalance + saLineBalance;

				auto limit = sleDestinationTrustLine->getFieldAmount(bHigh ? sfHighLimit : sfLowLimit); // 0 if no limit

				if ((bHigh && finalBalance < -limit) || (!bHigh && finalBalance > limit))
				{
					WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Invalid: Destination limits too low for transfering IOUs. " << finalBalance << ":" << limit;

					return terNO_AUTH;
				}

				sleDestinationTrustLine->setFieldAmount(sfBalance, finalBalance);

			}
			else
			{
				WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Invalid: Destination has missing trustline IOUs.";

				return terNO_AUTH;
			}
		}

		uint160 uAccountID = sleLine->getFirstOwner().getAccountID();
		uint160 uAccountPeerID = sleLine->getSecondOwner().getAccountID();

		uint160& uHighAccountID = uAccountID > uAccountPeerID ? uAccountID : uAccountPeerID;
		uint160& uLowAccountID = uAccountID < uAccountPeerID ? uAccountID : uAccountPeerID;


		TER terResult = mEngine->view().trustDelete(sleLine, uLowAccountID, uHighAccountID);

		if (terResult != tesSUCCESS){
			WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Deleting trust line failed: " << transHuman(terResult);
            return tefINTERNAL; 
		}

	}

	mEngine->entryModify(sleDst);


	// Transfer stellars

	STAmount saMoveBalance = mSourceBalance;
	mSourceBalance = zero;
	mTxnAccount->setFieldAmount(sfBalance, mSourceBalance);
	sleDst->setFieldAmount(sfBalance, sleDst->getFieldAmount(sfBalance) + saMoveBalance);

	assert(mTxnAccount->getFieldAmount(sfBalance) == zero);


	// Delete account offers

	
	std::list<uint256> offersList;

	mEngine->getLedger()->visitAccountItems(mTxnAccountID, BIND_TYPE(&offerAdder, boost::ref(offersList), P_1));

	WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Deleting " << offersList.size() << " account offers";

	BOOST_FOREACH(const uint256 offerIndex, offersList)
	{
		auto terResult = mEngine->view().offerDelete(offerIndex);

		if (terResult != tesSUCCESS){
			WriteLog(lsINFO, AccountMergeTransactor) << "AccountMerge: Deleting offer failed: " << transHuman(terResult);
            return tefINTERNAL;
		}
	}

	offersList.clear();

	// Delete account itself
	mEngine->view().entryDelete(mTxnAccount);
	

    return tesSUCCESS;
}

}
