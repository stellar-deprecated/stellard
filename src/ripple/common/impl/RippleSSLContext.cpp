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

#include "../RippleSSLContext.h"

#include <cstdint>
#include <sstream>

namespace ripple {

class RippleSSLContextImp : public RippleSSLContext
{
private:
    boost::asio::ssl::context m_context;

public:
    RippleSSLContextImp ()
        : RippleSSLContext (m_context)
        , m_context (boost::asio::ssl::context::sslv23)
    {
    }

    ~RippleSSLContextImp ()
    {
    }

    static DH* tmp_dh_handler (SSL*, int, int key_length)
    {
        return DHparams_dup (getDH (key_length));
    }

    // Pretty prints an error message
    std::string error_message (std::string const& what,
        boost::system::error_code const& ec)
    {
        std::stringstream ss;
        ss <<
            what << ": " <<
            ec.message() <<
            " (" << ec.value() << ")";
        return ss.str();
    }

    //--------------------------------------------------------------------------

    static std::string getRawDHParams (int keySize)
    {
        std::string params;

        switch (keySize)
        {
        case 512:
            {
                // These are the DH parameters that OpenCoin has chosen for Ripple
                //
                std::uint8_t const raw [] = {
                    0x30, 0x46, 0x02, 0x41, 0x00, 0x98, 0x15, 0xd2, 0xd0, 0x08, 0x32, 0xda,
                    0xaa, 0xac, 0xc4, 0x71, 0xa3, 0x1b, 0x11, 0xf0, 0x6c, 0x62, 0xb2, 0x35,
                    0x8a, 0x10, 0x92, 0xc6, 0x0a, 0xa3, 0x84, 0x7e, 0xaf, 0x17, 0x29, 0x0b,
                    0x70, 0xef, 0x07, 0x4f, 0xfc, 0x9d, 0x6d, 0x87, 0x99, 0x19, 0x09, 0x5b,
                    0x6e, 0xdb, 0x57, 0x72, 0x4a, 0x7e, 0xcd, 0xaf, 0xbd, 0x3a, 0x97, 0x55,
                    0x51, 0x77, 0x5a, 0x34, 0x7c, 0xe8, 0xc5, 0x71, 0x63, 0x02, 0x01, 0x02
                };

                params.resize (sizeof (raw));
                std::copy (raw, raw + sizeof (raw), params.begin ());
            }
            break;
        case 1024:
            {
                std::uint8_t const raw [] = {
                    0x30, 0x81, 0x87, 0x02, 0x81, 0x81, 0x00, 0xe6, 0x1d, 0xa1, 0x9f, 0x5a,
                    0x89, 0x99, 0x9e, 0x6d, 0x8e, 0xf2, 0xb6, 0x30, 0x75, 0x32, 0x2c, 0xe8,
                    0x17, 0x62, 0xcc, 0xaa, 0xfa, 0xd4, 0x63, 0xc9, 0xcf, 0x9a, 0xcd, 0xb6,
                    0x2f, 0x23, 0xad, 0x51, 0xfe, 0x64, 0x18, 0xca, 0x55, 0x3a, 0x11, 0x0a,
                    0xa5, 0x55, 0x31, 0x0e, 0x99, 0x35, 0x8d, 0xbd, 0x2c, 0x68, 0x94, 0xbd,
                    0x94, 0x58, 0xb7, 0xfb, 0x86, 0xd6, 0xce, 0x8f, 0xcd, 0x4c, 0x13, 0x98,
                    0x8d, 0x27, 0x5d, 0x77, 0x56, 0x45, 0xfa, 0x1b, 0xb1, 0x5d, 0x14, 0x7d,
                    0xad, 0xf7, 0xe0, 0xe7, 0x48, 0x0d, 0xe3, 0xa6, 0x3f, 0x36, 0x77, 0xe0,
                    0xb3, 0xcf, 0xcd, 0x67, 0xeb, 0xf0, 0xb9, 0xa1, 0xe3, 0x34, 0x1f, 0xfe,
                    0x39, 0xd9, 0xab, 0x0b, 0x07, 0xd7, 0x99, 0xe9, 0xfb, 0x52, 0xd7, 0xaa,
                    0x01, 0xe4, 0x1c, 0x9d, 0x71, 0x07, 0x43, 0xdf, 0xff, 0x2b, 0x85, 0x58,
                    0x24, 0xd2, 0x8b, 0x02, 0x01, 0x02
                };

                params.resize (sizeof (raw));
                std::copy (raw, raw + sizeof (raw), params.begin ());
            }
            break;
        case 2048:
            {
                std::uint8_t const raw [] = {
                    0x30, 0x82, 0x01, 0x08, 0x02, 0x82, 0x01, 0x01, 0x00, 0xa2, 0xf2, 0x63,
                    0x7a, 0x65, 0x57, 0xc4, 0x29, 0x39, 0x35, 0x41, 0x39, 0xb8, 0xd6, 0xf7,
                    0xf5, 0x8a, 0x0f, 0x52, 0xad, 0x6d, 0xb2, 0x7d, 0xe2, 0xee, 0xbe, 0x8a,
                    0xca, 0x97, 0xed, 0x1e, 0x66, 0x95, 0x62, 0xb8, 0x9e, 0xc5, 0x26, 0x9a,
                    0x61, 0x41, 0x3c, 0xbd, 0x07, 0xd1, 0x00, 0x09, 0xbd, 0x8b, 0x66, 0x85,
                    0x0f, 0x07, 0xcf, 0x85, 0xe4, 0xa3, 0x99, 0x2a, 0xf1, 0x95, 0x26, 0x74,
                    0x9f, 0xfc, 0x39, 0x3d, 0xc8, 0x6f, 0x26, 0x2a, 0x2e, 0xaa, 0x5b, 0x1e,
                    0x16, 0xe1, 0x63, 0xa9, 0xcb, 0xd4, 0xb1, 0x78, 0x93, 0x8a, 0x1b, 0xe3,
                    0x7a, 0xed, 0x71, 0x14, 0x7b, 0x57, 0x6f, 0x32, 0x5b, 0x8a, 0x42, 0xab,
                    0x82, 0x5d, 0x14, 0x7e, 0xaa, 0xbe, 0x12, 0x39, 0xa4, 0x4b, 0x8b, 0x2c,
                    0x26, 0x2c, 0x21, 0xf0, 0x92, 0xc9, 0x5d, 0xb6, 0xa6, 0x91, 0x90, 0x7c,
                    0x36, 0xf1, 0xe9, 0xe6, 0x3e, 0xc4, 0x48, 0xc7, 0xa8, 0xf4, 0x29, 0xbd,
                    0xfe, 0x2e, 0xff, 0xc2, 0x6c, 0xf9, 0xb2, 0x09, 0x1e, 0x5e, 0x63, 0xfe,
                    0x0d, 0x53, 0x9c, 0xe1, 0x42, 0x79, 0x31, 0x64, 0x98, 0x7e, 0xcf, 0xd7,
                    0x02, 0xd5, 0x97, 0xe7, 0x5c, 0x68, 0x14, 0x2d, 0x13, 0x83, 0xf7, 0x1e,
                    0xa4, 0xa3, 0xb2, 0x21, 0x0e, 0x7c, 0x2c, 0x5b, 0x1f, 0xf6, 0x64, 0x7f,
                    0x19, 0x7d, 0x88, 0x72, 0x42, 0xe7, 0xd3, 0xca, 0xbf, 0x3c, 0x49, 0x16,
                    0x3b, 0x41, 0x6f, 0x22, 0x45, 0x9f, 0x57, 0x2b, 0x72, 0x17, 0xbb, 0x11,
                    0x2d, 0x93, 0xb9, 0x4f, 0xcd, 0xa6, 0x00, 0xc7, 0xb1, 0x21, 0xc8, 0xb6,
                    0x45, 0x06, 0x98, 0xd9, 0x28, 0x19, 0x88, 0x4a, 0x97, 0x34, 0x3c, 0x09,
                    0x0e, 0x9f, 0x37, 0xdc, 0x84, 0xae, 0x7e, 0x4d, 0x3a, 0x8b, 0x8b, 0xc0,
                    0x7a, 0x60, 0xa8, 0x6e, 0x6b, 0xcf, 0x0f, 0xa8, 0x34, 0x1b, 0xf2, 0x97,
                    0x9b, 0x02, 0x01, 0x02
                };

                params.resize (sizeof (raw));
                std::copy (raw, raw + sizeof (raw), params.begin ());
            }
            break;
        default:
            return getRawDHParams(2048);
        };

        return params;
    }

    //--------------------------------------------------------------------------

    // Does common initialization for all but the bare context type.
    void initCommon ()
    {
        m_context.set_options (
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::single_dh_use);

        SSL_CTX_set_tmp_dh_callback (
            m_context.native_handle (),
            tmp_dh_handler);
    }

    //--------------------------------------------------------------------------

    void initAnonymous (std::string const& cipherList)
    {
        initCommon ();

        int const result = SSL_CTX_set_cipher_list (
            m_context.native_handle (),
            cipherList.c_str ());

        if (result != 1)
            beast::FatalError ("invalid cipher list", __FILE__, __LINE__);
    }

    //--------------------------------------------------------------------------

    void initAuthenticated (
        std::string key_file, std::string cert_file, std::string chain_file)
    {
        initCommon ();

        SSL_CTX* const ssl = m_context.native_handle ();

        bool cert_set = false;

        if (! cert_file.empty ())
        {
            boost::system::error_code ec;
            
            m_context.use_certificate_file (
                cert_file, boost::asio::ssl::context::pem, ec);

            if (ec)
            {
                beast::FatalError (error_message (
                    "Problem with SSL certificate file.", ec).c_str(),
                    __FILE__, __LINE__);
            }

            cert_set = true;
        }

        if (! chain_file.empty ())
        {
            // VFALCO Replace fopen() with RAII
            FILE* f = fopen (chain_file.c_str (), "r");

            if (!f)
            {
                beast::FatalError (error_message (
                    "Problem opening SSL chain file.", boost::system::error_code (errno,
                    boost::system::generic_category())).c_str(),
                    __FILE__, __LINE__);
            }

            try
            {
                for (;;)
                {
                    X509* const x = PEM_read_X509 (f, nullptr, nullptr, nullptr);

                    if (x == nullptr)
                        break;

                    if (! cert_set)
                    {
                        if (SSL_CTX_use_certificate (ssl, x) != 1)
                            beast::FatalError ("Problem retrieving SSL certificate from chain file.",
                                __FILE__, __LINE__);

                        cert_set = true;
                    }
                    else if (SSL_CTX_add_extra_chain_cert (ssl, x) != 1)
                    {
                        X509_free (x);
                        beast::FatalError ("Problem adding SSL chain certificate.",
                            __FILE__, __LINE__);
                    }
                }

                fclose (f);
            }
            catch (...)
            {
                fclose (f);
                beast::FatalError ("Reading the SSL chain file generated an exception.",
                    __FILE__, __LINE__);
            }
        }

        if (! key_file.empty ())
        {
            boost::system::error_code ec;

            m_context.use_private_key_file (key_file,
                boost::asio::ssl::context::pem, ec);

            if (ec)
            {
                beast::FatalError (error_message (
                    "Problem using the SSL private key file.", ec).c_str(),
                    __FILE__, __LINE__);
            }
        }

        if (SSL_CTX_check_private_key (ssl) != 1)
        {
            beast::FatalError ("Invalid key in SSL private key file.",
                __FILE__, __LINE__);
        }
    }

    //--------------------------------------------------------------------------

    // A simple RAII container for a DH
    //
    struct ScopedDHPointer
    {
        // Construct from an existing DH
        //
        explicit ScopedDHPointer (DH* dh)
            : m_dh (dh)
        {
        }

        // Construct from raw DH params
        //
        explicit ScopedDHPointer (std::string const& params)
        {
            auto const* p (
                reinterpret_cast <std::uint8_t const*>(&params [0]));
            m_dh = d2i_DHparams (nullptr, &p, params.size ());
            if (m_dh == nullptr)
                beast::FatalError ("d2i_DHparams returned nullptr.",
                    __FILE__, __LINE__);
        }

        ~ScopedDHPointer ()
        {
            if (m_dh != nullptr)
                DH_free (m_dh);
        }

        operator DH* () const
        {
            return get ();
        }

        DH* get () const
        {
            return m_dh;
        }

    private:
        DH* m_dh;
    };

    //--------------------------------------------------------------------------

    static DH* getDH (int keyLength)
    {
        if (keyLength == 512 || keyLength == 1024 || keyLength == 2048)
        {
            static ScopedDHPointer dh(getRawDHParams (keyLength));

            return dh.get ();
        }
        else
        {
            beast::FatalError ("unsupported key length", __FILE__, __LINE__);
        }

        return nullptr;
    }
};

//------------------------------------------------------------------------------

RippleSSLContext::RippleSSLContext (ContextType& context)
    : SSLContext (context)
{
}

RippleSSLContext* RippleSSLContext::createBare ()
{
    std::unique_ptr <RippleSSLContextImp> context (new RippleSSLContextImp ());
    return context.release ();
}

RippleSSLContext* RippleSSLContext::createWebSocket ()
{
    std::unique_ptr <RippleSSLContextImp> context (new RippleSSLContextImp ());
    context->initCommon ();
    return context.release ();
}

RippleSSLContext* RippleSSLContext::createAnonymous (std::string const& cipherList)
{
    std::unique_ptr <RippleSSLContextImp> context (new RippleSSLContextImp ());
    context->initAnonymous (cipherList);
    return context.release ();
}

RippleSSLContext* RippleSSLContext::createAuthenticated (
    std::string key_file, std::string cert_file, std::string chain_file)
{
    std::unique_ptr <RippleSSLContextImp> context (new RippleSSLContextImp ());
    context->initAuthenticated (key_file, cert_file, chain_file);
    return context.release ();
}

std::string RippleSSLContext::getRawDHParams (int keySize)
{
    return RippleSSLContextImp::getRawDHParams (keySize);
}

}
