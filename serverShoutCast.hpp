
#ifndef _serverShoutcast_hpp_
#define _serverShoutcast_hpp_


/**
 * \defgroup shoutProto Basic web-server / shoutcast implementation
 * Very basic webserver.
 * This both streams the actual music data to the clients, and provides a
 * web interface.
 * It is connected to the slim-protocol using the slimIPC class. This is needed
 * to determine which file to stream to the clients. It provides the following
 * root directory entries:
 * data/		link to the music database.
 * dynamic/		dynamic content, information on the slim-protocol status (playlists, etc.)
 * stream.mp3	the current song of the current playlist
 * html/		files located in the html directory, are served without any parsing
 *
 * The HTML spec. is at:
 * http://www.rfc-editor.org/rfc/rfc2616.txt
 *@{
 */


#include <string>
#include <iostream>
#include <sstream>

//for directory browsing:
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "TCPserver.hpp"	//needs to be first, to include winsock.h before windows.h

#include "slimIPC.hpp"
#include "util.hpp"

#include "fileInfo.hpp"
#include "debug.h"

class configParser;

//msvc madness:
#ifdef WIN32
#pragma warning (disable: 4996)
#endif



//--------------------------- Local types ------------------------------------

/*struct serverEntry {
	const char *name;
	const char *url;
	int port;
};

//--------------------------- Constants ---------------------------------------

//copied from http://www.shoutcast.com/
// not used yet
const serverEntry shoutStreams[] = {
	{"Radio ZET1", "http://91.121.179.221", 8052},
	{"Radio ZET2", "http://217.20.116.125", 8050},
	{"Radio ZET3", "http://213.251.140.82", 8050},
	{"Europa FM Romania", "http://89.238.252.138", 7000},
};
*/

const char serverString[] = "Squeezed 0.1";
//const char serverString[] = "SlimServer (6.5.4 - 12568)";
const char iconPath[] = "/html/icons";


//--------------------------- Internal functions -----------------------------


///Handles a socket-connection using the shoutserver protocol.
///Is provides a basic webserver, with some special additions for the slim protocol
class shoutConnectionHandler: public connectionHandler
{
private:
	slimIPC *ipc;	//inter-proces-data, gives acces to clients and the music database

	char bufferIn[1<<10];	//input buffer. Should be large enough to handle a single GET-request
	size_t bufferInPos;
	char sendBuffer[1<<16];	//output buffer.

	enum {
		ST_STOP,
		ST_START,
		ST_PLAY,
	} streamStatus;
	nbuffer::buffer *streamData;


public:
	shoutConnectionHandler(SOCKET socketFD, slimIPC *ipc ) :
			connectionHandler(socketFD),
			ipc(ipc)
	{
		streamData = NULL;
		bufferInPos = 0;
		streamStatus = ST_STOP;	//status of message sending
	}


	//note that there's a non-blocking write function in the base class

	bool processRead(const void *data, size_t len)
	{
		bool keepConnection = true;

		//TODO: use htons or something, to make it work on little-endian systems:
		uint32_t eoh4a = 0x0d0a0d0a;	//definition of end of request.
		uint32_t eoh4b = 0x0a0d0a0d;	//too lazy to use htons, this is a bit wrong
		uint32_t eoh2a = 0x0d0d;		//shouldn't happen, but who knows..
		uint32_t eoh2b = 0x0a0a;		//shouldn't happen, but who knows..

		//scan for termination code:
		const char *req = (const char*)data;
		size_t endIdx;
		uint32_t h4=0;	//history
		uint32_t h2=0;
		for(endIdx=0; endIdx < len; endIdx++)
		{
			h4 <<= 8; h4 |= req[endIdx];
			h2 <<= 8; h2 |= req[endIdx];
			if(  (h4==eoh4a) || (h4==eoh4b) || (h2==eoh2a) || (h2==eoh2b) )
					break;
		}
		bool complete = (endIdx < len);

		//add data to the input buffer:
		size_t maxCopy = util::min( sizeof(bufferIn) - bufferInPos, len);
		if (maxCopy < len)
			db_printf(0,"WARNING: dropping data\n");	//not so nice to do this

		memcpy(bufferIn + bufferInPos, data, maxCopy);
		bufferInPos += maxCopy;

		if(complete)
		{
			int msgLen = bufferInPos;
			bufferIn[msgLen-1] = 0;		// force zero termination, this overwrite the last character of the message
			nbuffer::buffer *response = NULL;

			// process the data:
			if( msgLen > 4)				// skip some remaining end-of-line characters
				response = handleGet(bufferIn);
			if( response != NULL)
				write( response );		// write will delete response;
			closeAfterLastWrite = true;	// done after write, close the connection
			bufferInPos = 0;
		}
		return keepConnection;
	}


	int sendHeader(std::string contentType="audio/mpeg", std::string code="200 OK")
	{
		std::ostringstream html;
		html << "HTTP/1.0 " << code << "\r\n";
		html << "Server: " << serverString << "\r\nConnection: close\r\n";
		html << "Content-Type: " << contentType << "\r\n\r\n";

		std::string hdr(html.str());
		int strLen = hdr.size();
		const char *data =  hdr.c_str();
		db_printf(5,">SHOUT: sending header of %i bytes. Last character: '%i'\n", strLen, data[strLen-1] );
		write( new nbuffer::bufferString( hdr ) );
		return strLen;
	}

	/// Main handler of complete messages
	nbuffer::buffer *handleGet(const char* request);

	/// Translate relative path to realpath. return dir-listing or file
	class nbuffer::buffer *handleVFS(const char* request, const char *vfsBase, const char *realPath);
};


//--------------------------- External / Interface ---------------------------


/// The base server class
///	this gives every connectionhandler acces to both *ipc and the SOCKET data.
class TCPserverShout : public TCPserver
{
private:
	slimIPC *ipc;
public:
	TCPserverShout(slimIPC *ipc, int port=9000, int maxConnections=10);

	connectionHandler* newHandler(SOCKET socketFD);
};





//--------------------------- Handling of get requests: ------------------------



/// Generate html code for a directory listing:
/// can add 'play' and 'add to playlist' buttons next to the files.
std::string htmlFileList(
					std::vector<std::map<std::string,std::string> > dirList,
					std::string base="",
					bool addButtons=true);


/**@}
 *end of doxygen shoutProto-group
 */


#endif
