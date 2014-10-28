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

namespace ripple
{
    // Historical quirks are nearly-compatible but not-quite-compatible
    // perturbations in the stellard logic that we have changed or fixed
    // over the life of the codebase, that are small enough to be handled
    // by a conditional branch in the code somewhere, but that need to be
    // accounted for with a history range so that we can replay history
    // before and after the quirk was changed / fixed.
    //
    // We're using the term "quirk" here so that code that handles these
    // things will be easy to grep for, relative to code that talks about
    // still-live bugs. A historical quirk is, by definition, something we
    // have fixed in the _past_ and is no longer a going concern of a bug.
    enum HistoricalQuirk
    {
        QuirkNonzeroInflationFees,
        QuirkLongPaymentPaths,
        QuirkAsymmetricTrustlines,
        QuirkUseLsfDisableMaster,
        QuirkUseLsfPasswordSpent,
    };

    // Struct merely exists to attach logging to. Container for static methods.
    struct LedgerDump
    {
        static bool enactHistoricalQuirk (HistoricalQuirk k);
        static void dumpLedger (int ledgerNum);
        static void dumpTransactions (std::string const& filename);
        static void loadTransactions (std::string const& filename);
    };
}
