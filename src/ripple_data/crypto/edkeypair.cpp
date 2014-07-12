
#include "edkeypair.h"
#include <sodium.h>

namespace ripple {

	EdKeyPair::EdKeyPair(const uint256& passPhrase)
	{
		mPublicKey.resize(crypto_sign_ed25519_PUBLICKEYBYTES);
		mPrivateKey.resize(crypto_sign_ed25519_SECRETKEYBYTES);
	

		if (crypto_box_seed_keypair(&(mPublicKey[0]), &mPrivateKey[0], passPhrase.begin()) == -1)
		{
			throw key_error("EdKeyPair::EdKeyPair(const uint128& passPhrase) failed");
		}
	}


	// <-- seed
	uint256 EdKeyPair::passPhraseToKey(const std::string& passPhrase)
	{
		// TODO: Should we use the libsodium hashing here?
		Serializer s;

		s.addRaw(passPhrase);
		uint256 hash256 = s.getSHA512Half();
		s.secureErase();

		return hash256;
	}

	void EdKeyPair::getPrivateKeyU(uint256& privKey)
	{
		memcpy(privKey.begin(), &mPrivateKey[0], crypto_sign_ed25519_SECRETKEYBYTES);
	}

}