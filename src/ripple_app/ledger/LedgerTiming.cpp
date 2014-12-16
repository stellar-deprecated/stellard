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

// VFALCO Should rename ContinuousLedgerTiming to LedgerTiming

struct LedgerTiming; // for Log

SETUP_LOG (LedgerTiming)

// NOTE: First and last times must be repeated
int ContinuousLedgerTiming::LedgerTimeResolution[] = { 10, 10, 20, 30, 60, 90, 120, 120 };

extern bool gFORCE_CLOSE;
// Called when a ledger is open and no close is in progress -- when a transaction is received and no close
// is in process, or when a close completes. Returns the number of seconds the ledger should be be open.
bool ContinuousLedgerTiming::shouldClose (
    bool anyTransactions,
    int targetProposers,        // proposers we're targetting for this consensus round
    int proposersClosed,        // proposers who have currently closed this ledgers
    int sinceLastCloseMSeconds, // milliseconds since the previous ledger closed
    int currentMSeconds,        // milliseconds since consensus started
    int idleInterval)           // network's desired idle interval
{
	if(gFORCE_CLOSE)
	{
		WriteLog(lsWARNING, LedgerTiming) <<
			"CLC::shouldClose gFORCE_CLOSE ";

		return true;
	}

    if ((proposersClosed * 100) / targetProposers >= 75)
    {
        WriteLog (lsTRACE, LedgerTiming) << 
            "many proposers: now (" << proposersClosed <<
            " closed, " << targetProposers << " expected)";
        return true;
    }

    if (currentMSeconds <= LEDGER_MIN_CLOSE)
    {
        WriteLog (lsDEBUG, LedgerTiming) <<
            "Must wait minimum time before closing";
        return false;
    }

    if (!anyTransactions)
    {
        return sinceLastCloseMSeconds >= (idleInterval * 1000); // normal idle
    }

    return true; // this ledger should close now
}

// Returns whether we have a consensus or not. If so, we expect all honest nodes
// to already have everything they need to accept a consensus. Our vote is 'locked in'.
bool ContinuousLedgerTiming::haveConsensus (
    int targetProposers,        // target number of proposers
    int currentProposers,       // proposers in this closing so far (not including us)
    int currentAgree,           // proposers who agree with us
    int previousAgreeTime,      // how long it took to agree on the last ledger
    int currentAgreeTime,       // how long we've been trying to agree
    bool forReal,               // deciding whether to stop consensus process
    bool& failed)               // we can't reach a consensus
{
    WriteLog (lsDEBUG, LedgerTiming) <<
        "CLC::haveConsensus: prop=" << currentProposers <<
        "/" << targetProposers <<
        " agree=" << currentAgree <<
        " time=" << currentAgreeTime <<  "/" << previousAgreeTime <<
        (forReal ? "" : "X");

	if(gFORCE_CLOSE)
	{
		WriteLog(lsWARNING, LedgerTiming) <<
			"CLC::haveConsensus gFORCE_CLOSE ";

		gFORCE_CLOSE = false;
		return true;
	}

    if ( (currentProposers+1) < targetProposers)
    {
        WriteLog(lsDEBUG, LedgerConsensus) 
            << "Waiting for more proposers " << (currentProposers+1) << "/" << targetProposers;
        return false;
    }

    // If 80% of current proposers (plus us) agree on a set, we have consensus
    int inConsensus = ((currentAgree * 100 + 100) / (currentProposers + 1));
    if ( inConsensus >= 80 )
    {
        CondLog (forReal, lsINFO, LedgerTiming) << "normal consensus";
        failed = false;
        return true;
    }

    // no consensus yet
    CondLog (forReal, lsDEBUG, LedgerTiming) << "no consensus: agree = " << inConsensus << "%";
    return false;
}

int ContinuousLedgerTiming::getNextLedgerTimeResolution (int previousResolution, bool previousAgree, int ledgerSeq)
{
    assert (ledgerSeq);

    if ((!previousAgree) && ((ledgerSeq % LEDGER_RES_DECREASE) == 0))
    {
        // reduce resolution
        int i = 1;

        while (LedgerTimeResolution[i] != previousResolution)
            ++i;

        return LedgerTimeResolution[i + 1];
    }

    if ((previousAgree) && ((ledgerSeq % LEDGER_RES_INCREASE) == 0))
    {
        // increase resolution
        int i = 1;

        while (LedgerTimeResolution[i] != previousResolution)
            ++i;

        return LedgerTimeResolution[i - 1];
    }

    return previousResolution;
}

} // ripple
