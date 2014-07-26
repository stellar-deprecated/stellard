//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

TER AccountSetTransactor::doApply ()
{
    std::uint32_t const uTxFlags = mTxn.getFlags ();

    std::uint32_t const uFlagsIn = mTxnAccount->getFieldU32 (sfFlags);
    std::uint32_t uFlagsOut = uFlagsIn;

    std::uint32_t const uSetFlag = mTxn.getFieldU32 (sfSetFlag);
    std::uint32_t const uClearFlag  = mTxn.getFieldU32 (sfClearFlag);

    // legacy AccountSet flags
    bool bSetRequireDest   = (uTxFlags & TxFlag::requireDestTag) || (uSetFlag == asfRequireDest);
    bool bClearRequireDest = (uTxFlags & tfOptionalDestTag) || (uClearFlag == asfRequireDest);
    bool bSetRequireAuth   = (uTxFlags & tfRequireAuth) || (uSetFlag == asfRequireAuth);
    bool bClearRequireAuth = (uTxFlags & tfOptionalAuth) || (uClearFlag == asfRequireAuth);

    if (uTxFlags & tfAccountSetMask)
    {
        m_journal.trace << "Malformed transaction: Invalid flags set.";
        return temINVALID_FLAG;
    }

    //
    // RequireAuth (if this issuer needs to allow others to hold its IOUs)
    //

    if (bSetRequireAuth && bClearRequireAuth)
    {
        m_journal.trace << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    if (bSetRequireAuth && !is_bit_set (uFlagsIn, lsfRequireAuth))
    {
        if (!mEngine->view().dirIsEmpty (Ledger::getOwnerDirIndex (mTxnAccountID)))
        {
            m_journal.trace << "Retry: Owner directory not empty.";

            return is_bit_set(mParams, tapRETRY) ? terOWNERS : tecOWNERS;
        }

        m_journal.trace << "Set RequireAuth.";
        uFlagsOut   |= lsfRequireAuth;
    }

    if (bClearRequireAuth && is_bit_set (uFlagsIn, lsfRequireAuth))
    {
        m_journal.trace << "Clear RequireAuth.";
        uFlagsOut   &= ~lsfRequireAuth;
    }

    //
    // RequireDestTag
    //

    if (bSetRequireDest && bClearRequireDest)
    {
        m_journal.trace << "Malformed transaction: Contradictory flags set.";
        return temINVALID_FLAG;
    }

    if (bSetRequireDest && !is_bit_set (uFlagsIn, lsfRequireDestTag))
    {
        m_journal.trace << "Set lsfRequireDestTag.";
        uFlagsOut   |= lsfRequireDestTag;
    }

    if (bClearRequireDest && is_bit_set (uFlagsIn, lsfRequireDestTag))
    {
        m_journal.trace << "Clear lsfRequireDestTag.";
        uFlagsOut   &= ~lsfRequireDestTag;
    }

  
    //
    // InflationDest
    //

    if ( mTxn.isFieldPresent(sfInflationDest) )
    {
        RippleAddress aInflationDest = mTxn.getFieldAccount(sfInflationDest);

		if ( !aInflationDest.isSet() ){
			m_journal.debug << "AccountSet: Removing inflation destination.";

			mTxnAccount->makeFieldAbsent(sfInflationDest);
		}
		else
		{
			if (!mEngine->getLedger()->hasAccount(aInflationDest))
			{
				m_journal.debug << "AccountSet: Inflation destination account doesn't exist.";

				return temDST_NEEDED;
			}

			m_journal.debug << "AccountSet: Set inflation destination account.";

			mTxnAccount->setFieldAccount(sfInflationDest, aInflationDest);
		}

    }

    

    //
    // TransferRate
    //

    if (mTxn.isFieldPresent (sfTransferRate))
    {
        std::uint32_t      uRate   = mTxn.getFieldU32 (sfTransferRate);

        if (!uRate || uRate == QUALITY_ONE)
        {
            m_journal.trace << "unset transfer rate";
            mTxnAccount->makeFieldAbsent (sfTransferRate);
        }
        else if (uRate > QUALITY_ONE)
        {
            m_journal.trace << "set transfer rate";
            mTxnAccount->setFieldU32 (sfTransferRate, uRate);
        }
        else
        {
            m_journal.trace << "bad transfer rate";
            return temBAD_TRANSFER_RATE;
        }
    }

    if (uFlagsIn != uFlagsOut)
        mTxnAccount->setFieldU32 (sfFlags, uFlagsOut);

    return tesSUCCESS;
}

}
