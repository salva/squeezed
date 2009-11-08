/* Minimal implementation of python's socket library
 *
 * http://docs.python.org/3.1/library/socket.html#module-socket
 *
 * TODO: rewrite slimIPC.cpp::musicFile::setFromFile() to use this also
 */


#include <string>

//for AF_INET
#ifdef WIN32
	#include <Ws2tcpip.h>
#else
	#include <netdb.h>
#endif

class Socket 
{
public:
       //Use an unnamed struct to hide the socket type from the interface.
       // This hides the different datatypes in win32/posix from this interface
       struct socket_s;

  private:
	int _family;
	int _type;
	int _prot;

	// Since socket_s is unnamed, we can only have a pointer to it:
	struct socket_s *_sock;
public:
	
	Socket(int af=AF_INET, int type=SOCK_STREAM, int protocol=IPPROTO_TCP);

	~Socket();

	/// Convert host name to an ip-address (as string)
	static std::string getHostByName(const char *hostname);

	/// Open a connection
	/// host-name should be an ascii-string of the ip-address
	int Connect(const char *host, int port);

	/// Close the connection
	void Close(void);

	/// Send data over the socket
	int Send(const char *data, size_t len);

	/// Receive data
	int Recv(char *data, size_t len);

	void setBlocking(bool doBlock);
};


