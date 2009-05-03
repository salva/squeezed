
#ifndef _TCP_SERVER_HPP_
#define _TCP_SERVER_HPP_

/**
 * \defgroup tcpServer TCP server
 * Wrapper code around low-level socket functions. To build a server,
 * a derived version of TCPserver and connectionHandler should be made.
 * Both slimProto and shoutProto use this class.
 *
 * Currently, is uses a select()-based implementation, multiple connections
 * are handled in a single thread. the processRead() function should therefore
 * not require much time (e.g. it should not block on large file-reads)
 *
 * A derived TCPserver-class can be used to pass external information (e.g. configuration data)
 * to the derived connectionHandler class.
 * The derived connectionHandler should implement a processRead() function, and can call
 * a write() to send data.
 * The derived TCP-server class should implement a newHandler() function, which generates a
 * instance of the derived connectionHandler class.
 *
 *@{
 */



#include <stdlib.h>
#include <vector>

#include "debug.h"
#include "util.hpp"	//for the buffer, which can be memory, file, or something else

#ifdef WIN32
	//#define NOMINMAX
	#include <winsock2.h>
	//#include <Ws2tcpip.h>
#else
	typedef int SOCKET;
#endif




/// Base class for non-blocking connection handlers:
class connectionHandler
{
private:
	const static size_t maxPacketSize = (1<<15);	//write out the data in small blocks, to keep concurrent connections responsive
	char localBuf[maxPacketSize];

	std::vector< nbuffer::buffer* > writeBufs;		//the current data queued to be sent out
	SOCKET socketFD;
public:
	bool closeAfterLastWrite;			//< Can be set by clients to indicate that connection is done after sending.

	connectionHandler(SOCKET socketFD):
			socketFD(socketFD),
			closeAfterLastWrite(false)
	{
	}


	size_t bytesRemaining(void)
	{
		int maxBytes = 0;
		if( writeBufs.size() > 0 )
			maxBytes = writeBufs[0]->size() - writeBufs[0]->pos();
		return maxBytes;
	}


	/// Ready to close the connection ?
	bool canClose(void)
	{
		return closeAfterLastWrite && ( writeBufs.size() == 0 );
	}

	/// Write part of the data
	/// TODO: look for a fifo implementation instead of std::vector
	///	return result of send(). if 0, write is completed, <0 means an error
	int writeBuf(void)
	{
		//don't raise signals, since they're hard to use in multi-threaded apps:
		int sendFlags = MSG_NOSIGNAL;	

	if( writeBufs.size() == 0)
			return 0;
		size_t pos   = writeBufs[0]->pos();
		size_t maxBytes = writeBufs[0]->size() - pos;
		const char *data = writeBufs[0]->ptr();
		int nSend = 0;

		if( data != NULL ) {
			//the buffer resides in memory, just send it out:
			//db_printf(1,"(M) sending %lu bytes to socket %i, ", maxBytes, socketFD);
			nSend = send( socketFD, data + pos, maxBytes, sendFlags);
			//db_printf(1,"(M) sent %i\n", nSend);
			if(nSend > 0)
				writeBufs[0]->seek( nSend );
		} else {
			//need to use temporary data:
			maxBytes = util::min(maxBytes, maxPacketSize);
			nSend = writeBufs[0]->read( localBuf, maxBytes);

			nSend = send( socketFD, localBuf, maxBytes, sendFlags);
			if( nSend != (int)maxBytes)
			{
				db_printf(1,"(F) sending %zu bytes to socket %i, ", maxBytes, socketFD);
				db_printf(1,"(F) sent %i\n", nSend);
				db_printf(1,"(F) buf._pos = %zu\n", writeBufs[0]->pos() );
			}
		}

		maxBytes = writeBufs[0]->size() - writeBufs[0]->pos();
		
		//Stop sending if we're done, or if we couldn't send anything:
		if( (nSend <= 0) || (maxBytes <= 0 ) )
		{
			delete writeBufs[0];
			writeBufs.erase( writeBufs.begin() );
		}
		return nSend;
	}


	//Interface which derived classes should use to send and receive:

	//this will be called by the server:
	//accept 'len' bytes, and do something with it:
	//	return false to close the connection
	virtual bool processRead(const void *data, size_t len)=0;

	//call this in a derived class to get the data out of the door:
	// the *buf will be deleted by this class, once it is written to disk
	void write( nbuffer::buffer *buf)
	{
		writeBufs.push_back( buf );
	}

};



/// Generic TCP server
class TCPserver
{
private:
	//serve function, for single connections:
	// called by the base class, after network a connection has been set-up
    //virtual void serveSingle(void)=0;

	/// a derived classes implement this, which is called once per incoming onnection
	virtual connectionHandler* newHandler(SOCKET socketFD)=0;

    int port;
	unsigned maxConnections;
	unsigned maxConQueue;    //max number of waiting connections


protected:
	//int read(void *data, size_t len);
	//int write(const void *data, size_t len);

	//check for data in receive buffer:
	//bool canRead(int timeOut=0);

public:
    bool stop;  //set to true to abort.

	TCPserver(int port, int maxConnections=10  );
	//virtual ~TCPserver();

	//run with blocking accept call (one connection at a time)
	//int run();

	/// Run with select, everyting non-blocking in a single thread
	int runNonBlock();

	int getPort(void) { return port; }
};


/**@}
 *end of doxygen group
 */


#endif
