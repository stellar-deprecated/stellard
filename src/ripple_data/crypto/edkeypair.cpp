
#include "edkeypair.h"
#include <sodium.h>

namespace ripple {

	EdKeyPair::EdKeyPair(const uint128& passPhrase)
	{
		mPublicKey.resize(32);
		mPrivateKey.resize(32);
		unsigned char seed[32];
		
		memcpy(seed, passPhrase.begin(), 16);
		memcpy(seed+16, passPhrase.begin(), 16); 

		if (crypto_box_seed_keypair(&(mPublicKey[0]), &mPrivateKey[0], (unsigned char*)&seed)==-1)
		{
			throw key_error("EdKeyPair::EdKeyPair(const uint128& passPhrase) failed");
		}
	}


	/*
	EdKeyPair::EdKeyPair(const RippleAddress& base, const BIGNUM* rootPrivKey, int n)
	{
		// private deterministic key
		pkey = GeneratePrivateDeterministicKey(base, rootPrivKey, n);
		assert(pkey);
	}

	EdKeyPair::EdKeyPair(const RippleAddress& generator, int n)
	{
		// public deterministic key
		pkey = GeneratePublicDeterministicKey(generator, n);
		fSet = true;
		assert(pkey);
	}

	


	BIGNUM* EdKeyPair::GetSecretBN() const
	{
		// DEPRECATED
		return BN_dup(EC_KEY_get0_private_key(pkey));
	}

	*/


	// <-- seed
	uint128 EdKeyPair::PassPhraseToKey(const std::string& passPhrase)
	{
		Serializer s;

		s.addRaw(passPhrase);
		// NIKB TODO this caling sequence is a bit ugly; this should be improved.
		uint256 hash256 = s.getSHA512Half();
		uint128 ret(uint128::fromVoid(hash256.data()));

		s.secureErase();

		return ret;
	}

	void EdKeyPair::GetPrivateKeyU(uint256& privKey)
	{
		memcpy(privKey.begin(), &mPrivateKey[0], 32);
	}

}