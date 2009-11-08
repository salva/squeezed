

//#define NOMINMAX
#include <winsock2.h>
#include <Ws2tcpip.h>

#include <vector>

#include "debug.h"
#include "TCPserver.hpp"
#include "util.hpp"

//msvc madness:
#pragma warning (disable: 4996)



// Little hack to initialize winsock:
class winSockInit {
private:
	WSADATA wsaData;
	int iResult;
public:
	winSockInit()
	{
		iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
		db_printf(2,"WSA init returns %i\n", iResult);
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



// Main server class
TCPserver::TCPserver(int port, int maxCon): 
		port(port),
		maxConnections(maxCon),
		stop(false)
{
	db_printf(2,"TCPserver: port %i\n",port);
}


// Info on all clients:
struct connections_s
{
	SOCKET socket;
	connectionHandler *handler;
	//bool needsWrite;	//when handler->writeBuf() returns >0, this stays true

	bool needsWrite(void)
	{
		bool needsWrite = (handler->bufsRemaining() > 0) && (!handler->isWriteBufBlocking());
		return needsWrite;
	}


	connections_s(SOCKET s, connectionHandler* h):
		socket(s), handler(h)
		//, needsWrite(false)
	{}

	~connections_s()
	{
	}
};

	
SOCKET setupListenSocket(int port)
{
	char portStr[6];
	int iResult;
	struct addrinfo *result = NULL, hints;
	SOCKET ListenSocket = INVALID_SOCKET;

	// Open a socket
	ZeroMemory( &hints, sizeof(hints) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	//		Resolve the local address and port to be used by the server
	sprintf(portStr, "%i", port);
	iResult = getaddrinfo(NULL, portStr, &hints, &result);
	if ( iResult != 0 )
	{
		return INVALID_SOCKET;
	}

	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) 
	{
		freeaddrinfo(result);
		return INVALID_SOCKET;
	}

	// Bind to a port
	iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if(iResult == SOCKET_ERROR) 
	{
		freeaddrinfo(result);
		closesocket(ListenSocket);
		return INVALID_SOCKET;
	}
	freeaddrinfo(result);

	// And listen
	if ( listen(ListenSocket, SOMAXCONN ) == SOCKET_ERROR ) 
	{
		db_printf(1,"Error at bind(): %ld\n", WSAGetLastError() );
		closesocket(ListenSocket);
		return INVALID_SOCKET;
	}
	db_printf(1,"Listening on port %i\n",port);

	//set to non-blocking:
	u_long iMode = 1;	//non-blocking
	int rc = ioctlsocket(ListenSocket, FIONBIO, &iMode);
	if (rc < 0)
	{
	  perror("setsockopt() failed");
	  closesocket(ListenSocket);
	  return INVALID_SOCKET;
	}

	return ListenSocket;
}


/// Close a connection
void closeHandler(std::vector<connections_s>& connections, size_t idx, fd_set *read, fd_set *write)
{
	connections_s *conn = &connections[idx];
	FD_CLR(conn->socket, read);
	FD_CLR(conn->socket, write);
	closesocket( conn->socket );
	delete conn->handler;
	connections.erase( connections.begin() + idx );	//this also deletes any outstanding write buffers
}



/// handle multiple connections:
int TCPserver::runNonBlock()
{
	// Buffer for incoming data:
	char rxBuf[ 1<<15 ];	//receive max. 32kb at once, larger data will be handled in multiple iterations

	SOCKET ListenSocket = setupListenSocket(port);
	if( ListenSocket == INVALID_SOCKET)
	{
		db_printf(1,"Could not open port %i for listening\n",port);
		return -1;
	}

	// Set up FD-sets for select-calls:
	fd_set tempset, readset, writeset; // exceptset;

	FD_ZERO(&readset);
	FD_SET(ListenSocket, &readset);
	size_t maxfd = ListenSocket;
	timeval tv;

	std::vector<connections_s> connections;	//TODO: use maxConnections to statically allocate


	//see  http://www.developerweb.net/forum/showthread.php?t=2933
	int sresult;
	while(!stop)
	{
		//(re-) initialize read sockets:
		//memcpy(&tempset, &readset, sizeof(tempset));

		maxfd = ListenSocket;		//if connections are closing, this needs to be rebuild
		// Add all clients which have needsWrite() set to the write set.
		FD_ZERO(&writeset);
		for(size_t i=0; i<connections.size(); i++)
		{
			maxfd = util::max(maxfd, connections[i].socket );
			if( connections[i].needsWrite() )
				FD_SET(connections[i].socket, &writeset );
		}

        //Re-build list of connections to read from, some might not be ready
        //for input
        FD_ZERO(&tempset);
        //TODO: Don't listen for new connections if max. connections is reached:
        FD_SET(ListenSocket, &tempset);
		for(size_t i=0; i<connections.size(); i++)
		{
			//maxfd = util::max(maxfd, connections[i].socket );
            if( !connections[i].handler->isReadBufBlocking() )
				FD_SET(connections[i].socket, &tempset );
		}
/*
		//Watch all sockets for errors:
		FD_ZERO(&exceptset);
		FD_SET(ListenSocket, &exceptset);
		for(size_t i=0; i<connections.size(); i++)
		{
			maxfd = util::max(maxfd, connections[i].socket );
			FD_SET(connections[i].socket, &exceptset );
		}
*/

		// Time-out for the master-select()-call, If a write is to be initiated from another
		// part of the software (e.g. web-gui starts playing through slim protocol).
		// slimIPC->notifyClientUpdate() connects to both slimServer and shoutServer,
		// to wake them both out of the select() call.
		tv.tv_sec =   4;		//for debug, test responsiveness
		tv.tv_usec =  0;

		sresult = select(maxfd + 1, &tempset, &writeset, NULL, &tv);

		if( sresult == 0 ) {
			//db_printf(15,"select() timed out\n");
		} else if( (sresult < 0)  && (errno != WSAEINTR) ) 
		{
			db_printf(5,"Error in select(): %s\n", strerror(errno));
			int err, errLen = sizeof(err);
			//Try to find a way to detect which socket caused this:
			for(size_t conn=0; conn < connections.size(); conn++ )
			{
				
				int r = getsockopt( connections[conn].socket, SOL_SOCKET, SO_ERROR, (char *)&err, &errLen);
				if( r < 0 )
				{
					closeHandler(connections, conn, &tempset, &writeset);
					conn--;
					continue;
				}
			}
			//This happens quite often, since slimIPC opens/closes connections immediately.
			// Eventually, the connections will be closed.
			// "no such file or directory" does not seem to cause an error for getsockopt()
			db_printf(5,"Error in select(), but couldn't find a socket with error: %s\n", strerror(errno));
			db_printf(5,"\tError of connection %i = %i\n", connections.size()-1, err);

		} 
		else if (sresult > 0)	
		{
			/*
			//Close all connections that give errors:
			for(int conn=0; conn < (int)connections.size(); conn++)
			{
				connections_s *it = &connections[conn];
				if (FD_ISSET(it->socket, &exceptset) )
				{
					FD_CLR(it->socket, &readset);	//maxFD is updated at the start of while(!stop)
					FD_CLR(it->socket, &writeset);
					closesocket( it->socket );
					delete it->handler;
					connections.erase( connections.begin() + conn );	//this also deletes any outstanding write buffers
					conn--;			//update for-loop status
					continue;       //need to break, to prevent using non-existing connections in code below
				}
			}
			*/

			//Select has got something, check if it's the server:
			if ( FD_ISSET(ListenSocket, &tempset) ) 
			{
				struct sockaddr_in peername;
				int peerlen = sizeof(peername);

				// Accept a client socket
				SOCKET ClientSocket = accept(ListenSocket, (struct sockaddr*)&peername, &peerlen );
				if (ClientSocket == INVALID_SOCKET) 
				{
					//Can happen if the client directly closes the connection again.
					db_printf(5,"accept failed: %d\n", WSAGetLastError());
					//closesocket(ListenSocket);
					continue;
				}
				
				// for debug: get client info
				char host[NI_MAXHOST],serv[NI_MAXSERV];
				int iResult = getnameinfo ( (struct sockaddr *) &peername, 
					sizeof(struct sockaddr),
					host, sizeof(host),	serv, sizeof(serv),
					NI_NUMERICSERV);


				if( connections.size() < maxConnections )
				{
					FD_SET(ClientSocket, &readset);
					maxfd = util::max(maxfd,ClientSocket);
					FD_CLR(ListenSocket, &tempset);	//server is handled, don't do it again in the comming for() loop

					//set client to non-blocking mode: (is server is non-blocking, so will the clients be)
					u_long iMode = 1;	//non-blocking
					iResult = ioctlsocket(ClientSocket, FIONBIO, &iMode);

					//create a new connection handler for this:
					connectionHandler *C = newHandler(ClientSocket);
					connections.push_back( connections_s(ClientSocket, C) );

					//db_printf(2,"connected to %s, port %s\n", host, serv );
					db_printf(2,"Opening client socket %4i on port %i, to %s:%s\n", ClientSocket, port, host,serv );
				} else {
					closesocket(ClientSocket);
					db_printf(2,"refused connection to %s, port %s\n", host, serv );
				}
			} // if FD_ISSET(ListenSocket)

			// Now iterate over all clients:
			for(int conn=0; conn < (int)connections.size(); conn++)
			{
				connections_s *it = &connections[conn];
				if (FD_ISSET(it->socket, &tempset) )			//handle reads
				{
					sresult = recv(it->socket, rxBuf, sizeof(rxBuf), 0);
					bool keepOpen = true;

					if(sresult > 0) 
						//accept the data:
						keepOpen = it->handler->processRead(rxBuf, sresult);
						//it->needsWrite = true;

					if ((sresult == 0) || (!keepOpen) )
					{
						db_printf(3,"Closing client socket %i on port %i. recv = %i, keepOpen = %i\n", it->socket, port, sresult, keepOpen);
						closeHandler(connections, conn, &tempset, &writeset);
						conn--;			//update for-loop status
						continue;       //need to break, to prevent using non-existing connections in code below
					}
					else if ( sresult < 0)
					{
						db_printf(5,"Error in recv(): %s\n", strerror(errno));
						closeHandler(connections, conn, &tempset, &writeset);
						conn--;			//update for-loop status
                        continue;       //need to break, to prevent using non-existing connections in code below
					}
				} 

                if (FD_ISSET(it->socket, &writeset) )		//handle writes
				{
					sresult = it->handler->writeBuf();
					//if( sresult <= 0)	it->needsWrite = false;
					if( it->handler->canClose() )
					{
						db_printf(3,"Closing client socket %i on port %i, done sending\n", it->socket, port);
						closeHandler(connections, conn, &tempset, &writeset);
						conn--;			//update for-loop status
                        continue;       //need to break, to prevent using non-existing connections in code below
					}
				}
			} //for j=0..all clients
		} //select call succesfull
	} //while !stop
	
	db_printf(1,"Closing server on port %i\n", port);

	//close server connection
	closesocket(ListenSocket);
	return 0;
}

