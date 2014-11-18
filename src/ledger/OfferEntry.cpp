#include <boost/format.hpp>
#include "OfferEntry.h"
#include "ripple_app/data/DatabaseCon.h"
#include "ripple_app/main/Application.h"
#include "ripple_basics/log/Log.h"
#include "ripple_data/protocol/Serializer.h"
#include "ripple_app/ledger/Ledger.h"

using namespace std;

namespace stellar
{

    void OfferEntry::appendSQLInit(vector<const char*> &init)
    {
        init.push_back("CREATE TABLE IF NOT EXISTS Offers ( \
                        accountID           CHARACTER(35),  \
                        sequence            INT UNSIGNED,   \
                        takerPaysCurrency   CHARACTER(40),  \
                        takerPaysAmount CHARACTER(39),      \
                        takerPaysIssuer CHARACTER(35),      \
                        takerGetsCurrency CHARACTER(40),    \
                        takerGetsAmount CHARACTER(39),      \
                        takerGetsIssuer CHARACTER(35),      \
                        expiration INT UNSIGNED,            \
                        passive BOOL,                       \
                        PRIMARY KEY ( accountID, sequence ) \
                        );");
    }

    OfferEntry::OfferEntry(SLE::pointer sle)
    {
        mAccountID = sle->getFieldAccount160(sfAccount);
        mSequence = sle->getFieldU32(sfSequence);

        mTakerPays = sle->getFieldAmount(sfTakerPays);
        mTakerGets = sle->getFieldAmount(sfTakerGets);

        if (sle->isFieldPresent(sfExpiration))
            mExpiration = sle->getFieldU32(sfExpiration);
        else mExpiration = 0;

        uint32 flags = sle->getFlags();

        mPassive = flags & lsfPassive;
    }

    OfferEntry::OfferEntry(Database *db)
    {
        setFromCurrentRow(db);
    }

    void OfferEntry::calculateIndex()
    {
        Serializer  s(26);

        s.add16(spaceOffer);       //  2
        s.add160(mAccountID);      // 20
        s.add32(mSequence);        //  4

        mIndex = s.getSHA512Half();
    }

    void OfferEntry::insertIntoDB()
    {
        uint160 paysIssuer = mTakerPays.getIssuer();
        uint160 getsIssuer = mTakerGets.getIssuer();

        string sql = str(boost::format("INSERT OR REPLACE INTO Offers (accountID,sequence,"
            "takerPaysCurrency,takerPaysAmount,takerPaysIssuer,takerGetsCurrency,"
            "takerGetsAmount,takerGetsIssuer,expiration,passive)"
            "values ('%s',%d,'%s',%d,'%s','%s',%d,'%s',%d,%d);")
            % mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mSequence
            % mTakerPays.getHumanCurrency()
            % mTakerPays.getText()
            % paysIssuer.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mTakerGets.getHumanCurrency()
            % mTakerGets.getText()
            % getsIssuer.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mExpiration
            % mPassive);

        {
            DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
            Database* db = getApp().getWorkingLedgerDB()->getDB();

            if (!db->executeSQL(sql, false))
            {
                WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
            }
        }
    }

    void OfferEntry::updateInDB()
    {
        uint160 paysIssuer = mTakerPays.getIssuer();
        uint160 getsIssuer = mTakerGets.getIssuer();

        string sql = str(boost::format("UPDATE Offers set takerPaysCurrency='%s', "
            "takerPaysAmount=%d, takerPaysIssuer='%s', takerGetsCurrency='%s' ,takerGetsAmount=%d,"
            "takerGetsIssuer='%s' ,expiration=%d, passive=%d where accountID='%s' AND sequence=%d;")
            % mTakerPays.getHumanCurrency()
            % mTakerPays.getText()
            % paysIssuer.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mTakerGets.getHumanCurrency()
            % mTakerGets.getText()
            % getsIssuer.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mExpiration
            % mPassive
            % mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mSequence);

        {
            DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
            Database* db = getApp().getWorkingLedgerDB()->getDB();

            if (!db->executeSQL(sql, false))
            {
                WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
            }
        }
    }

    void OfferEntry::setFromCurrentRow(Database *db)
    {
        mAccountID = db->getAccountID("accountID");
        mSequence = db->getBigInt("sequence");

        std::string currency, amount, issuer;
        db->getStr("takerPaysCurrency", currency);
        db->getStr("takerPaysAmount", amount);
        db->getStr("takerPaysIssuer", issuer);
        mTakerPays.setFullValue(amount, currency, issuer);

        db->getStr("takerGetsCurrency", currency);
        db->getStr("takerGetsAmount", amount);
        db->getStr("takerGetsIssuer", issuer);
        mTakerGets.setFullValue(amount, currency, issuer);

        mExpiration = db->getInt("expiration");
        mPassive = db->getBool("passive");
    }

    void OfferEntry::deleteFromDB()
    {
        string sql = str(boost::format("DELETE FROM Offers where accountID='%s' AND sequence=%d;")
            % mAccountID.base58Encode(RippleAddress::VER_ACCOUNT_ID)
            % mSequence);

        {
            DeprecatedScopedLock sl(getApp().getWorkingLedgerDB()->getDBLock());
            Database* db = getApp().getWorkingLedgerDB()->getDB();

            if (!db->executeSQL(sql, false))
            {
                WriteLog(lsWARNING, ripple::Ledger) << "SQL failed: " << sql;
            }
        }
    }

    void OfferEntry::dropAll(LedgerDatabase &db)
    {
        if (!db.getDBCon()->getDB()->executeSQL("DROP TABLE IF EXISTS Offers;"))
        {
            throw std::runtime_error("Could not drop Offers data");
        }
    }
}