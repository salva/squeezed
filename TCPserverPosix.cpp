

#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <fcntl.h>
#include <errno.h>

#include <memory.h>




#include "TCPserver.hpp"
#include "util.hpp"
#include "debug.h"


/*
// Temp storage for data to be written:
connectionHandler::connectionBuffer::connectionBuffer(const void *newdata, size_t len): len(len)
{
	data = new char[len];
	memcpy(data, newdata, len);
	pos = 0;
}

connectionHandler::connectionBuffer::~connectionBuffer()
{
	delete data;
}
*/


// Main server class
TCPserver::TCPserver(int port, int maxCon):
		port(port),
		maxConnections(maxCon),
		stop(false)
{
	db_printf(4,"TCPserver: port %i\n",port);
}


bool setBlocking(SOCKET socket, bool doBlock)
{
	//set to non-blocking:
	int iof = fcntl(socket, F_GETFL, 0);
	if (iof < 0)
	{
		perror("setsockopt() failed");
		close(socket);
		return false;
	}

	if(doBlock)
		fcntl(socket, F_SETFL, iof | O_NONBLOCK   );
	else
		fcntl(socket, F_SETFL, iof & (~O_NONBLOCK));
	return true;
}



//info on all clients:
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
};




SOCKET setupListenSocket(int port)
{
    struct sockaddr_in serveraddr;

    SOCKET ListenSocket = -1; // = INVALID_SOCKET;

	// Open a socket
	memset( &serveraddr, 0, sizeof(serveraddr) );
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = INADDR_ANY;
	serveraddr.sin_port = htons(port);

	ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(ListenSocket< 0)
		return -1;

	// Bind to a port
	if( bind(ListenSocket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
		return -2;

	// And listen
	if( listen(ListenSocket, SOMAXCONN) == -1)
		return -3;

	db_printf(1,"Listening on port %i\n",port);

	//set to non-blocking:
	if( !setBlocking(ListenSocket,false) )
	{
		perror("setsockopt() failed");
		close(ListenSocket);
		return -6;
	}

	return ListenSocket;
}



int TCPserver::runNonBlock()
{
    // Buffer for incoming data:
	char rxBuf[ 1<<15 ];	//receive max. 32kb at once, larger data will be handled in multiple iterations

	//struct sockaddr_in serveraddr;

    SOCKET ListenSocket = setupListenSocket(port);
	if( ListenSocket < 0)
	{
		db_printf(1,"Could not open port %i for listening\n",port);
		return -1;
	}

	//set up FD-sets for select-calls:
	fd_set tempset, readset, writeset;

	FD_ZERO(&readset);
	FD_SET(ListenSocket, &readset);
	SOCKET maxfd = ListenSocket;
	timeval tv;


	std::vector<connections_s> connections;	//TODO: use maxConnections to statically allocate


	//see  http://www.developerweb.net/forum/showthread.php?t=2933
	int sresult;
	while(!stop)
	{
		//(re-) initialize read sockets:
		memcpy(&tempset, &readset, sizeof(tempset));

		maxfd = ListenSocket;		//if connections are closing, this needs to be rebuild
		// Add all clients which have needsWrite set to the write set.
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


		// Time-out for the master-select()-call, If a write is to be initiated from another
		// part of the software (e.g. web-gui starts playing through slim protocol),
		// this value must be relatively small to keep the delay acceptable.
		tv.tv_sec =   2;
		tv.tv_usec =  0;

		sresult = select(maxfd + 1, &tempset, &writeset, NULL, &tv);

		if (sresult == 0)
		{
			db_printf(3,"select() timed out on port %i\n", port);
		}
		else if( (sresult < 0)  && (errno != EINTR) )
		{
			db_printf(1,"Error in select(): %s\n", strerror(errno));
		}
		else if (sresult > 0)
		{
			//select has got something, check if it's the server:
			if (FD_ISSET(ListenSocket, &tempset))
			{
				struct sockaddr_in peername;
				socklen_t peerlen = sizeof(peername);

				SOCKET clientSocket = accept(ListenSocket, (struct sockaddr *) &peername, &peerlen);
				if(clientSocket < 0)
				{
					db_printf(1,"accept failed on socket %d: %s\n", clientSocket, strerror(errno) );
					//close(serverSocket);
					continue;
				}

				db_printf(2,"accepting socket %i on serverport %i\n", clientSocket, port);

				//for debug, get client info
				char host[INET6_ADDRSTRLEN];
				//inet_ntop(peername.ss_family, );
				inet_ntop( AF_INET, (struct sockaddr *)&peername, host, sizeof(host)  );


				if( connections.size() < maxConnections)
				{
					FD_SET(clientSocket, &readset);
					maxfd = util::max<size_t>(maxfd, clientSocket);
					FD_CLR(ListenSocket, &tempset);	//server is handled, don't do it again in the comming for() loop

                    //set client to non-blocking mode: (if server is non-blocking, the clients will also be)
					setBlocking( clientSocket, false );

					//create a new connection handler for this:
					connectionHandler *C = newHandler(clientSocket);
					connections.push_back( connections_s(clientSocket, C) );

					//db_printf(2,"connected to %s, port %s\n", host, serv );
				}
				else
				{
					close(clientSocket);
					db_printf(2,"refused connection to %s, serverport %i\n", host, port );
				}

			} // if FD_ISSET(ListenSocket)

			//Now iterate over all clients:
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
						db_printf(3,"Closing client socket %i. recv = %i, keepOpen = %i, serverPortL %i\n", it->socket, sresult, keepOpen, port);
						FD_CLR(it->socket, &readset);	//maxFD is updated at the start of while(!stop)
						FD_CLR(it->socket, &writeset);
						close( it->socket );
						delete it->handler;
						connections.erase( connections.begin() + conn );
						conn--;			//update for-loop status
                        continue;          //need to break, to prevent using non-existing connections in code below
					}
					else 	if( sresult < 0)
					{
						printf("Error on port %i socket %i in recv(): %s\n", port, it->socket,  strerror(errno));
						FD_CLR(it->socket, &readset);	//maxFD is updated at the start of while(!stop)
						FD_CLR(it->socket, &writeset);
						delete it->handler;
						connections.erase( connections.begin() + conn );	//this also deletes any outstanding write buffers
						conn--;			//update for-loop status
                        continue;          //need to break, to prevent using non-existing connections in code below
					}
				}
				if (FD_ISSET(it->socket, &writeset) )		//handle writes
				{
					sresult = it->handler->writeBuf();
					//if( sresult <= 0)	it->needsWrite = false;

					if( it->handler->canClose() )
					{
						db_printf(3,"Closing client socket %i on port %i, done sending\n", it->socket, port);
						FD_CLR(it->socket, &readset);	//maxFD is updated at the start of while(!stop)
						FD_CLR(it->socket, &writeset);
						close( it->socket );
						delete it->handler;
						connections.erase( connections.begin() + conn );
						conn--;			//update for-loop status
                        continue;          //need to break, to prevent using non-existing connections in code below
					}
				}
			} //for j=0..all clients
		} //select call succesfull
	} //while !stop

	db_printf(1,"Closing server on port %i\n", port);

	//close server connection
	close(ListenSocket);

	return 0;
}
