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
	public:
		StellarPublicKey(Blob& publicKey, RippleAddress::VersionEncoding type);
		StellarPublicKey(std::string& hexKey, RippleAddress::VersionEncoding type);

		bool verifySignature(uint256 const& hash, Blob const& vchSig) const;
		bool verifySignature(uint256 const& hash, const std::string& strSig) const;
	};
}

#endif
