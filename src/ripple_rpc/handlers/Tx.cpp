//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

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

// {
//   transaction: <hex>
// }
Json::Value RPCHandler::doTx (Json::Value params, Resource::Charge& loadType, Application::ScopedLockType& masterLockHolder)
{
    masterLockHolder.unlock ();

    if (!params.isMember (jss::transaction))
        return rpcError (rpcINVALID_PARAMS);

    bool binary = params.isMember (jss::binary) && params[jss::binary].asBool ();

    std::string strTransaction  = params[jss::transaction].asString ();

    if (Transaction::isHexTxID (strTransaction))
    {
        // transaction by ID
        uint256 txid (strTransaction);

        Transaction::pointer txn = getApp().getMasterTransaction ().fetch (txid, true);

        if (!txn)
            return rpcError (rpcTXN_NOT_FOUND);

#ifdef READY_FOR_NEW_TX_FORMAT
        Json::Value ret;
        ret[jss::transaction] = txn->getJson (0, binary);
#else
        Json::Value ret = txn->getJson (0, binary);
#endif

        bool didMeta = false;

        if (txn->getLedger () != 0)
        {
            Ledger::pointer lgr = mNetOps->getLedgerBySeq (txn->getLedger ());

            

            if (lgr)
            {
                bool okay = false;

                bool isValidLedger = mNetOps->isValidated(lgr);

                // only serve the meta and ledger info if it made it into the network
                // we may want to have an option to see the intermediate state in the future for debug purpose
                if (txn->getStatus() == TransStatus::COMMITTED && isValidLedger)
                {
                    didMeta = true;

                    if (binary)
                    {
                        std::string meta;

                        if (lgr->getMetaHex (txid, meta))
                        {
                            ret[jss::meta] = meta;
                            okay = true;
                        }
                    }
                    else
                    {
                        TransactionMetaSet::pointer set;

                        if (lgr->getTransactionMeta (txid, set))
                        {
                            okay = true;
                            ret[jss::meta] = set->getJson (0);
                        }
                    }

                    if (okay)
                        ret[jss::validated] = isValidLedger;
                }
            }
        }

        if (!didMeta)
        {
            Json::Value jvObj (Json::objectValue);
            std::string sToken;
            std::string sHuman;

            transResultInfo (txn->getResult(), sToken, sHuman);

            jvObj[jss::engine_result]          = sToken;
            jvObj[jss::engine_result_code]     = txn->getResult();
            jvObj[jss::engine_result_message]  = sHuman;

            ret[jss::alt_info] = jvObj;
        }

        return ret;
    }

    return rpcError (rpcNOT_IMPL);
}

} // ripple
