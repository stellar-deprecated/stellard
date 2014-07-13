#ifndef __STELLARPUBLICKEY__
#define __STELLARPUBLICKEY__

//#include <boost/functional/hash.hpp>
//#include "../crypto/Base58Data.h"
#include "../protocol/RippleAddress.h"
//#include "../ripple/types/api/base_uint.h"
//#include "../ripple/types/api/Blob.h"

/*
one half of the signing key
*/
namespace ripple
{

	class StellarPublicKey : public CBase58Data
	{
		bool    mIsValid;
	public:
		StellarPublicKey();

		StellarPublicKey(const Blob& publicKey, RippleAddress::VersionEncoding type);
		StellarPublicKey(const std::string& base58Key, RippleAddress::VersionEncoding type);

		bool setKey(const std::string& base58Key, RippleAddress::VersionEncoding type);

		void clear();

		uint160 getAccountID() const;

		bool verifySignature(uint256 const& hash, Blob const& vchSig) const;
		bool verifySignature(uint256 const& hash, const std::string& strSig) const;

		std::string base58Account() const;
		std::string base58Key() const;
	};
}

#endif
