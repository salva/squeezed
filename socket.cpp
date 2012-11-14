#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>

#ifdef WIN32
	#include <Winsock2.h>
	typedef SOCKET SOCKET_T;
#else
	#include <netdb.h>		//to lookup hostnames
	#include <netinet/in.h>
	#include <arpa/inet.h>  
	#include <unistd.h>     // close()
	#include <ctype.h>		// isalpha()
	typedef int SOCKET_T;
	typedef struct sockaddr SOCKADDR;
#endif


#include "socket.hpp"

using namespace std;


#ifdef WIN32
// Little hack to initialize winsock only once during application startup:
class winSockInit {
private:
	WSADATA wsaData;
	int iResult;
public:
	winSockInit()
	{
		iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		if (iResult != NO_ERROR)
			fprintf(stderr,"WSA init returns %i\n", iResult);
	}

	~winSockInit()
	{
		WSACleanup();
	}

	int result(void)
	{
		return iResult;
	}

};
const static winSockInit _winSockInit;
#endif


struct Socket::socket_s {
  SOCKET_T s;
};


Socket::Socket(int af, int type, int protocol):
	_family(af), _type(type), _prot(protocol)
{
      _sock = new socket_s;
      _sock->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}


Socket::~Socket()
{
	Close();
	delete _sock;
}


string Socket::getHostByName(const char *hostname)
{
	string ret;
	struct hostent *remoteHost;
	in_addr address;

	if( isalpha( hostname[0] ) )
	{
		remoteHost = gethostbyname( hostname );
		if( remoteHost != NULL )
		{
#ifdef WIN32
			address.s_addr = *(ULONG*)remoteHost->h_addr_list[0];	//address = *((in_addr * )remoteHost->h_addr);
#else
			address.s_addr = *(in_addr_t*)remoteHost->h_addr_list[0];	//address = *((in_addr * )remoteHost->h_addr);
#endif
		} else {
			address.s_addr = 0;
		}
	} else {
		//in_addr in_address;	address = in_address.s_addr;
		//inet_aton ( ip.c_str(), &address );
		address.s_addr = inet_addr( hostname );
		//remoteHost = gethostbyaddr((char *) &address, 4, AF_INET);
	}

	//ret.assign( remoteHost->h_name );
	ret.assign( inet_ntoa(address) );

	//ret = *((unsigned int*)(&address.s_addr));
	return ret;
}



int Socket::Connect(const char *host, int port)
{
	sockaddr_in target;
	target.sin_family = _family;
	target.sin_addr.s_addr = inet_addr(host);
	target.sin_port = htons( port );

#ifdef WIN32
	if (_sock->s == INVALID_SOCKET) 
#else
	if (_sock->s < 0)
#endif
		return -1;

	connect(_sock->s, (SOCKADDR*)&target, sizeof(target));
	return 0;
}



void Socket::Close(void)
{
#ifdef WIN32
	closesocket(_sock->s );
#else
	close( _sock->s );
#endif
}



int Socket::Send(const char *data, size_t len)
{
	int flags = 0;
	return send(_sock->s, data, len, flags);
}



int Socket::Recv(char *data, size_t len)
{
	int flags = 0;
	return recv(_sock->s, data, len, flags);
}



void Socket::setBlocking(bool doBlock)
{
   int flags;
   int r;
	
   flags = fcntl (_sock->s, F_GETFL);

	if( doBlock )
	{
		u_long iMode = 0;	// blocking
		//int r = ioctlsocket(_sock->s, FIONBIO, &iMode);
		//int r = fcntl(_sock->s, F_SETFL, &iMode) ;
      		r = fcntl (_sock->s, F_SETFL, flags & ~O_NONBLOCK);	
	} else {
		//set client to non-blocking mode: (is server is non-blocking, so will the clients be)
		u_long iMode = 1;	// non-blocking
		//int r = ioctlsocket(_sock->s, FIONBIO, &iMode);
		//int r = fcntl(_sock->s, F_SETFL, &iMode) ;
      		r = fcntl (_sock->s, F_SETFL, flags | O_NONBLOCK);	
	}
	//return r==0;	//return True if no error.
}
