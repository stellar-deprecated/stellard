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

/*
 This code prevents scenarios like the following:
1) A client submits a transaction.
2) The transaction gets into the ledger this server
   believes will be the consensus ledger.
3) The server builds a succeeding open ledger without the
   transaction (because it's in the prior ledger).
4) The local consensus ledger is not the majority ledger
   (due to network conditions, Byzantine fault, etcetera)
   the majority ledger does not include the transaction.
5) The server builds a new open ledger that does not include
   the transaction or have it in a prior ledger.
6) The client submits another transaction and gets a terPRE_SEQ
   preliminary result.
7) The server does not relay that second transaction, at least
   not yet.

With this code, when step 5 happens, the first transaction will
be applied to that open ledger so the second transaction will
succeed normally at step 6. Transactions remain tracked and
test-applied to all new open ledgers until seen in a fully-
validated ledger
*/

#include <map>
#include <vector>

namespace ripple {

// This class wraps a pointer to a transaction along with
// its expiration ledger. It also caches the issuing account.
class LocalTx
{
public:

    // The number of ledgers to hold a transaction is essentially
    // arbitrary. It should be sufficient to allow the transaction to
    // get into a fully-validated ledger.
    static int const holdLedgers = 5;

    LocalTx (LedgerIndex index, SerializedTransaction::ref txn)
        : m_txn (txn)
        , m_expire (index + holdLedgers)
        , m_id (txn->getTransactionID ())
        , m_account (txn->getSourceAccount ())
        , m_seq (txn->getSequence())
        , m_failed(false)
        , m_lastStatus(tesPENDING)
    {
        if (txn->isFieldPresent (sfLastLedgerSequence))
        {
           LedgerIndex m_txnexpire = txn->getFieldU32 (sfLastLedgerSequence) + 1;
           m_expire = std::min (m_expire, m_txnexpire);
        }
    }

    uint256 const& getID ()
    {
        return m_id;
    }

    std::uint32_t getSeq ()
    {
        return m_seq;
    }

    bool isExpired (LedgerIndex i)
    {
        return i > m_expire;
    }

    SerializedTransaction::ref getTX ()
    {
        return m_txn;
    }

    RippleAddress const& getAccount ()
    {
        return m_account;
    }

    void setFailed() { m_failed = true; }
    bool isFailed() { return m_failed; }

    void setLastStatus(TER s) { m_lastStatus = s; }
    TER getLastStatus() { return m_lastStatus; }

private:

    SerializedTransaction::pointer m_txn;
    LedgerIndex                    m_expire;
    uint256                        m_id;
    RippleAddress                  m_account;
    std::uint32_t                  m_seq;
    bool                           m_failed;
    TER                            m_lastStatus;
};

class LocalTxsImp : public LocalTxs
{
public:

    // when out of sync, use the local current ledger as an approximation for the ledgerindex
    // but shift it even more in the (non probable) case that the current non validated closed faster than
    // the validated one
    static int const holdLedgerNonValidatedGap = 10;

    LocalTxsImp()
    { }

    // Add a new transaction to the set of local transactions
    void push_back (LedgerIndex index, SerializedTransaction::ref txn) override
    {
        std::lock_guard <std::mutex> lock (m_lock);

        LocalTx tx(index, txn);
        m_txns.insert(make_pair(tx.getID(), tx)); // will not insert if there is already an entry with the same hash
    }

    bool can_remove (LocalTx& txn, Ledger::pointer ledger, LedgerIndex currentLedgerIndex)
    {
        if (txn.isExpired(currentLedgerIndex))
            return true;

        if (txn.isExpired (ledger->getLedgerSeq ()))
            return true;

        try {
            if (ledger->hasTransaction (txn.getID ()))
                return true;
        }
        catch (SHAMapMissingNode) { // can't tell at this time, safer to keep the tx around
            return false;
        }

        SLE::pointer sle;
        try {
            sle = ledger->getAccountRoot(txn.getAccount());
        }
        catch (SHAMapMissingNode) {
            // can't tell if we can't find the account
        }
        if (!sle)
            return false;

        if (sle->getFieldU32 (sfSequence) > txn.getSeq ())
            return true;


        return false;
    }

    void apply (TransactionEngine& engine) override
    {

        CanonicalTXSet tset (uint256 {});

        doSweep(engine.getLedger()->getLedgerSeq()); // housekeeping

        // Get the set of local transactions as a canonical
        // set (so they apply in a valid order)
        {
            std::lock_guard <std::mutex> lock (m_lock);

            for (auto& it : m_txns)
            {
                if (!it.second.isFailed())
                    tset.push_back (it.second.getTX());
            }
        }

        for(auto &it: tset)
        {
            TER tx_res = temUNCERTAIN;
            bool didApply = false;
            try
            {
                TransactionEngineParams parms = tapOPEN_LEDGER;
                tx_res = engine.applyTransaction (*it.second, parms, didApply);
            }
            catch (...)
            {
                // Nothing special we need to do.
                // It's possible a cleverly malformed transaction or
                // corrupt back end database could cause an exception
                // during transaction processing.
            }

            LocalTx &tx = m_txns.at(it.first.getTXID());
            tx.setLastStatus(tx_res);

            if (!didApply &&
                (isTelLocal(tx_res) || isTemMalformed(tx_res) || isTefFailure(tx_res))
                )
            {
                // mark this transaction as failed, will not attempt to apply it in the future
                // but keep it in the local tx set as a way to temporary ignore any other attempts to apply
                // this transaction
                tx.setFailed();
            }
        }
    }

    void sweep (Ledger::ref validLedger) override
    {
        std::lock_guard <std::mutex> lock (m_lock);
        mSweepLedgers.push_back(validLedger);
    }

    // Remove transactions that have either been accepted into a fully-validated
    // ledger, are (now) impossible, or have expired
    void doSweep (LedgerIndex currentLedgerIndex)
    {
        // adjust currentLedgerIndex to be threshold for expiration
        if (currentLedgerIndex > holdLedgerNonValidatedGap)
        {
            currentLedgerIndex -= holdLedgerNonValidatedGap;
        }
        else
        {
            currentLedgerIndex = 0;
        }

        std::lock_guard <std::mutex> lock (m_lock);
        for (auto ledger = mSweepLedgers.begin(); ledger != mSweepLedgers.end(); ledger++)
        {
            for (auto it = m_txns.begin (); it != m_txns.end (); )
            {
                if (can_remove (it->second, *ledger, currentLedgerIndex))
                {
                    // this section puts in the tx caches the result of the transaction
                    // so that calls such as "tx" return something useful even if there was no operation on the ledger
                    Transaction::pointer trans;
                    // if we did not commit this tx in the db, force update the caches
                    trans = getApp().getMasterTransaction().fetch(it->first, true);
                    if (!(trans && trans->getLedger() != 0))
                    {
                        trans = boost::make_shared<Transaction>(it->second.getTX(), false);
                        TER r = it->second.getLastStatus();
                        if (r == tesPENDING || r == tesSUCCESS)
                            trans->setResult(telFAILED_PROCESSING);
                        else
                            trans->setResult(r);
                        getApp().getMasterTransaction ().canonicalize (&trans);
                    }
                    it = m_txns.erase (it);
                }
                else
                    ++it;
            }
        }
        mSweepLedgers.clear();
    }

    std::size_t size () override
    {
        std::lock_guard <std::mutex> lock (m_lock);

        return m_txns.size ();
    }

    bool contains(uint256 transactionId) override
    {
        std::lock_guard <std::mutex> lock (m_lock);
        return m_txns.find(transactionId) != m_txns.end();
    }

private:

    vector<Ledger::pointer> mSweepLedgers;
    std::mutex m_lock;
    std::map<uint256, LocalTx> m_txns;
};

std::unique_ptr <LocalTxs> LocalTxs::New()
{
    return std::make_unique <LocalTxsImp> ();
}

} // ripple
