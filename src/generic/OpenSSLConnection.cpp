#include "OpenSSLConnection.h"

#ifdef HTTPS_BACKEND_OPENSSL

#include "../common/LibraryLoader.h"

// Not present in openssl 1.1 headers
#define SSL_CTRL_OPTIONS 32

OpenSSLConnection::SSLFuncs::SSLFuncs()
{
	using namespace LibraryLoader;

	valid = false;

	// Try OpenSSL 1.1
	handle *sslhandle = OpenLibrary("libssl.so.1.1");
	handle *cryptohandle = OpenLibrary("libcrypto.so.1.1");
	// Try OpenSSL 1.0
	if (!sslhandle || !cryptohandle)
	{
		sslhandle = OpenLibrary("libssl.so.1.0.0");
		cryptohandle = OpenLibrary("libcrypto.so.1.0.0");
	}
	// Try OpenSSL without version
	if (!sslhandle || !cryptohandle)
	{
		sslhandle = OpenLibrary("libssl.so");
		cryptohandle = OpenLibrary("libcrypto.so");
	}
	// Give up
	if (!sslhandle || !cryptohandle)
		return;

	valid = true;
	valid = valid && (LoadSymbol(library_init, sslhandle, "SSL_library_init") ||
			LoadSymbol(init_ssl, sslhandle, "OPENSSL_init_ssl"));

	valid = valid && LoadSymbol(CTX_new, sslhandle, "SSL_CTX_new");
	valid = valid && LoadSymbol(CTX_ctrl, sslhandle, "SSL_CTX_ctrl");
	valid = valid && LoadSymbol(CTX_set_verify, sslhandle, "SSL_CTX_set_verify");
	valid = valid && LoadSymbol(CTX_set_default_verify_paths, sslhandle, "SSL_CTX_set_default_verify_paths");
	valid = valid && LoadSymbol(CTX_free, sslhandle, "SSL_CTX_free");

	valid = valid && LoadSymbol(SSL_new, sslhandle, "SSL_new");
	valid = valid && LoadSymbol(SSL_free, sslhandle, "SSL_free");
	valid = valid && LoadSymbol(set_fd, sslhandle, "SSL_set_fd");
	valid = valid && LoadSymbol(connect, sslhandle, "SSL_connect");
	valid = valid && LoadSymbol(read, sslhandle, "SSL_read");
	valid = valid && LoadSymbol(write, sslhandle, "SSL_write");
	valid = valid && LoadSymbol(shutdown, sslhandle, "SSL_shutdown");
	valid = valid && LoadSymbol(get_verify_result, sslhandle, "SSL_get_verify_result");
	valid = valid && LoadSymbol(get_peer_certificate, sslhandle, "SSL_get_peer_certificate");

	valid = valid && (LoadSymbol(SSLv23_method, sslhandle, "SSLv23_method") ||
			LoadSymbol(SSLv23_method, sslhandle, "TLS_method"));

	valid = valid && LoadSymbol(check_host, cryptohandle, "X509_check_host");

	if (library_init)
		library_init();
	else if(init_ssl)
		init_ssl(0, nullptr);
	// else not valid
}

bool OpenSSLConnection::valid()
{
	return ssl.valid;
}

OpenSSLConnection::OpenSSLConnection()
	: conn(nullptr)
{
	context = ssl.CTX_new(ssl.SSLv23_method());
	if (!context)
		return;

	ssl.CTX_ctrl(context, SSL_CTRL_OPTIONS, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3, nullptr);
	ssl.CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
	ssl.CTX_set_default_verify_paths(context);
}

OpenSSLConnection::~OpenSSLConnection()
{
	if (conn)
		ssl.SSL_free(conn);

	if (context)
		ssl.CTX_free(context);
}

bool OpenSSLConnection::connect(const std::string &hostname, uint16_t port)
{
	if (!context)
		return false;

	if (!socket.connect(hostname, port))
		return false;

	conn = ssl.SSL_new(context);
	if (!conn)
	{
		socket.close();
		return false;
	}

	ssl.set_fd(conn, socket.getFd());
	if (ssl.connect(conn) != 1 || ssl.get_verify_result(conn) != X509_V_OK)
	{
		socket.close();
		return false;
	}

	X509 *cert = ssl.get_peer_certificate(conn);
	if (ssl.check_host(cert, hostname.c_str(), hostname.size(), 0, nullptr) != 1)
	{
		close();
		return false;
	}

	return true;
}

size_t OpenSSLConnection::read(char *buffer, size_t size)
{
	return ssl.read(conn, buffer, (int) size);
}

size_t OpenSSLConnection::write(const char *buffer, size_t size)
{
	return ssl.write(conn, buffer, (int) size);
}

void OpenSSLConnection::close()
{
	ssl.shutdown(conn);
	socket.close();
}

OpenSSLConnection::SSLFuncs OpenSSLConnection::ssl;

#endif // HTTPS_BACKEND_OPENSSL
