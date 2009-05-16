

#include "serverShoutCast.hpp"	//needs to be first, to include winsock.h before windows.h

#include "slimIPC.hpp"
#include "util.hpp"
#include "configParser.hpp"




TCPserverShout::TCPserverShout(slimIPC *ipc, int port, int maxConnections) :
		TCPserver( port, maxConnections ),
		ipc(ipc)
{
	ipc->registerShoutServer(this);
}


connectionHandler* TCPserverShout::newHandler(SOCKET socketFD)
{
	return new shoutConnectionHandler(socketFD,ipc);
}


//---------------------- helper functions -----------------------


/// Generate html code for a directory listing:
/// can add 'play' and 'add to playlist' buttons next to the files.
std::string htmlFileList(
					std::vector<std::map<std::string,std::string> > dirList,
					std::string base,
					bool addButtons)
{
	std::ostringstream html;
	std::string out;

	size_t maxSize = 0;
	for(size_t i=0; i < dirList.size(); i++)
		maxSize = max<size_t>( maxSize, (size_t)dirList[i]["name"].size() );

	//header:
	html << "<html><head><title>Index of " << base << "</title></head>\r\n";
	//body:
	html << "<body><H2>Index of " << base << "</H2>\r\n<pre>";
	for(size_t i=0; i < dirList.size(); i++)
	{
		//TODO: iterate over all keys
		std::string name = dirList[i]["name"];
		std::string mime = dirList[i]["mime"];
		std::string spaces;
		if(addButtons)
		{
			html << "<a href=\"/dynamic/play?url=" << path::escape(base+name) << "\">",
			html << "<img src=\"" << iconPath << "/play.svg\" alt=\"[play]\" border=\"0\"></a>";

			html << " <a href=\"/dynamic/add?url=" << path::escape(base+name) << "\">",
			html << "<img src=\"" << iconPath << "/add.svg\" type=\"image/svg+xml\" alt=\"[add]\" border=\"0\"></a>";
		}
		spaces = "";
		int nrSpaces = max<int>( (int)(maxSize + 5 - name.size()), (int)(0) );
		spaces.resize( nrSpaces, ' ');
		html << " <a href=\"" << path::escape( name ) << "\">" << name << "</a>" << spaces;

		//TODO: get file type:
		if(mime.length() == 0)
		{
			char e = name[name.size() - 1 ];	//last charachter
			if( e == '/')
				mime = "directory";
			else
				mime = "file";
		}
		html << mime;

		html << "\r\n";
	}
	//end:
	html << "</pre><br><hr>\r\n</body></html>\r\n\r\n";
	return html.str();
}




//Translate relative path to realpath. Get contents or dir-listing
class nbuffer::buffer *shoutConnectionHandler::handleVFS(const char* request, const char *vfsBase, const char *realPath)
{
	nbuffer::buffer *outBuf=NULL;

	//extract relative path:
	std::string fullPath = "";
	const char *relPath = strstr(request, vfsBase ) + strlen(vfsBase);
	const char *end=NULL;	//needed later on, for debug
	if(relPath != NULL)
	{
		end = strchr(relPath,' ');
		if( end != NULL )
		{
			fullPath = path::join( realPath, string(relPath, end-relPath) );
			//fullPath.append(realPath);
			//fullPath.append(relPath, end-relPath );
		} else
			end = relPath;	//needed later on, for debug
	}
	std::string relStr(relPath, end-relPath);
	relStr = path::unescape(relStr);

	// Unescape the html-escapes.
	fullPath = path::unescape(fullPath);

	// Convert to windows style, and prevent "../../" exploits
	fullPath = path::normalize(fullPath);

	db_printf(2,"fullPath = '%s'\n", fullPath.c_str());

	//check if it's a file or directory:
	if( path::isfile(fullPath) )
	{
		//ordinary file, send the contents
		//TODO: determine file type
		sendHeader( getMime(strrchr(relStr.c_str(),'.'))  );
		outBuf = new nbuffer::bufferFile( fullPath.c_str() );
	}
	else if ( path::isdir(fullPath)   )
	{
		// Generate directory listing:
		std::string html;
		std::vector<std::map<std::string,std::string> > dirList;

		std::vector<std::string> files = path::listdir( fullPath , true );	//generate sorted file-list
		std::map<std::string,std::string> fileProps;

		for(size_t i=0; i<files.size(); i++)
		{
			std::string mime,fullName =  path::join(fullPath, files[i]);
			if( path::isdir(  fullName ) ) 
			{
				files[i].push_back('/');				//append trailing slash to directories:
				mime = "directory";
			} else {
				mime = getMime( strrchr(files[i].c_str(),'.') );
			}
			fileProps["name"] = files[i];
			fileProps["mime"] = mime;
			dirList.push_back( fileProps );
		}
		html = htmlFileList(dirList, relStr );	//generate list with relative pathnames
		sendHeader("text/html");
		outBuf = new nbuffer::bufferString( html );
	}
	else	//invalid filename
	{
		std::string errStr = "<html><head><title>Not Found</title></head>";
		errStr.append("<body><H2>File not found:</H2>\r\n");
		errStr.append( relStr );
		errStr.append("\r\n</body></html>\r\n\r\n");\

		sendHeader("text/html","404 Not Found");
		outBuf = new nbuffer::bufferString( errStr );
	}

	return outBuf;
} //handleVFS



/// Parse url parameters in the form http://127.0.0.1/stream?player=str?something=else
std::map<std::string,std::string> parseUrlParams(std::string paramStr, std::string *fname=NULL)
{
	std::map<std::string,std::string> ret;
	std::vector<std::string> pairs = pstring::split(paramStr, '?');

	//first pair is the filename, so skip that:
	if(fname != NULL)
		*fname = pairs[0];
	for(size_t i=1; i< pairs.size(); i++)
	{
		std::vector<std::string> keyVal = pstring::split( pairs[i] , '=' );
		if( keyVal.size() == 2 )
			ret[ keyVal[0] ] = keyVal[1];
	}

	return ret;	//return a copy....
}


/// Parse request, returns a buffer with the response, or NULL if there is no response
nbuffer::buffer* shoutConnectionHandler::handleGet(const char* request)
{
	nbuffer::buffer *response = NULL;
	using namespace std;

	string dataPath = ipc->getConfig("musicDB","path", ".");
	string htmlPath = ipc->getConfig("shout","htmlPath", "./html" );

	//Definition of the root file system:
	const char *dynamicEntries[] = {
		"GET / ",			//root index, will point to /html/index.html
		"GET /data/",		//browse files in music databse
		"GET /dynamic/",	//link to virtual .csv files with current statistics
		"GET /html/",		//static html data
		"GET /stream.mp3",	//current stream
	};

	enum dynName{ ROOT, DATA, DYNAMIC, HTML, STREAM, OTHER };	//names for dynamicEntries[]
	size_t dynIdx;

	//Get root index:
	for(dynIdx=0; dynIdx < array_size(dynamicEntries); dynIdx++)
	{
		if( strncasecmp(request, dynamicEntries[dynIdx], strlen(dynamicEntries[dynIdx]) ) == 0 )
			break;
	}

	db_printf(1,"SHOUT: handleGet(%llu), root dir %zu\n", (LLU)strlen(request), dynIdx );
	switch(dynIdx)
	{
		case ROOT:		// virtual root directory
			{
				using namespace std;
				vector< map<string, string> > dirList;
				map<string,string> props;

				props["name"] = "data/";		dirList.push_back( props );	//music data
				props["name"] = "dynamic/";		dirList.push_back( props );	//various files with status info
				props["name"] = "html/";		dirList.push_back( props );	//static html files that build up the user interface
				props["name"] = "stream.mp3";	dirList.push_back( props );	//the current music stream.
				std::string dirPage = htmlFileList(dirList);

				//copy the string into the buffer:
				response     = new nbuffer::bufferString( dirPage );
				sendHeader("text/html");
			}
			break;
		case STREAM:	// stream data
			{
				db_printf(1,">SHOUT: sending music stream\n");

				//get device id:
				string paramStr = pstring::split( request, ' ')[1];
				std::map<std::string,std::string> params = parseUrlParams(paramStr);

				// get song to be played
				const playList *list = ipc->getList( params["player"] );
				if( list != NULL)
				{
					vector<musicFile>::const_iterator it = list->begin() + list->currentItem;
					const char *fname = it->url.c_str();
					response     = new nbuffer::bufferFile(fname);
				} else
					response     = new nbuffer::bufferMem(NULL, 0, 0);	//send an empty file
				streamStatus = ST_START;
				sendHeader();
			}
			break;
		case DATA:		// music database
			{
				response = handleVFS(request, dynamicEntries[DATA], dataPath.c_str() );
			}
			break;
		case HTML:		// html data, for user interface
			{
				response = handleVFS(request, dynamicEntries[HTML], htmlPath.c_str() );
			}
			break;
		case DYNAMIC:	// dynamic content, playlists, queries, statistics
			{
				//parse request, break it down into a command name and it's parameters
				//TODO: determine group name
				//TODO: match SqueezeCenter: squeezecenter-7.3.2/HTML/EN/html/docs/http.html
				std::string fname;
				string paramStr = pstring::split( request, ' ')[1];
				std::map<std::string,std::string> params = parseUrlParams(paramStr, &fname);

				std::string relUrl = path::unescape( params["url"] );
				db_printf(1,"dynamic content for: %s, url = %s\n", fname.c_str(), relUrl.c_str() );

				if( fname.find("add") < fname.size() )
				{
					std::string url = path::join( dataPath, relUrl );
					std::vector<musicFile> entries = makeEntries(url);
					ipc->addToGroup( "all", entries);
				} else if( fname.find("play") < fname.size() )
				{
					std::string url = path::join( dataPath, relUrl );
					std::vector<musicFile> entries = makeEntries(url);
					if( entries.size() > 0 )
					{
						ipc->setGroup( "all", entries);
						ipc->seekList( "all", 0, SEEK_SET);
					}
				}

			}
			break;
		default:
			{
				std::string errStr = "<html><head><title>Not Found</title></head>";
				errStr.append("<body><H2>Invalid request. HELP!:</H2>\r\n");
				errStr.append(request);
				errStr.append("\r\n</body></html>\r\n\r\n");

				response   = new nbuffer::bufferString( errStr );
				sendHeader("","404 Not Found");
			}
	}
	return response;
} //handleGet()




