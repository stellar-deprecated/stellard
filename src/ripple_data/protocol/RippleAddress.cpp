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
#include "../ripple_data/crypto/StellarPrivateKey.h"
#include "../ripple_data/crypto/StellarPublicKey.h"

namespace ripple {

SETUP_LOG (RippleAddress)

RippleAddress::RippleAddress ()
    : mIsValid (false)
{
    nVersion = VER_NONE;
}

RippleAddress::RippleAddress(const RippleAddress& naSeed, VersionEncoding type)
{
	assert(0);
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
    naNew.setNodePublic (ckSeed.getPubKey ());

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

std::string RippleAddress::human(Blob& blob, VersionEncoding type)
{
	Blob vch(1, type);
	
	vch.insert(vch.end(), blob.begin(), blob.end());

	return Base58::encodeWithCheck(vch);
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


bool RippleAddress::verifySignature(uint256 const& hash, const std::string& strSig) const
{
	Blob vchSig(strSig.begin(), strSig.end());
	return(verifySignature(hash, vchSig));
}

bool RippleAddress::verifySignature(uint256 const& hash, Blob const& vchSig) const
{
    if (vchData.size() != crypto_sign_PUBLICKEYBYTES
        || vchSig.size () != crypto_sign_BYTES)
        throw std::runtime_error("bad inputs to verifySignature");

    bool verified = crypto_sign_verify_detached(vchSig.data(),
                 hash.data(), hash.bytes, vchData.data()) == 0;
    bool canonical = signatureIsCanonical (vchSig);
    return verified && canonical;
}

/**
 * This is a helper function that appeared briedly in libsodium:
 *
 *   https://github.com/jedisct1/libsodium/commit/4099618de2cce5099ac2ec5ce8f2d80f4585606e
 *
 * and was subsequently removed to maintain backward compatibility:
 *
 *   https://github.com/jedisct1/libsodium/commit/cb07df046f19ee0d5ad600c579df97aaa4295cc3
 *
 * It is used to check canonical representation of signatures; that is, to
 * prevent anyone from presenting multiple forms of valid signature, which
 * might permit a "transaction malleability" attack on certain gateways or
 * wallets that treat transactions as identified by their hash.
 *
 * At present (2014-10-22, v1.0.0) libsodium is still deciding on the
 * interface with which to expose this functionality (likely to add an
 * is_standard()/is_canonical() function) but has not done so yet, so for
 * the time being we copy-and-paste their code here. We contacted Frank
 * Denis (jedisct1) about this and this was his recommended course of
 * action.
 *
 * When libsodium presents such an interface, delete this code and
 * use their interface instead.
 */
static int
crypto_sign_check_S_lt_l(const unsigned char *S)
{
    static const unsigned char l[32] =
      { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
        0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };
    unsigned char c = 0;
    unsigned char n = 1;
    unsigned int  i = 32;

    do {
        i--;
        c |= ((S[i] - l[i]) >> 8) & n;
        n &= ((S[i] ^ l[i]) - 1) >> 8;
    } while (i != 0);

    return -(c == 0);
}

bool RippleAddress::signatureIsCanonical(Blob const& vchSig)
{
    return crypto_sign_check_S_lt_l(
        ((const unsigned char*) vchSig.data ()) + 32
        ) == 0;
}

void RippleAddress::sign(uint256 const& message, Blob& retSignature) const
{
	assert(vchData.size() == 64);

	const unsigned char *key = vchData.data();
	retSignature.resize(crypto_sign_BYTES);
	crypto_sign_detached(&retSignature[0], NULL,
		(unsigned char*)message.begin(), message.size(),
		key);
}


//
// NodePrivate
//



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

	if (mIsValid) mIsValid = (vchData.size() == crypto_sign_SECRETKEYBYTES);

    return mIsValid;
}

void RippleAddress::setNodePrivate (Blob const& vPrivate)
{
    mIsValid = true;
	

    SetData (VER_NODE_PRIVATE, vPrivate);
	assert(vchData.size() == crypto_sign_SECRETKEYBYTES);
}

void RippleAddress::setNodePrivate (uint256 hash256)
{
    mIsValid = true;

    SetData (VER_NODE_PRIVATE, hash256);
	assert(vchData.size() == crypto_sign_SECRETKEYBYTES);
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

RippleAddress RippleAddress::createAccountPublic (const RippleAddress& seed)
{
	EdKeyPair       ckPub(seed.getSeed());
    RippleAddress   naNew;

    naNew.setAccountPublic (ckPub.getPubKey ());

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


void RippleAddress::setAccountPublic (const RippleAddress& seed )
{
	EdKeyPair pubkey(seed.getSeed());

    setAccountPublic (pubkey.getPubKey ());
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
	EdKeyPair pair(naSeed.getSeed());

    naNew.setAccountPrivate (pair.mPrivateKey);

    return naNew;
}

RippleAddress RippleAddress::createNodePrivate(const RippleAddress& naSeed)
{
	RippleAddress   naNew;
	EdKeyPair pair(naSeed.getSeed());

	naNew.setNodePrivate(pair.mPrivateKey);

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
	if(mIsValid) mIsValid=(vchData.size() == crypto_sign_SECRETKEYBYTES);

    return mIsValid;
}

void RippleAddress::setAccountPrivate (Blob const& vPrivate)
{
    mIsValid        = true;

    SetData (VER_ACCOUNT_PRIVATE, vPrivate);
	assert(vchData.size() == crypto_sign_SECRETKEYBYTES);
}

void RippleAddress::setAccountPrivate (uint256 hash256)
{
    mIsValid = true;

    SetData (VER_ACCOUNT_PRIVATE, hash256);
	assert(vchData.size() == crypto_sign_SECRETKEYBYTES);
}
/*
void RippleAddress::setAccountPrivate (const RippleAddress& naGenerator, const RippleAddress& naSeed)
{
	assert(0);
	
    CKey    ckPubkey    = CKey (naSeed.getSeed ());
    CKey    ckPrivkey   = CKey (naGenerator, ckPubkey.GetSecretBN (), seq);
    uint256 uPrivKey;

    ckPrivkey.GetPrivateKeyU (uPrivKey);

    setAccountPrivate (uPrivKey);

	
}
*/




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
    else
    {
        // Log::out() << "Creating seed from pass phrase.";
        setSeed (EdKeyPair::passPhraseToKey (strText));
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
	void testBase58(int type,char first)
	{
		Blob vchData(32);
		for (int i = 0; i < 32; i++) vchData[i] = rand();
		//Log::out() << vchData.size();

		Blob vch(1, type);
		vch.insert(vch.end(), vchData.begin(), vchData.end());
		std::string human = Base58::encodeWithCheck(vch);
		//Log::out() << type << " :  " << human;
		expect(human[0] == first, human);
	}

    void add_l(unsigned char * const S)
    {
        static const unsigned char l[32] =
            { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
              0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10 };
        unsigned char c = 0U;
        unsigned int  i;
        unsigned int  s;

        for (i = 0U; i < 32U; i++) {
            s = S[i] + l[i] + c;
            S[i] = (unsigned char) s;
            c = (s >> 8) & 1;
        }
    }

public:
    void run()
    {
		testBase58(RippleAddress::VER_NODE_PUBLIC, 'n');
		testBase58(RippleAddress::VER_NODE_PRIVATE, 'h');
		testBase58(RippleAddress::VER_ACCOUNT_PUBLIC, 'p');
		testBase58(RippleAddress::VER_ACCOUNT_PRIVATE, 'h');
		testBase58(RippleAddress::VER_SEED, 's');

		// check pass phrase
		std::string strPass("masterpassphrase");
		std::string strBase58Seed("s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN");
		std::string strBase58NodePublic("nfbbWHgJqzqfH1cfRpMdPRkJ19cxTsdHkBtz1SLJJQfyf9Ax6vd");
		std::string strBase58AccountPublic("pGreoXKYybde1keKZwDCv8m5V1kT6JH37pgnTUVzdMkdygTixG8");
			
		AccountPrivateKey accountPrivateKey;
		NodePrivateKey nodePrivateKey;

		accountPrivateKey.fromPassPhrase(strPass);
		nodePrivateKey.fromPassPhrase(strPass);

		expect(accountPrivateKey.base58Seed() == "s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN", accountPrivateKey.base58Seed());
		expect(accountPrivateKey.base58AccountID() == "ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb", accountPrivateKey.base58AccountID());
		expect(accountPrivateKey.base58PublicKey() == strBase58AccountPublic, accountPrivateKey.base58PublicKey());
		expect(nodePrivateKey.base58PublicKey() == strBase58NodePublic, nodePrivateKey.base58PublicKey());


		Blob sig;
		uint256 message;
		accountPrivateKey.sign(message, sig);

		StellarPublicKey publicKey(accountPrivateKey.getPublicKey(), RippleAddress::VER_NODE_PUBLIC);
		expect(publicKey.verifySignature(message, sig), "Signature didn't verify");
		expect(publicKey.getAccountID() == accountPrivateKey.getAccountID(), "Account Id's mis match");
		expect(publicKey.base58AccountID() == accountPrivateKey.base58AccountID(), "Account Id's mis match");

        Blob nonCanonicalSig(sig);
        add_l(nonCanonicalSig.data() + 32);
        expect(sig != nonCanonicalSig, "Non-canonical signature equal to canonical signature");
        expect(crypto_sign_verify_detached(nonCanonicalSig.data(),
                                           message.data(), message.bytes,
                                           publicKey.vchData.data()) == 0,
               "Non-canonical signature didn't verify (ignoring canonical-ness)");
        expect(!publicKey.verifySignature(message, nonCanonicalSig), "Non-canonical signature verified");
		
		AccountPrivateKey privateKey2;
		privateKey2.fromString(strBase58Seed); // key from base58seed
		expect(privateKey2.base58Seed() == "s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN", privateKey2.base58Seed());
		expect(privateKey2.base58AccountID() == "ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb", privateKey2.base58AccountID());
		expect(privateKey2.base58PublicKey() == strBase58AccountPublic, privateKey2.base58PublicKey());
		privateKey2.sign(message, sig);
		expect(publicKey.verifySignature(message, sig), "Signature didn't verify"); // check with the previous pubkey

		// check random

		/// ======= OLD ====

		// Construct a seed.
		RippleAddress naSeed;

		expect(naSeed.setSeedGeneric("masterpassphrase"));
		expect(naSeed.humanSeed() == "s3q5ZGX2ScQK2rJ4JATp7rND6X5npG3De8jMbB7tuvm2HAVHcCN", naSeed.humanSeed());

		// Create node public/private key pair
		RippleAddress naNodePublic = RippleAddress::createNodePublic(naSeed);
		expect(naNodePublic.verifySignature(message, sig), "Signature didn't verify");
		expect(naNodePublic.humanNodePublic() == strBase58NodePublic, naNodePublic.humanNodePublic());

		naNodePublic.setNodePublic(strBase58NodePublic);
		expect(naNodePublic.verifySignature(message, sig), "Signature didn't verify");
		expect(naNodePublic.humanNodePublic() == strBase58NodePublic, naNodePublic.humanNodePublic());

		RippleAddress naAccountPublic = RippleAddress::createAccountPublic(naSeed);
		expect(naAccountPublic.humanAccountID() == "ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb", naAccountPublic.humanAccountID());
		expect(naAccountPublic.verifySignature(message, sig), "Signature didn't verify");
		expect(naAccountPublic.humanAccountPublic() == strBase58AccountPublic, naAccountPublic.humanAccountPublic());

		naAccountPublic.setAccountPublic(strBase58AccountPublic);
		expect(naAccountPublic.humanAccountID() == "ganVp9o5emfzpwrG5QVUXqMv8AgLcdvySb", naAccountPublic.humanAccountID());
		expect(naAccountPublic.verifySignature(message, sig), "Signature didn't verify");
		expect(naAccountPublic.humanAccountPublic() == strBase58AccountPublic, naAccountPublic.humanAccountPublic());

		Blob rippleSig;
		RippleAddress naAccountPrivate = RippleAddress::createAccountPrivate(naSeed);
		naAccountPrivate.sign(message, rippleSig);
		expect(rippleSig==sig, "Signature don't match");

		RippleAddress naNodePrivate = RippleAddress::createNodePrivate(naSeed);
		naNodePrivate.sign(message, rippleSig);
		expect(rippleSig == sig, "Signature don't match");


		std::string strPrivateKey("ssQMHypYAPSPgniSyvJQccuL1dJUbXJWVgAPV5QcAuBVEWsZTVQwffsnwTY6Mivoy3NRSVR28ZaCW74F67VSq4VRC4zY1XR");
		expect(naNodePrivate.humanNodePrivate() == strPrivateKey, naNodePrivate.humanNodePrivate());
		
		expect(naNodePrivate.setNodePrivate(strPrivateKey),"couldn't create private node");
		expect(naNodePrivate.humanNodePrivate() == strPrivateKey, naNodePrivate.humanNodePrivate());
		naNodePrivate.sign(message, rippleSig);
		expect(rippleSig == sig, "Signature don't match");




		/*
		RippleAddress naNodePrivate = RippleAddress::createNodePrivate(naSeed);
		expect(naNodePrivate.humanNodePrivate() == "pnen77YEeUd4fFKG7iycBWcwKpTaeFRkW2WFostaATy1DSupwXe", naNodePrivate.humanNodePrivate());

		// Check node signing.
		Blob vucTextSrc = strCopy("Hello, nurse!");
		uint256 uHash = Serializer::getSHA512Half(vucTextSrc);
		Blob vucTextSig;

		naNodePrivate.signNodePrivate(uHash, vucTextSig);
		expect(naNodePublic.verifyNodePublic(uHash, vucTextSig, ECDSA::strict), "Verify failed.");
		*/


       
       
    }
};

//------------------------------------------------------------------------------

class RippleIdentifier_test : public beast::unit_test::suite
{
public:
    void run ()
    {

    }
};

BEAST_DEFINE_TESTSUITE(RippleAddress,ripple_data,ripple);
BEAST_DEFINE_TESTSUITE(RippleIdentifier,ripple_data,ripple);

} // ripple
