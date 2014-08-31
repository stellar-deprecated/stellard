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

#include "AutoSocket.h"

#include "ripple_basics/log/Log.h"
//#include "beast/beast/asio/bind_handler.h"

ripple::LogPartition AutoSocket::AutoSocketPartition ("AutoSocket");


bool AutoSocket::rfc2818_verify(const std::string& domain, bool preverified, boost::asio::ssl::verify_context& ctx)
{
	using namespace ripple;

	if (boost::asio::ssl::rfc2818_verification(domain) (preverified, ctx))
		return true;

	Log(lsWARNING, AutoSocketPartition) << "Outbound SSL connection to " <<
		domain << " fails certificate verification";
	return false;
}


boost::system::error_code AutoSocket::verify(const std::string& strDomain)
{
	boost::system::error_code ec;

	mSocket->set_verify_mode(boost::asio::ssl::verify_peer);

	// XXX Verify semantics of RFC 2818 are what we want.
	mSocket->set_verify_callback(std::bind(&rfc2818_verify, strDomain, std::placeholders::_1, std::placeholders::_2), ec);

	return ec;
}

void AutoSocket::handle_autodetect(callback cbFunc, const error_code& ec, size_t bytesTransferred)
{
	using namespace ripple;

	if (ec)
	{
		Log(lsWARNING, AutoSocketPartition) << "Handle autodetect error: " << ec;
		cbFunc(ec);
	}
	else if ((mBuffer[0] < 127) && (mBuffer[0] > 31) &&
		((bytesTransferred < 2) || ((mBuffer[1] < 127) && (mBuffer[1] > 31))) &&
		((bytesTransferred < 3) || ((mBuffer[2] < 127) && (mBuffer[2] > 31))) &&
		((bytesTransferred < 4) || ((mBuffer[3] < 127) && (mBuffer[3] > 31))))
	{
		// not ssl
		if (AutoSocketPartition.doLog(lsTRACE))
			Log(lsTRACE, AutoSocketPartition) << "non-SSL";

		mSecure = false;
		cbFunc(ec);
	}
	else
	{
		// ssl
		if (AutoSocketPartition.doLog(lsTRACE))
			Log(lsTRACE, AutoSocketPartition) << "SSL";

		mSecure = true;
		mSocket->async_handshake(ssl_socket::server, cbFunc);
	}
}


void AutoSocket::async_handshake(handshake_type type, callback cbFunc)
{
	if ((type == ssl_socket::client) || (mSecure))
	{
		// must be ssl
		mSecure = true;
		mSocket->async_handshake(type, cbFunc);
	}
	else if (mBuffer.empty())
	{
		// must be plain
		mSecure = false;
		mSocket->get_io_service().post(
			beast::asio::bind_handler(cbFunc, error_code()));
	}
	else
	{
		// autodetect
		mSocket->next_layer().async_receive(boost::asio::buffer(mBuffer), boost::asio::socket_base::message_peek,
			boost::bind(&AutoSocket::handle_autodetect, this, cbFunc,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
	}
}
