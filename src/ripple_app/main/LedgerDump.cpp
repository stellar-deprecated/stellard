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

#include <fstream>

namespace ripple
{

SETUP_LOG(LedgerDump)

// ASCII 0x1E "record separator"
static char const DUMP_DELIM = '\x1E';

/**
 * Throw an exception `b` is false.
 */
static void
require (bool b, char const* msg)
{
    if (!b)
        throw std::runtime_error (msg);
}


// Very small amount of global-static state to track replay.
static LedgerSeq gLedgerSeq = 0;
static std::map<HistoricalQuirk, std::pair<LedgerSeq, LedgerSeq>> const gQuirks =
{
    { QuirkNonzeroInflationFees, { 0, 100000 } },
    { QuirkLongPaymentPaths,     { 87668, 87668 } },
    { QuirkAsymmetricTrustlines, { 0, 900000 } },
    { QuirkUseLsfDisableMaster,  { 0, 100000 } },
    { QuirkUseLsfPasswordSpent,  { 0, 100000 } },
};

bool
LedgerDump::enactHistoricalQuirk (HistoricalQuirk q)
{
    auto i = gQuirks.find (q);
    if (i == gQuirks.end ())
        return false;
    return i->second.first <= gLedgerSeq &&
        gLedgerSeq <= i->second.second;
}

/**
 * Instantiate an application and dump a single JSON object,
 * representing a ledger, to standard out.
 */
void
LedgerDump::dumpLedger (int ledgerNum)
{
    std::unique_ptr <Application> app (make_Application ());
    app->setup ();
    auto &lm = app->getLedgerMaster ();
    auto ledger = lm.getLedgerBySeq (ledgerNum);
    std::cout << ledger->getJson (LEDGER_JSON_DUMP_TSTR|
                                  LEDGER_JSON_EXPAND) << std::endl;
    exit (0);
}


/**
 * Instantiate an application, enumerate all its ledgers, and dump them
 * to `filename` as a stream of JSON objects, one per ledger,
 * delimited by the `DUMP_DELIM` character.
 */
void
LedgerDump::dumpTransactions (std::string const& filename)
{
    std::ofstream out (filename);
    require(bool(out), "Cannot open output file");

    std::unique_ptr <Application> app (make_Application ());
    std::uint32_t minLedgerSeq = 0, maxLedgerSeq = static_cast<uint32_t> (-1);
    app->setup ();
    auto &lm = app->getLedgerMaster ();
    std::map< std::uint32_t, std::pair<uint256, uint256> >
        ledgerHashes = Ledger::getHashesByIndex (minLedgerSeq, maxLedgerSeq);

    WriteLog (lsINFO, LedgerDump)
        << "Dumping " << ledgerHashes.size () << " ledgers to " << filename;

    auto nLedgers = 0;
    for (auto const &ix : ledgerHashes)
    {
        if (nLedgers != 0)
        {
            out << DUMP_DELIM << std::endl;
        }

        if ((nLedgers & 0xfff) == 0) {
            Job j;
            app->doSweep (j);
        }

        nLedgers++;
        auto ledger = lm.getLedgerBySeq (ix.first);
        out << ledger->getJson (LEDGER_JSON_BULK) << std::endl;
    }

    WriteLog (lsINFO, LedgerDump) << "Dumped " << ledgerHashes.size()
                                  << " ledgers to " << filename;
    exit (0);
}

/**
 * Throw an exception if `want` and `have` are not equal.
 */
template<typename T> static void
requireEqual(char const* name, T const& want, T const &have)
{
    WriteLog (lsTRACE, LedgerDump) << name << ": want " << want;
    WriteLog (lsTRACE, LedgerDump) << name << ": have " << have;
    if (want != have)
        throw std::runtime_error (std::string(name) + " mismatch");
}


/**
 * Decode a JSON value representing a ledger (as found in a dump file) into
 * a 3-tuple of: #1 a Ledger object, #2 a SHAMap holding a set of
 * transactions, and #3 a vector of transaction IDs indicating the order
 * that the transactions in the SHAMap were applied in the Ledger.
 */
static std::tuple<Ledger::pointer, SHAMap::pointer, std::vector<uint256> >
loadLedgerAndTransactionsFromJSON(Application& app, Json::Value const& j)
{
    require (j.isMember ("raw") && j["raw"].isString (),
             "JSON ledger \"raw\" entry missing or non-string");

    std::pair<Blob, bool> ledgerBlob = strUnHex (j["raw"].asString ());
    require (ledgerBlob.second, "Error decoding ledger \"raw\" field ");

    Ledger::pointer ledger = boost::make_shared<Ledger> (ledgerBlob.first, true);

    require (j.isMember ("txs") && j["txs"].isArray(),
             "JSON ledger \"txs\" entry missing or non-array");

    // Fill a SHAMap full of the current transactions.
    SHAMap::pointer txSet =
        boost::make_shared<SHAMap> (smtTRANSACTION, app.getFullBelowCache());

    std::vector<uint256> txOrder;

    for (auto const& jtx : j["txs"])
    {
        std::pair<Blob, bool> txBlob = strUnHex (jtx.asString ());
        require (txBlob.second, "Error decoding tx");

        Serializer ser (txBlob.first);
        SerializerIterator sit (ser);
        SerializedTransaction stx (sit);

        auto txID = stx.getTransactionID();
        require (txSet->addItem (SHAMapItem (txID, ser), true, true),
                 "Error adding transaction");
        txOrder.push_back (txID);
    }
    return std::make_tuple (ledger, txSet, txOrder);
}

static std::map<LedgerEntryType, std::string>
entryTypeNames =
{
    { ltINVALID,       "ltINVALID" },
    { ltACCOUNT_ROOT,  "ltACCOUNT_ROOT" },
    { ltDIR_NODE,      "ltDIR_NODE" },
    { ltGENERATOR_MAP, "ltGENERATOR_MAP" },
    { ltRIPPLE_STATE,  "ltRIPPLE_STATE" },
    { ltNICKNAME,      "ltNICKNAME" },
    { ltOFFER,         "ltOFFER" },
    { ltCONTRACT,      "ltCONTRACT" },
    { ltLEDGER_HASHES, "ltLEDGER_HASHES" },
    { ltAMENDMENTS,    "ltAMENDMENTS" },
    { ltFEE_SETTINGS,  "ltFEE_SETTINGS" }
};

/**
 * Emit lsTRACE-level logging of a count of the objects in a
 * ledger's account-state SHAMap, grouped by LedgerEntryType.
 */
static void
traceLedgerContents(Ledger& ledger)
{
    if (ShouldLog (lsTRACE, LedgerDump))
    {
        std::map<LedgerEntryType, uint> entryTypeCounts;
        ledger.peekAccountStateMap ()->visitLeaves (
            [&](SHAMapItem::ref item)
            {
                uint256 idx = item->getTag ();
                Serializer ser = item->peekSerializer ();
                SerializedLedgerEntry sle (ser, idx);
                entryTypeCounts[sle.getType ()]++;
            });
        WriteLog (lsTRACE, LedgerDump) << "Entry-type counts in account state map:";
        for (auto const& etc : entryTypeCounts)
        {
            WriteLog (lsTRACE, LedgerDump) << entryTypeNames[etc.first]
                                      << ": " << etc.second;
        }
    }
}

/**
 * Throw an error if the fields of two ledgers aren't equal.
 */
static void
checkLedgersEqual(Ledger & want, Ledger & have)
{

#define CHK(field) \
    requireEqual (#field, want.get##field(), have.get##field());
    CHK(LedgerSeq);
    CHK(TotalCoins);
    CHK(FeePool);
    CHK(InflationSeq);
    CHK(ParentCloseTimeNC);
    CHK(CloseTime);
    CHK(CloseResolution);
    CHK(CloseFlags);
    CHK(ParentHash);
    CHK(TransHash);
    CHK(AccountHash);
#undef CHK

    requireEqual ("accountStateMapHash",
                  want.getAccountHash (),
                  have.peekAccountStateMap ()->getHash ());
}


static Json::Value
loadJsonRecord (std::istream& in)
{
    std::string buf;
    std::getline (in, buf, DUMP_DELIM);
    Json::Value j;
    Json::Reader r;
    require (r.parse (buf, j), "Error parsing json");
    return j;
}

/**
 * Instantiate an application and replay a ledger history out
 * of the dump file `filename`.
 */
void
LedgerDump::loadTransactions (std::string const& filename)
{
    std::ifstream in (filename);
    require (in, "opening file");

    std::unique_ptr <Application> app (make_Application ());
    app->setup ();
    auto &lm = app->getLedgerMaster ();
    WriteLog (lsINFO, LedgerDump) << "Loading ledgers from " << filename;

    auto nTxs = 0;

    // app->setup() when called with START_UP == Config::FRESH calls
    // ApplicationImp::startNewLedger(). Unfortunately it starts the new
    // ledger at the wrong timestamp, so we call it again once we've read
    // the first ledger we're going to apply. However, it's worth
    // understanding that startNewLedger() establishes 3 ledgers:
    //
    // Ledger 0 is the genesis ledger, it's not a real ledger, just a
    //          number.
    //
    // Ledger 1 is the root-account deposit ledger, with a single pile of
    //          currency owned by a single account generated by the seed
    //          "masterpassword".
    //
    // Ledger 2 is created and closed immediately on start, not sure why.
    //
    // Ledger 3 is a new ledger chained to #2 and left open in
    //          ledgermaster.
    //
    // Ledger 3 is where replay should be starting, so (once we call
    // startNewLedger() again) we pull out ledger #2 and use it as the
    // parent of the new ledger 3 we're replaying, and throw away the #3
    // that was made by startNewLedger().

    Ledger::pointer parentLedger;

    while (in)
    {
        if ((gLedgerSeq & 0xfff) == 0) {
            Job j;
            app->doSweep (j);
        }

        Json::Value j = loadJsonRecord (in);

        Ledger::pointer deserializedLedger;
        SHAMap::pointer txSet;
        std::vector<uint256> txOrder;
        std::tie (deserializedLedger, txSet, txOrder) =
            loadLedgerAndTransactionsFromJSON (*app, j);

        if (!parentLedger)
        {
            if (getConfig ().START_LEDGER.empty ())
            {
                require (deserializedLedger->getLedgerSeq () == 3,
                         "Initial ledger isn't seq #3");

                // On first iteration, restart the app's view of the ledger
                // history at the same instant as the parent close time of the
                // first ledger (ledger #3).
                app->startNewLedger (deserializedLedger->getParentCloseTimeNC ());
                parentLedger = lm.getClosedLedger ();
                require (parentLedger->getLedgerSeq () == 2,
                         "Initial ledger parent isn't seq #2");
            }
            else
            {
                // We're being invoked with --ledger, which is where we
                // will start replay from.
                require (app->loadOldLedger (getConfig ().START_LEDGER, false),
                         "Reloading old ledger failed.");
                parentLedger = lm.getClosedLedger ();
            }

            auto const parentSeq = parentLedger->getLedgerSeq ();
            auto seq = j["seq"].asUInt ();
            while (parentSeq + 1 > seq)
            {
                // Fast-scan JSON records until we hit the right one.
                WriteLog (lsINFO, LedgerDump) << "scanning past ledger: "
                                              << seq;
                j = loadJsonRecord (in);
                seq = j["seq"].asUInt ();
                if (parentSeq + 1 <= seq)
                {
                    require (parentSeq + 1 == seq,
                             "Missing ledgers between loaded and replay-start");
                    std::tie (deserializedLedger, txSet, txOrder) =
                        loadLedgerAndTransactionsFromJSON (*app, j);
                }
            }
            gLedgerSeq = parentSeq;
            require(parentLedger->getLedgerSeq () + 1 ==
                    deserializedLedger->getLedgerSeq (),
                    "Mismatched ledger-sequence prefix.");
        }

        Ledger::pointer currentLedger =
            boost::make_shared<Ledger> (true, *parentLedger);
        currentLedger->setCloseTime (deserializedLedger->getCloseTimeNC ());
        currentLedger->setCloseFlags (deserializedLedger->getCloseFlags ());
        currentLedger->setParentHash (deserializedLedger->getParentHash ());

        WriteLog (lsINFO, LedgerDump) << "loading ledger: "
                                      << currentLedger->getLedgerSeq();

        if (ShouldLog (lsTRACE, LedgerDump))
        {
            WriteLog (lsTRACE, LedgerDump) << "expecting next ledger:";
            WriteLog (lsTRACE, LedgerDump) << deserializedLedger->getJson (0);
            WriteLog (lsTRACE, LedgerDump) << "synthetic next ledger:";
            WriteLog (lsTRACE, LedgerDump) << currentLedger->getJson (0);
        }

        gLedgerSeq++;

        // Apply transactions, transitioning from one ledger state to next.
        WriteLog (lsDEBUG, LedgerDump)
            << "Applying set of " << txOrder.size() << " transactions";
        CanonicalTXSet failedTransactions (txSet->getHash ());
        LedgerConsensus::applyTransactions (txSet, currentLedger, currentLedger,
                                            failedTransactions, false, txOrder);
        require (failedTransactions.empty (), "failed tx set is not empty");
        currentLedger->updateSkipList ();
        currentLedger->setClosed ();
        currentLedger->setCloseTime (deserializedLedger->getCloseTimeNC ());

        int asf = currentLedger->peekAccountStateMap ()->flushDirty (
            hotACCOUNT_NODE, currentLedger->getLedgerSeq());
        int tmf = currentLedger->peekTransactionMap ()->flushDirty (
            hotTRANSACTION_NODE, currentLedger->getLedgerSeq());
        nTxs += tmf;

        WriteLog (lsDEBUG, LedgerDump) << "Flushed " << asf << " account "
                                  << "and " << tmf << "transaction nodes";

        // Finalize with the LedgerMaster.
        currentLedger->setAccepted ();
        bool alreadyHadLedger = lm.storeLedger (currentLedger);
        assert (! alreadyHadLedger);
        lm.pushLedger (currentLedger);

        WriteLog (lsTRACE, LedgerDump) << "parent ledger:";
        traceLedgerContents (*parentLedger);
        WriteLog (lsTRACE, LedgerDump) << "current ledger:";
        traceLedgerContents (*currentLedger);

        try
        {
            checkLedgersEqual (*deserializedLedger, *currentLedger);
        }
        catch (...)
        {
            WriteLog (lsINFO, LedgerDump) << "bad ledger:";
            std::cerr << currentLedger->getJson (LEDGER_JSON_FULL);
            throw;
        }
        parentLedger = currentLedger;
    }

    WriteLog (lsINFO, LedgerDump) << "Loaded "
                             << gLedgerSeq << "ledgers, "
                             << nTxs << " transactions from "
                             << filename;
    exit (0);
}

} // ripple
