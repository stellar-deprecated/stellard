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


#include "../../ripple_rpc/impl/TransactionSign.h"

namespace ripple {

	// create the inflate transaction
	// pull the inflate seq # from the ledger
	Json::Value RPCHandler::doInflate(Json::Value params, Resource::Charge& loadType, Application::ScopedLockType& masterLockHolder)
	{
		masterLockHolder.unlock();

		loadType = Resource::feeMediumBurdenRPC;

		uint32 inflationSeq = getApp().getLedgerMaster().getClosedLedger()->getInflationSeq();
		params["tx_json"]["TransactionType"] = "Inflation";
		params["tx_json"]["InflateSeq"] = inflationSeq;
		params["tx_json"]["Account"] = "gJgtFgEAtBbv9t6poqAy2sQHL95i6VvnD4";
		params["tx_json"]["Fee"] = "0";
		params["secret"] = "s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN";

		return RPC::transactionSign(params, true, true, masterLockHolder, *mNetOps, mRole);
	}

} // ripple
