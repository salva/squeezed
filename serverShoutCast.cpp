

#include "httpclient.hpp"		//also includes winsock.h...

#include "serverShoutCast.hpp"	//needs to be first, to include winsock.h before windows.h

#include "slimIPC.hpp"
#include "util.hpp"
#include "configParser.hpp"


// Dynamic content for web-based user-interface:
#include "htmlIPC.hpp"




TCPserverShout::TCPserverShout(slimIPC *ipc, int port, int maxConnections):
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



//Extract path from HTML request
string htmlRequestExtractPath(const char* htmlpath,		//relative path, with html-encoding
							  const char *vfsBase,		//base of HTML path, will be stripped off
							  const char *absBasePath	//absolute start of the path on disk
							  )
{
	string fullPath;
	//Strip the base of off
	const char *relPath = strstr(htmlpath, vfsBase);
/*	const char *end=NULL;	//needed later on, for debug
	if(relPath != NULL)
	{
		relPath += strlen(vfsBase);
		end = strchr(relPath,' ');
		if( end != NULL )
		{
			fullPath = path::join( realPath, string(relPath, end-relPath) );
		} else
			end = relPath;	//needed later on, for debug
	}
	std::string relStr(relPath, end-relPath);*/
	if( relPath == NULL)
		return fullPath;
	relPath += strlen(vfsBase);

	string relStr = path::unescape(relPath);

	// Unescape the html-escapes.
	fullPath = path::unescape( path::join(absBasePath,relStr) );

	// Convert to windows style, and prevent "../../" exploits
	fullPath = path::normalize(fullPath);
	return fullPath;
}


/*
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

	//std::string fullPath = htmlRequestExtractPath(htmlpath, vfsBase, realPath);

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
*/


/* Class to keep an network connection open until an event has passed */
class bufferNotify: public nbuffer::buffer
{
public:
	bool canClose;
	string data;

private:

	class callback: public slimIPC::callbackFcn
	{
	private:
		slimIPC *ipc;
		string clientName;
		bufferNotify *parent;
        string templateFname;
    public:
		callback(slimIPC *ipc, string clientName, bufferNotify *parent, string templateFname):
				ipc(ipc),
				clientName(clientName),
				parent(parent),
                templateFname(templateFname)
		{ }

		~callback()
		{
		}

        void writeHeader(string &data, const char *httpCode, string *mimeType=NULL)
        {
            stringstream html;
            html << "HTTP/1.0 " << httpCode << "\r\n";
            if( mimeType != NULL)
                html << "Content-Type: " << mimeType << "\r\n";
            html << "Connection: close\r\n\r\n";
            data = html.str();
        }


		bool call(void)
		{
			// One call is enough:
			//ipc->unregisterCallback(this);

			//const char *fields[] = {"volume","elapsed","playstate"};
			string groupName = ipc->getGroup(clientName);
			musicFile song = ipc->getSong(groupName);
			//const playList* list = ipc->getList(clientName);

            //Check if we can get device info:
            if( ipc->getDevice( clientName, "volume" ).size() > 0)
            {
                size_t fsize;
                char *templateData = file::readfile( templateFname.c_str(), &fsize);

                if( templateData != NULL )
                {
                    //TODO: adjust mimetype base on file to serve.
                    writeHeader(parent->data, "200 OK" );

                    // Fill string with status update
		            keywordMatcherIPC keywordMatcher(ipc, clientName,  NULL, NULL, NULL, NULL);
                    dHtmlTemplate parser( &keywordMatcher );
				    string parsedData = parser.generateData( templateData );
                    free( templateData );
                    parent->data.append( parsedData);
                } else {
                    writeHeader(parent->data, "404 NOT FOUND" );
                    parent->data.append("URL '" + templateFname + "' could not be found\r\n");
                }

            } else {
                writeHeader(parent->data, "404 NOT FOUND" );
                parent->data.append("Device '" + clientName + "' is not known\r\n");
            }

            // Mark that it's ready to sent and close the connection:
			parent->canClose = true;
			return false;       //don't want another callback
		}
	};

	callback *callbackFcn;
	slimIPC *ipc;
public:

	bufferNotify(slimIPC *ipc, string clientName, string templateFname, bool doWait=true):
			ipc(ipc)
	{
		_size = 0;
		_pos  = 0;
		canClose = false;
		callbackFcn = new callback(ipc, clientName, this, templateFname);

		if( doWait )
		{
			// Register callback with ipc, to set
			// data on update.
			ipc->registerCallback(callbackFcn,clientName);
		} else {
			// Set data immediately
			callbackFcn->call();
		}
	}

	~bufferNotify()
	{
		ipc->unregisterCallback(callbackFcn);
		delete callbackFcn;
	}


	//nbuffer interface:
	char eof(void)
	{
		return canClose && (_pos >= _size);
	}

	size_t size(void)
	{
		return canClose ? data.size() : NULL;
	}

	//don't return any pointer until we are ready to send:
	const char* ptr(void)
	{
		return canClose ? data.c_str(): NULL;
	}

	int read(void *dst, size_t len)
	{
        if( !canClose) return 0;
		size_t nrCopy = util::min<int32_t>(len, _size - _pos );
		memcpy(dst, data.c_str() + _pos, nrCopy);
		_pos += nrCopy;
		return nrCopy;
	}


    /// Return true of read() will not be blocking.
    bool canRead(void)
    {
        return canClose;
    }

	int close(void)
	{	//send out the data. (which will not work anymore, because this thing
		//is closed for a reason, but still...)
		//callbackFcn->call();
		return 0;
	}

};



/// Parse request, returns a buffer with the response, or NULL if there is no response
nbuffer::buffer* shoutConnectionHandler::handleGet(const char* request)
{
	nbuffer::buffer *response = NULL;
	using namespace std;

	string dataPath = ipc->getConfig("musicDB","path", ".");
	string htmlPath = ipc->getConfig("shout","htmlPath", "./html" );

	//Definition of the root file system:
	const char *dynamicEntries[] = {
		"/ ",			//root index, will point to /html/index.html
		"/data/",		//browse files in music databse
		"/dynamic/",	//link to virtual .csv files with current statistics
		"/html/",		//static html data
		"/stream.mp3",	//current stream
	};

	enum dynName{ ROOT, DATA, DYNAMIC, HTML, STREAM, OTHER };	//names for dynamicEntries[]
	size_t dynIdx;

	//First line: "GET %path% http/?.?"
	const char *method  = request;
	const char *path    = (method==NULL)? NULL : strchr(method,' ');

	//Get root index:
	for(dynIdx=0; dynIdx < array_size(dynamicEntries); dynIdx++)
	{
		//if pathStr has a trailing space:
		//if( strncasecmp(pathStr.c_str(), dynamicEntries[dynIdx], strlen(dynamicEntries[dynIdx]) ) == 0 )
		//if pathStr is properly splitted:
		//if( strncasecmp(pathStr.c_str(), dynamicEntries[dynIdx], pathStr.size() ) == 0 )
		//with trailing space, to separate "/" from the rest:
		if( strncasecmp( path+1, dynamicEntries[dynIdx], strlen(dynamicEntries[dynIdx]) ) == 0 )
			break;
	}


	// Parse the header:
	http::parseRequestHeader hdr(request);
	//map<string,string> *params = &(hdr.headerParam);

	// Get current device:
	string currentDeviceName = path::unescape( hdr.getCookie("device") );
	if( currentDeviceName.size() == 0)
	{
		vector<string> devices = ipc->listDevices();
		if (devices.size() > 0)
			currentDeviceName = devices[0];
		//TODO: set "set-cookie:" field in the response header.
		// HTTP/1.1 200 OK
		// Set-Cookie: device=all
		// Add a form in header.html to select the devices??
	}

	// Filename without parameters:
	string fname = pstring::split(hdr.path,'?')[0];

	db_printf(3,"SHOUT: handleGet(%llu), root dir %llu (%s), device %s\n",
		(LLU)strlen(request), (LLU)dynIdx, hdr.path.c_str(), currentDeviceName.c_str() );
	switch(dynIdx)
	{
		case ROOT:		// Virtual root directory
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
		case STREAM:	// Stream data
			{
				db_printf(2,">SHOUT: sending music stream\n");

				// Get song to be played
				const playList *list = ipc->getList( hdr.getUrlParam("player") );
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
				//response = handleVFS(request, dynamicEntries[DATA], dataPath.c_str() );
				// only parse directories, not files:
				string templateDir     = path::join( htmlPath, "dirlist.html" );
				string templateDirItem = path::join( htmlPath, "dirlistItem.html" );
				size_t sizeDir, sizeDirItem;
				char *dataDir     = file::readfile( templateDir.c_str(), &sizeDir );
				char *dataDirItem = file::readfile( templateDirItem.c_str(), &sizeDirItem );
				const char *emptyStr    = "";


				keywordMatcherIPC keywordMatcher(ipc, currentDeviceName,
											     dataPath.c_str(), dynamicEntries[DATA],	//disk and http path
												 emptyStr, emptyStr);	//song,playlist info

				dHtmlVFS vfs( &keywordMatcher,
							   hdr.path, dataPath,	//The VFS-path and disk-path
							   dataDir, dataDirItem);			//the template files
				string absPath = htmlRequestExtractPath( hdr.path.c_str(), dynamicEntries[dynIdx], dataPath.c_str() );
				response = vfs.generateData( absPath );

			}
			break;
		case HTML:		// html data, for user interface
			{
				//response = handleVFS(request, dynamicEntries[HTML], htmlPath.c_str() );

				// Test the more generic system:
				string templateDir     = path::join( htmlPath, "dirlist.html" );
				string templateDirItem = path::join( htmlPath, "dirlistItem.html" );
				string templatePLitem  = path::join( htmlPath, "playlistItem.html" );
				string templateDevice  = path::join( htmlPath, "deviceListItem.html" );
				size_t sizeDir, sizeDirItem, sizePLitem, sizeDevItem;
				char *dataDir     = file::readfile( templateDir.c_str(), &sizeDir );
				char *dataDirItem = file::readfile( templateDirItem.c_str(), &sizeDirItem );
				char *dataPLitem  = file::readfile( templatePLitem.c_str(), &sizePLitem );
				char *dataDevice  = file::readfile( templateDevice.c_str(), &sizeDevItem );

				// TODO: combine all keyword matchers:
				//		htmlTemplateGroup matcher;
				//		matcher.add( htmlTemplateIPC(ipc) );
				//		matcher.add( htmlTemplateLasFM(lastfm) );
				keywordMatcherIPC keywordMatcher(ipc, currentDeviceName,
											     dataPath.c_str(), dynamicEntries[DATA],	//disk and http path
												 dataPLitem, dataDevice  					//playlist and dirlist templates
												);	//song,playlist info

				dHtmlVFS vfs( &keywordMatcher,
							   hdr.path, htmlPath,	//The VFS-path and disk-path
							   dataDir, dataDirItem);			//the template files
				string absPath = htmlRequestExtractPath( hdr.path.c_str(), dynamicEntries[HTML], htmlPath.c_str() );
				response = vfs.generateData( absPath );

				free(dataDir);
				free(dataDirItem);
				free(dataPLitem);
				free(dataDevice);
			}
			break;
		case DYNAMIC:	// dynamic content, playlists, queries, statistics
			{
				//TODO: match SqueezeCenter: squeezecenter-7.3.2/HTML/EN/html/docs/http.html
				string cmd = fname.substr( strlen(dynamicEntries[dynIdx]) );
				string relUrl = path::unescape( hdr.getUrlParam("url") );
				string idx    = hdr.getUrlParam("idx");

				//db_printf(1,"dynamic content for: %s, urlPath = %s\n", fname.c_str(), hdr.path.c_str() );

				string groupName = ipc->getGroup( currentDeviceName );
				string diskUrl = htmlRequestExtractPath( relUrl.c_str(), dynamicEntries[DATA], dataPath.c_str() );

				if( cmd == "add" )
				{
					std::vector<musicFile> entries = makeEntries(diskUrl);
					ipc->addToGroup( groupName, entries);
				}
				else if( cmd == "play" )
				{
					if( idx.size() > 0 )
					{	//seek in current playlist
						ipc->seekList( groupName, atoi(idx.c_str()), SEEK_SET);
					}
					else
					{	//replace playlist, then start playing
						// replace the VFSbase with the absolute disk base path:

						std::vector<musicFile> entries = makeEntries(diskUrl);
						if( entries.size() > 0 )
						{
							ipc->setGroup( groupName, entries);
							ipc->seekList( groupName, 0, SEEK_SET);
						}
					}
				}
				else if( cmd == "remove" )
				{
					//int i = atoi( idx.c_str() );
					//ipc->
				}
				else if( cmd == "control" )
                {
                    string subCmd  = hdr.getUrlParam("action");
					string subValue= hdr.getUrlParam("value");
                    ipc->setDevice( currentDeviceName, subCmd, subValue );
                }
				else if( cmd == "notify" )
				{
                    // Server this file upon return:
                    string respFile     = path::join( htmlPath, hdr.getUrlParam("url") );

                    // Keep the connection open until an update has occured:
					response = new bufferNotify(ipc, currentDeviceName, respFile, true);
                    // Don't accept any incoming data anymore.
                    this->isReadBlocking = true;
				}

				//TODO: send redirect response, to force updating of the browser
                //Need to find the origin of the call, is this in the http 'referrrer' header?
				/*std::ostringstream html;
				html << "HTTP/1.x 302 Moved Temporarily\r\n";
				html << "Location: http://127.0.0.1:9000" << hdr.path << "\r\n";
				html << "Connection: close\r\n\r\n";
				response   = new nbuffer::bufferString( html.str() );
				*/
			} //case DYNAMIC
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




