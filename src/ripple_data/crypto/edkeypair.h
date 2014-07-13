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
		
	public:
		Blob mPrivateKey;
		Blob mPublicKey;

		EdKeyPair();
		EdKeyPair(const uint256& passPhrase);
		EdKeyPair(const Blob& seed);

		void setSeed(const Blob& seed);

		Blob getPubKey() const {  return(mPublicKey); };

		static uint256 passPhraseToKey(const std::string& passPhrase);

		void getPrivateKeyU(uint256& privKey);
	};

}


#endif
