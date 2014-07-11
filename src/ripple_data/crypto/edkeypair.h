#ifndef __EDKEYPAIR__
#define __EDKEYPAIR__

#include "../ripple_data.h"

/*

*/

namespace ripple {

	class key_error : public std::runtime_error
	{
	public:
		explicit key_error(const std::string& str) : std::runtime_error(str) {}
	};

	class EdKeyPair
	{
		Blob mPrivateKey;
		Blob mPublicKey;

	public:
		EdKeyPair(const uint256& passPhrase);



		Blob GetPubKey() const {  return(mPublicKey); };

		static uint256 PassPhraseToKey(const std::string& passPhrase);

		void GetPrivateKeyU(uint256& privKey);
	};

}


#endif
