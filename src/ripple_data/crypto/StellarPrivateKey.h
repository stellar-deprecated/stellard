#ifndef __STELLARPRIVATECKEY__
#define __STELLARPRIVATECKEY__

#include "../protocol/RippleAddress.h"
#include "EdKeyPair.h"

/*
one half of the signing key
*/
namespace ripple
{

	class StellarPrivateKey
	{
		Blob mSeed;
		EdKeyPair mPair;
		RippleAddress::VersionEncoding mType;


	public:
		// make a new random key
		StellarPrivateKey(RippleAddress::VersionEncoding type);
		// make a key from a pass phrase
		StellarPrivateKey(std::string& passPhrase, RippleAddress::VersionEncoding type);
		// make a key from a base58 encoded one
		StellarPrivateKey(std::string& base58seed);

		void sign(uint256 const& message, Blob& retSignature) const;

		std::string base58Seed() const;
		std::string base58Account() const;
		std::string base58PublicKey(RippleAddress::VersionEncoding type) const;
		std::string hexPublicKey() const;

		uint160 getAccountID() const;
		Blob& getPublicKey(){ return(mPair.mPublicKey); }
		bool isValid(){ return(mType != RippleAddress::VER_NONE); }
	};
}

#endif