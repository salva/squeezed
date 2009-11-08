/* Generate all keywords
 * for the IPC
 */


#include "htmlTemplate.hpp"
#include "slimIPC.hpp"


class keywordMatcherDevice: public keywordMatcher
{
	const string *devName;
	slimIPC *ipc;
	vector<string> kw;
public:
	keywordMatcherDevice(slimIPC *ipc, const string& devName):
	  devName(&devName),
	  ipc(ipc)
	{
		kw.push_back("device.name");
	}

  	vector<string> keyWords(void)
	{
		return kw;
	}

	string replace(string keyword)
	{
		string out;
		if (keyword == "device.name")
			out = *devName;
		else if (keyword == "device.group")
			out = ipc->getGroup(*devName);
		return out;
	}
};



// Generate current playlist keyword
class keywordMatcherSong: public keywordMatcher
{
private:
	//slimIPC* ipc;
	//string   clientName;
	vector<string> kw;
	const musicFile *current;
	int playListIdx;
public:
	keywordMatcherSong(const musicFile& current, int playListIdx=-1 ):
			current(&current),
			playListIdx(playListIdx)
	{
		//groupName = ipc->getGroup(clientName);
		kw.push_back("title");		// current song info
		kw.push_back("album");
		kw.push_back("artist");
		kw.push_back("url");
        kw.push_back("length");
		kw.push_back("listidx");
	}


	vector<string> keyWords(void)
	{
		return kw;
	}


	string replace(string keyword)
	{
		char tmp[20];
		string out;
		if (keyword == "title")
			if( current->title.size() > 0 )
				out = current->title;
			else
				out = path::split(current->url).back();
		else if (keyword == "album")
			out = current->album;
		else if (keyword == "artist")
			out = current->artist;
		else if (keyword == "url")
			out = current->url;
        else if (keyword == "length")
		{
			//int sec = current->length % 60;
			//int min = current->length / 60;
			//sprintf(tmp, "%i:%02i", min,sec);
			sprintf(tmp, "%i", current->length);
            out = tmp;
		}
		else if (keyword == "cover")
		{
			string coverName = path::join( current->url, "cover.jpg" );
			if( path::isfile( coverName ) )
			{
				//split of the absolute path, replace it by the relative one...
			}
		}
		else if (keyword == "listidx")
		{
			char tmp[20];
			sprintf(tmp, "%i", playListIdx);
			out = tmp;
		}
		return out;
	}
};



// Generate data for all IPC keywords
class keywordMatcherIPC: public keywordMatcher
{
private:
	const char	*templatePLitem;		// html template for single playlist item
	const char	*templateDevice;

	const char *diskBasePath;			// base paths for data, to convert pathnames
	const char *httpBasePath;

	slimIPC		*ipc;

	string groupName;
	string clientName;
	//keywordMatcherSong *kwSong;
	vector<string> kw;
	musicFile currentSong;
public:
	keywordMatcherIPC(slimIPC *ipc, string clientName, 
						const char *diskBasePath, const char *httpBasePath,
						const char *templatePLitem, const char *templateDevice):
		templatePLitem(templatePLitem),
		templateDevice(templateDevice),
		diskBasePath(diskBasePath),
		httpBasePath(httpBasePath),
	    ipc(ipc),
		clientName(clientName)
	{
		//kwSong = new keywordMatcherSong(ipc, clientName);
		groupName   = ipc->getGroup(clientName);
		currentSong = ipc->getSong(groupName);

		kw.push_back("title");		// current song info
		kw.push_back("album");
		kw.push_back("artist");
		kw.push_back("url");
		kw.push_back("playlist.list");	// is handled by a keywordMatcherSong()
        kw.push_back("playlist.index");
		kw.push_back("device.list");
        kw.push_back("config.sections");
	}

	~keywordMatcherIPC()
	{
		//delete kwSong;
	}

	//Return vector of keywords handler by this class
	vector<string> keyWords(void)
	{
		return kw;
	}

	string replace(string keyword)
	{
		string out;
		string splaylist = "playlist.";
		string sdevice   = "device.";
		string sconfig   = "config.";

		//TODO: for each of the sections, generate a hash of the current status, such
		// that "/dynamic/notify?url=" can be used to determine what to update

        if( pstring::startswith(keyword, splaylist ) )
        {
			int checksum;
            const playList* currentList = ipc->getList(clientName, &checksum);
			char tmp[20];

			string field = keyword.substr( 9 );

			if( currentList == NULL)
			{
				//don't do anything
			} 
			else if (field == "index")
            {
                sprintf(tmp, "%llu", (LLU)currentList->currentItem);
                out = tmp;
            }
			else if( field == "size" )
			{
				sprintf(tmp, "%llu", (LLU)currentList->items.size());
                out = tmp;
			}
			else if( field == "repeat" )
			{
				sprintf(tmp, "%i", currentList->repeat );
                out = tmp;
			}
			else if( field == "checksum" )
			{
				sprintf(tmp, "%x", checksum );
				out = tmp;
			}
		    else if (field == "list")	//special case
		    {
			    if( currentList != NULL )
			    {
				    for(size_t i=0; i< currentList->items.size(); i++)
				    {
					    keywordMatcherSong matchSong( currentList->items[i], i );
					    out.append( dHtmlTemplate::generateData( templatePLitem, matchSong ) );
				    }
			    }
		    }
        }
        else if( pstring::startswith(keyword, sdevice ) )
        {
			string field = keyword.substr( 7 );
		    if (field == "list")
		    {
			    vector<string> devList = ipc->listDevices();
			    for( vector<string>::iterator it=devList.begin(); it != devList.end(); it++)
			    {
				    keywordMatcherDevice matchDevice( ipc, *it );
				    out.append( dHtmlTemplate::generateData( templateDevice, matchDevice ) );
                }
			} else {
				out = ipc->getDevice( clientName, field);
			}
        }
        else if( pstring::startswith(keyword, sconfig ) )
        {
            if( keyword == "config.sections")
            {
                vector<string> sections = ipc->getConfigSections();
                for( size_t i=0; i < sections.size(); i++)
                {
                    out.append( sections[i] );
                    out.append( "," );
                }
                out.erase( out.size() - 1 );    //remove trailing comma
            }
            //TODO: have dynamic keywords:
            //this will require java-script-parsing to present a UI.
            //(and many connections to build up a single page with all options...)
            // config.section.xxx   // get list of items in section 'xxx'.
            // config.val.xxx.yyy   // get value for
            // or: have the caller extract parameters: #keyword:params#, such that
            // param can be loaded as template file..
		}
		else //if keyword=...
		{	//handle current song info
			//TODO: also send path info, to search for cover.
			keywordMatcherSong matchSong( currentSong );
			out = matchSong.replace( keyword );
		}
		return out;
	}
};

