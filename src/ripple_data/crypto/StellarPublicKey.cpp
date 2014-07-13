#include "StellarPublicKey.h"
#include <sodium.h>

namespace ripple
{


	StellarPublicKey::StellarPublicKey(Blob& publicKey, RippleAddress::VersionEncoding type)
	{
		SetData(type, publicKey);
	}

	StellarPublicKey::StellarPublicKey(std::string& hexKey, RippleAddress::VersionEncoding type) 
	{
		assert(0); // TODO:
	}

	bool StellarPublicKey::verifySignature(uint256 const& hash, const std::string& strSig) const
	{
		Blob vchSig(strSig.begin(), strSig.end());
		return(verifySignature(hash, vchSig));
	}

	bool StellarPublicKey::verifySignature(uint256 const& hash, Blob const& vchSig) const
	{
		if (vchData.size() != crypto_sign_PUBLICKEYBYTES
			|| vchSig.size() != crypto_sign_BYTES)
			throw std::runtime_error("bad inputs to verifySignature");
		/*
		unsigned char signed_buf[crypto_sign_BYTES + hash.bytes];
		memcpy (signed_buf, vchSig.data(), crypto_sign_BYTES);
		memcpy (signed_buf+crypto_sign_BYTES, hash.data(), hash.bytes);

		unsigned char ignored_buf[hash.bytes];
		unsigned long long ignored_len;
		return crypto_sign_open (ignored_buf, &ignored_len,
		signed_buf, sizeof (signed_buf),
		vchData.data()) == 0;
		*/

		unsigned char signed_buf[crypto_sign_BYTES + hash.bytes];
		memcpy(signed_buf, vchSig.data(), crypto_sign_BYTES);
		memcpy(signed_buf + crypto_sign_BYTES, hash.data(), hash.bytes);


		unsigned char ignored_buf[hash.bytes];
		unsigned long long ignored_len;
		return crypto_sign_open(ignored_buf, &ignored_len,
			signed_buf, sizeof (signed_buf),
			vchData.data()) == 0;


	}


}
