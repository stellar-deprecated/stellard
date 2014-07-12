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

#include "../../beast/beast/unit_test/suite.h"
#include <sodium.h>

namespace ripple {

SETUP_LOG (RippleAddress)

RippleAddress::RippleAddress ()
    : mIsValid (false)
{
    nVersion = VER_NONE;
}

void RippleAddress::clear ()
{
    nVersion = VER_NONE;
    vchData.clear ();
    mIsValid = false;
}

bool RippleAddress::isSet () const
{
    return nVersion != VER_NONE;
}

std::string RippleAddress::humanAddressType () const
{
    switch (nVersion)
    {
    case VER_NONE:
        return "VER_NONE";

    case VER_NODE_PUBLIC:
        return "VER_NODE_PUBLIC";

    case VER_NODE_PRIVATE:
        return "VER_NODE_PRIVATE";

    case VER_ACCOUNT_ID:
        return "VER_ACCOUNT_ID";

    case VER_ACCOUNT_PUBLIC:
        return "VER_ACCOUNT_PUBLIC";

    case VER_ACCOUNT_PRIVATE:
        return "VER_ACCOUNT_PRIVATE";


	case VER_SEED:
        return "VER_SEED";
    }

    return "unknown";
}

//
// NodePublic
//

RippleAddress RippleAddress::createNodePublic (const RippleAddress& naSeed)
{
    EdKeyPair ckSeed (naSeed.getSeed ());
    RippleAddress   naNew;

    // YYY Should there be a GetPubKey() equiv that returns a uint256?
    naNew.setNodePublic (ckSeed.GetPubKey ());

    return naNew;
}

RippleAddress RippleAddress::createNodePublic (Blob const& vPublic)
{
    RippleAddress   naNew;

    naNew.setNodePublic (vPublic);

    return naNew;
}

RippleAddress RippleAddress::createNodePublic (const std::string& strPublic)
{
    RippleAddress   naNew;

    naNew.setNodePublic (strPublic);

    return naNew;
}

uint160 RippleAddress::getNodeID () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getNodeID");

    case VER_NODE_PUBLIC:
        // Note, we are encoding the left.
        return Hash160 (vchData);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}
Blob const& RippleAddress::getNodePublic () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getNodePublic");

    case VER_NODE_PUBLIC:
        return vchData;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanNodePublic () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanNodePublic");

    case VER_NODE_PUBLIC:
        return ToString ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

bool RippleAddress::setNodePublic (const std::string& strPublic)
{
    mIsValid = SetString (strPublic, VER_NODE_PUBLIC, Base58::getRippleAlphabet ());

    return mIsValid;
}

void RippleAddress::setNodePublic (Blob const& vPublic)
{
    mIsValid        = true;

    SetData (VER_NODE_PUBLIC, vPublic);
}

bool RippleAddress::verifyNodePublic(uint256 const& hash, Blob const& vchSig, ECDSA) const
{
	assert(nVersion == VER_NODE_PUBLIC); // TODO: this nVersion stuff is BS
	return(verifySignature(hash, vchSig));
}

bool RippleAddress::verifySignature(uint256 const& hash, Blob const& vchSig) const
{
	if (vchData.size() != crypto_sign_PUBLICKEYBYTES
        || vchSig.size () != crypto_sign_BYTES)
      return false;

    unsigned char signed_buf[crypto_sign_BYTES + hash.bytes];
    memcpy (signed_buf, vchSig.data(), crypto_sign_BYTES);
    memcpy (signed_buf+crypto_sign_BYTES, hash.data(), hash.bytes);

    unsigned char ignored_buf[hash.bytes];
    unsigned long long ignored_len;
    return crypto_sign_open (ignored_buf, &ignored_len,
			     signed_buf, sizeof (signed_buf),
				 vchData.data()) == 0;
}

bool RippleAddress::verifyNodePublic (uint256 const& hash, const std::string& strSig, ECDSA fullyCanonical) const
{
    Blob vchSig (strSig.begin (), strSig.end ());

    return verifyNodePublic (hash, vchSig, fullyCanonical);
}

//
// NodePrivate
//

RippleAddress RippleAddress::createNodePrivate (const RippleAddress& naSeed)
{
    uint256         uPrivKey;
    RippleAddress   naNew;
    EdKeyPair       ckSeed (naSeed.getSeed ());

    ckSeed.GetPrivateKeyU (uPrivKey);

    naNew.setNodePrivate (uPrivKey);

    return naNew;
}

Blob const& RippleAddress::getNodePrivateData () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getNodePrivateData");

    case VER_NODE_PRIVATE:
        return vchData;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

uint256 RippleAddress::getNodePrivate () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source = getNodePrivate");

    case VER_NODE_PRIVATE:
        return uint256 (vchData);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanNodePrivate () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanNodePrivate");

    case VER_NODE_PRIVATE:
        return ToString ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

bool RippleAddress::setNodePrivate (const std::string& strPrivate)
{
    mIsValid = SetString (strPrivate, VER_NODE_PRIVATE, Base58::getRippleAlphabet ());

    return mIsValid;
}

void RippleAddress::setNodePrivate (Blob const& vPrivate)
{
    mIsValid = true;

    SetData (VER_NODE_PRIVATE, vPrivate);
}

void RippleAddress::setNodePrivate (uint256 hash256)
{
    mIsValid = true;

    SetData (VER_NODE_PRIVATE, hash256);
}

void RippleAddress::signNodePrivate (uint256 const& hash, Blob& vchSig) const
{
    unsigned char out[crypto_sign_BYTES + hash.bytes];
    unsigned long long len;
    const unsigned char *key = vchData.data();

    crypto_sign(out, &len, 
    		(unsigned char*) hash.begin (), hash.size (),
		key);

    vchSig.resize(crypto_sign_BYTES);
    memcpy (&vchSig[0], out, crypto_sign_BYTES);
}

//
// AccountID
//

uint160 RippleAddress::getAccountID () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getAccountID");

    case VER_ACCOUNT_ID:
        return uint160 (vchData);

    case VER_ACCOUNT_PUBLIC:
        // Note, we are encoding the left.
        return Hash160 (vchData);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

typedef RippleMutex StaticLockType;
typedef std::lock_guard <StaticLockType> StaticScopedLockType;
static StaticLockType s_lock;

static ripple::unordered_map< Blob , std::string > rncMap;

std::string RippleAddress::humanAccountID () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanAccountID");

    case VER_ACCOUNT_ID:
    {
        StaticScopedLockType sl (s_lock);

        auto it = rncMap.find (vchData);

        if (it != rncMap.end ())
            return it->second;

        // VFALCO NOTE Why do we throw everything out? We could keep two maps
        //             here, switch back and forth keep one of them full and clear the
        //             other on a swap - but always check both maps for cache hits.
        //
        if (rncMap.size () > 250000)
            rncMap.clear ();

        return rncMap[vchData] = ToString ();
    }

    case VER_ACCOUNT_PUBLIC:
    {
        RippleAddress   accountID;

        (void) accountID.setAccountID (getAccountID ());

        return accountID.ToString ();
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

bool RippleAddress::setAccountID (const std::string& strAccountID, Base58::Alphabet const& alphabet)
{
    if (strAccountID.empty ())
    {
        setAccountID (uint160 ());

        mIsValid    = true;
    }
    else
    {
        mIsValid = SetString (strAccountID, VER_ACCOUNT_ID, alphabet);
    }

    return mIsValid;
}

void RippleAddress::setAccountID (const uint160& hash160)
{
    mIsValid        = true;

    SetData (VER_ACCOUNT_ID, hash160);
}

//
// AccountPublic
//

RippleAddress RippleAddress::createAccountPublic (const RippleAddress& seed, int )
{
	EdKeyPair       ckPub(seed.getSeed());
    RippleAddress   naNew;

    naNew.setAccountPublic (ckPub.GetPubKey ());

    return naNew;
}

Blob const& RippleAddress::getAccountPublic () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getAccountPublic");

    case VER_ACCOUNT_ID:
        throw std::runtime_error ("public not available from account id");
        break;

    case VER_ACCOUNT_PUBLIC:
        return vchData;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanAccountPublic () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanAccountPublic");

    case VER_ACCOUNT_ID:
        throw std::runtime_error ("public not available from account id");

    case VER_ACCOUNT_PUBLIC:
        return ToString ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

bool RippleAddress::setAccountPublic (const std::string& strPublic)
{
    mIsValid = SetString (strPublic, VER_ACCOUNT_PUBLIC, Base58::getRippleAlphabet ());

    return mIsValid;
}

void RippleAddress::setAccountPublic (Blob const& vPublic)
{
    mIsValid = true;

    SetData (VER_ACCOUNT_PUBLIC, vPublic);
}

void RippleAddress::setAccountPublic (const RippleAddress& generator, int )
{
	EdKeyPair pubkey(generator.getSeed());

    setAccountPublic (pubkey.GetPubKey ());
}

bool RippleAddress::accountPublicVerify (uint256 const& uHash, Blob const& vucSig, ECDSA) const
{
	assert(nVersion == VER_ACCOUNT_PUBLIC);  // TODO: get rid of this nVersion BS
	return(verifySignature(uHash, vucSig));
}

RippleAddress RippleAddress::createAccountID (const uint160& uiAccountID)
{
    RippleAddress   na;

    na.setAccountID (uiAccountID);

    return na;
}

//
// AccountPrivate
//

RippleAddress RippleAddress::createAccountPrivate (const RippleAddress& naSeed)
{
    RippleAddress   naNew;

    naNew.setAccountPrivate (naSeed.getSeed());

    return naNew;
}

uint256 RippleAddress::getAccountPrivate () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getAccountPrivate");

    case VER_ACCOUNT_PRIVATE:
        return uint256 (vchData);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanAccountPrivate () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanAccountPrivate");

    case VER_ACCOUNT_PRIVATE:
        return ToString ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

bool RippleAddress::setAccountPrivate (const std::string& strPrivate)
{
    mIsValid = SetString (strPrivate, VER_ACCOUNT_PRIVATE, Base58::getRippleAlphabet ());

    return mIsValid;
}

void RippleAddress::setAccountPrivate (Blob const& vPrivate)
{
    mIsValid        = true;

    SetData (VER_ACCOUNT_PRIVATE, vPrivate);
}

void RippleAddress::setAccountPrivate (uint256 hash256)
{
    mIsValid = true;

    SetData (VER_ACCOUNT_PRIVATE, hash256);
}

void RippleAddress::setAccountPrivate (const RippleAddress& naGenerator, const RippleAddress& naSeed, int seq)
{
	assert(0);
	/*
    CKey    ckPubkey    = CKey (naSeed.getSeed ());
    CKey    ckPrivkey   = CKey (naGenerator, ckPubkey.GetSecretBN (), seq);
    uint256 uPrivKey;

    ckPrivkey.GetPrivateKeyU (uPrivKey);

    setAccountPrivate (uPrivKey);

	*/
}

bool RippleAddress::accountPrivateSign (uint256 const& uHash, Blob& vucSig) const
{
    signNodePrivate (uHash, vucSig);

    return true; // XXX signing never fails
}



Blob RippleAddress::accountPrivateDecrypt (const RippleAddress& naPublicFrom, Blob const& vucCipherText) const
{

    Blob    vucPlainText;
	assert(0);

	/* TEMP 
    if (!ckPublic.SetPubKey (naPublicFrom.getAccountPublic ()))
    {
        // Bad public key.
        WriteLog (lsWARNING, RippleAddress) << "accountPrivateDecrypt: Bad public key.";
    }
    else if (!ckPrivate.SetPrivateKeyU (getAccountPrivate ()))
    {
        // Bad private key.
        WriteLog (lsWARNING, RippleAddress) << "accountPrivateDecrypt: Bad private key.";
    }
    else
    {
        try
        {
            vucPlainText = ckPrivate.decryptECIES (ckPublic, vucCipherText);
        }
        catch (...)
        {
            nothing ();
        }
    }
	*/

    return vucPlainText;
}



//
// Seed
//

uint256 RippleAddress::getSeed() const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - getSeed");

	case VER_SEED:
		return uint256(vchData);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanSeed1751 () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanSeed1751");

	case VER_SEED:
    {
        std::string strHuman;
        std::string strLittle;
        std::string strBig;
		uint256 uSeed = getSeed();

        strLittle.assign (uSeed.begin (), uSeed.end ());

        strBig.assign (strLittle.rbegin (), strLittle.rend ());

        RFC1751::getEnglishFromKey (strHuman, strBig);

        return strHuman;
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

std::string RippleAddress::humanSeed () const
{
    switch (nVersion)
    {
    case VER_NONE:
        throw std::runtime_error ("unset source - humanSeed");

	case VER_SEED:
        return ToString ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nVersion)));
    }
}

int RippleAddress::setSeed1751 (const std::string& strHuman1751)
{
    std::string strKey;
    int         iResult = RFC1751::getKeyFromEnglish (strKey, strHuman1751);

    if (1 == iResult)
    {
        Blob    vchLittle (strKey.rbegin (), strKey.rend ());
		uint256     uSeed(vchLittle);

        setSeed (uSeed);
    }

    return iResult;
}

bool RippleAddress::setSeed (const std::string& strSeed)
{
	mIsValid = SetString(strSeed, VER_SEED, Base58::getRippleAlphabet());

    return mIsValid;
}

bool RippleAddress::setSeedGeneric (const std::string& strText)
{
    RippleAddress   naTemp;
    bool            bResult = true;
	uint256         uSeed;

    if (strText.empty ()
            || naTemp.setAccountID (strText)
            || naTemp.setAccountPublic (strText)
            || naTemp.setAccountPrivate (strText)
            || naTemp.setNodePublic (strText)
            || naTemp.setNodePrivate (strText))
    {
        bResult = false;
    }
    else if (strText.length () == 32 && uSeed.SetHex (strText, true))
    {
        setSeed (uSeed);
    }
    else if (setSeed (strText))
    {
        // Log::out() << "Recognized seed.";
        nothing ();
    }
    else if (1 == setSeed1751 (strText))
    {
        // Log::out() << "Recognized 1751 seed.";
        nothing ();
    }
    else
    {
        // Log::out() << "Creating seed from pass phrase.";
        setSeed (EdKeyPair::PassPhraseToKey (strText));
    }

    return bResult;
}

void RippleAddress::setSeed(uint256 seed)
{
    mIsValid = true;

	SetData(VER_SEED, seed);
}

void RippleAddress::setSeedRandom ()
{
    // XXX Maybe we should call MakeNewKey
	uint256 key;

    RandomNumbers::getInstance ().fillBytes (key.begin (), key.size ());

    RippleAddress::setSeed (key);
}

RippleAddress RippleAddress::createSeedRandom ()
{
    RippleAddress   naNew;

    naNew.setSeedRandom ();

    return naNew;
}

RippleAddress RippleAddress::createSeedGeneric (const std::string& strText)
{
    RippleAddress   naNew;

    naNew.setSeedGeneric (strText);

    return naNew;
}

//------------------------------------------------------------------------------

class RippleAddress_test : public beast::unit_test::suite
{
	void testBase58(RippleAddress::VersionEncoding type,char first)
	{
		Blob vchData;
		vchData.resize(32);
		for (int i = 0; i < 32; i++) vchData[i] = 60;
		Blob vch(1, type);
		vch.insert(vch.end(), vchData.begin(), vchData.end());
		std::string human = Base58::encodeWithCheck(vch);
		Log::out() << type << " :  " << human;
		expect(human[0] == first, human);
	}

public:
    void run()
    {
        // Construct a seed.
        RippleAddress naSeed;
		
		// gsphnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCr65jkm8oFqi1tuvAxyz

		testBase58(RippleAddress::VER_NODE_PUBLIC,'n');
		testBase58(RippleAddress::VER_NODE_PRIVATE, 'v');
		testBase58(RippleAddress::VER_ACCOUNT_PUBLIC, 'p');
		testBase58(RippleAddress::VER_ACCOUNT_PRIVATE, 'h');
		testBase58(RippleAddress::VER_SEED,'s');


        expect (naSeed.setSeedGeneric ("masterpassphrase"));
        expect (naSeed.humanSeed () == "snoPBgXtMeMyMHUVTrbuqAfr1SUTb", naSeed.humanSeed ());

        // Create node public/private key pair
        RippleAddress naNodePublic    = RippleAddress::createNodePublic (naSeed);
        RippleAddress naNodePrivate   = RippleAddress::createNodePrivate (naSeed);

        expect (naNodePublic.humanNodePublic () == "n94a1u4jAz288pZLtw6yFWVbi89YamiC6JBXPVUj5zmExe5fTVr9", naNodePublic.humanNodePublic ());
        expect (naNodePrivate.humanNodePrivate () == "pnen77YEeUd4fFKG7iycBWcwKpTaeFRkW2WFostaATy1DSupwXe", naNodePrivate.humanNodePrivate ());

        // Check node signing.
        Blob vucTextSrc = strCopy ("Hello, nurse!");
        uint256 uHash   = Serializer::getSHA512Half (vucTextSrc);
        Blob vucTextSig;

        naNodePrivate.signNodePrivate (uHash, vucTextSig);
        expect (naNodePublic.verifyNodePublic (uHash, vucTextSig, ECDSA::strict), "Verify failed.");

   
     
 
       

       
    }
};

//------------------------------------------------------------------------------

class RippleIdentifier_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        testcase ("Seed");
        RippleAddress seed;
        expect (seed.setSeedGeneric ("masterpassphrase"));
        expect (seed.humanSeed () == "snoPBgXtMeMyMHUVTrbuqAfr1SUTb", seed.humanSeed ());

        testcase ("RipplePublicKey");
        RippleAddress deprecatedPublicKey (RippleAddress::createNodePublic (seed));
        expect (deprecatedPublicKey.humanNodePublic () ==
            "n94a1u4jAz288pZLtw6yFWVbi89YamiC6JBXPVUj5zmExe5fTVr9",
                deprecatedPublicKey.humanNodePublic ());
        RipplePublicKey publicKey (deprecatedPublicKey);
        expect (publicKey.to_string() == deprecatedPublicKey.humanNodePublic(),
            publicKey.to_string());

        testcase ("RipplePrivateKey");
        RippleAddress deprecatedPrivateKey (RippleAddress::createNodePrivate (seed));
        expect (deprecatedPrivateKey.humanNodePrivate () ==
            "pnen77YEeUd4fFKG7iycBWcwKpTaeFRkW2WFostaATy1DSupwXe",
                deprecatedPrivateKey.humanNodePrivate ());
        RipplePrivateKey privateKey (deprecatedPrivateKey);
        expect (privateKey.to_string() == deprecatedPrivateKey.humanNodePrivate(),
            privateKey.to_string());
    }
};

BEAST_DEFINE_TESTSUITE(RippleAddress,ripple_data,ripple);
BEAST_DEFINE_TESTSUITE(RippleIdentifier,ripple_data,ripple);

} // ripple
