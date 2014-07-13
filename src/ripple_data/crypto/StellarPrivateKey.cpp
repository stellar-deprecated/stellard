
#include "StellarPrivateKey.h"
#include <sodium.h>
#include "../ripple/sslutil/api/HashUtilities.h"
/*
Private key

can come from:
-randomness
-passed in pass phrase
-base58 seed
*/

namespace ripple
{
	

	//create from the void!
	StellarPrivateKey::StellarPrivateKey(RippleAddress::VersionEncoding type) 
	{
		mType = type;
		mSeed.resize(crypto_sign_SEEDBYTES);
		RandomNumbers::getInstance().fillBytes(&(mSeed[0]), mSeed.size());

		mPair.setSeed(mSeed);
	}

	// create from a passphrase
	StellarPrivateKey::StellarPrivateKey(std::string& passPhrase, RippleAddress::VersionEncoding type)
	{
		mType = type;
		mSeed.resize(crypto_sign_SEEDBYTES);

		// TODO: Should we use the libsodium hashing here?
		Serializer s;

		s.addRaw(passPhrase);
		uint256 hash256 = s.getSHA512Half();
		s.secureErase();

		memcpy(&mSeed[0], hash256.data(), crypto_sign_SEEDBYTES);
		
		mPair.setSeed(mSeed);
	}

	// create from a base58 encoded seed
	StellarPrivateKey::StellarPrivateKey(std::string& base58seed) 
	{
		mSeed.resize(crypto_sign_SEEDBYTES);

		Blob vchTemp;
		Base58::decodeWithCheck(base58seed.c_str(), vchTemp, Base58::getRippleAlphabet() );

		if (vchTemp.empty() && 
			vchTemp.size() == crypto_sign_SEEDBYTES + 1 &&
			vchTemp[0]==RippleAddress::VER_SEED)
		{
			mType = RippleAddress::VER_NONE;
			return;
		}

		mType = RippleAddress::VER_ACCOUNT_PRIVATE;
		memcpy(&mSeed[0], vchTemp.data() + 1, crypto_sign_SEEDBYTES);
		mPair.setSeed(mSeed);
	}

	uint160 StellarPrivateKey::getAccountID() const
	{
		return Hash160(mPair.mPublicKey);
	}
	
	// create accountID from this seed and base58 encode it
	std::string StellarPrivateKey::base58Account() const
	{
		uint160 account=getAccountID();

		Blob vch(1, RippleAddress::VER_ACCOUNT_ID);

		vch.insert(vch.end(), account.begin(), account.end());

		return Base58::encodeWithCheck(vch);

	}

	std::string StellarPrivateKey::base58PublicKey(RippleAddress::VersionEncoding type) const
	{
		Blob vch(1, type);

		vch.insert(vch.end(), mPair.mPublicKey.begin(), mPair.mPublicKey.end());

		return Base58::encodeWithCheck(vch);
	}

	std::string StellarPrivateKey::base58Seed() const
	{
		Blob vch(1, RippleAddress::VER_SEED);

		vch.insert(vch.end(), mSeed.begin(), mSeed.end());

		return Base58::encodeWithCheck(vch);
	}

	std::string StellarPrivateKey::hexPublicKey() const
	{
		// TODO: 
		Blob vch(1, RippleAddress::VER_SEED);

		vch.insert(vch.end(), mSeed.begin(), mSeed.end());

		return Base58::encodeWithCheck(vch);
	}

	void StellarPrivateKey::sign(uint256 const& message, Blob& retSignature) const
	{
		unsigned char out[crypto_sign_BYTES + message.bytes];
		unsigned long long len;
		const unsigned char *key = mPair.mPrivateKey.data();

		// contrary to the docs it puts the signature in front
		crypto_sign(out, &len,
			(unsigned char*)message.begin(), message.size(),
			key);

		retSignature.resize(crypto_sign_BYTES);
		memcpy(&retSignature[0], out, crypto_sign_BYTES);
	}
}