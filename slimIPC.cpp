

#ifdef WIN32
	#include <Winsock2.h>
#else
	#include <netdb.h>	//to lookup shout streams
#endif


#include "configParser.hpp"
#include "serverShoutCast.hpp"
#include "slimProto.hpp"


#include "slimIPC.hpp"


//----------------- small helper functions ---------------------


void musicFile::clear(void)
{
	format = '?';
	nChannels = 0;
	nBits = 0;
	sampleRate = 0;
	gainTrack = 0;
	gainAlbum = 0;
	ip		  = 0;
	port	  = 0;
	length    = 0;
}

musicFile::musicFile(void)
{
	clear();
}


musicFile::musicFile(const char *fname, uint16_t port)
{
	clear();
	setFromFile(fname,port);
}


bool musicFile::setFromFile(const char *fname, uint16_t defaultPort)
{
	const size_t httpLen = 7; // "http://"
	if( strncasecmp(fname, "http://", httpLen ) == 0 )
	{
		//handle as shoutcast stream:
		const char *ipStr   = strstr(fname,"//") + 2;
		if( ipStr == NULL )
			return false;

		const char *portStr = strrchr(fname+httpLen,':');	//also, end of ipStr
		const char *fileStr = strchr(ipStr,'/');
		int port = 80;		//default port

		if( portStr != NULL)
			port = atoi( portStr + 1 );
		else
			portStr = strchr(ipStr, '/' );
		if( portStr == NULL )
			portStr = fname + strlen(fname);

		if( fileStr != NULL)
			url = fileStr;

		std::string ip(ipStr, portStr - ipStr );
		printf("looking up: '%s'\n", ip.c_str() );
		//lookup an ip-address:
		//in_addr_t address;
		in_addr address;
		if( isalpha( ip[0] ) )
		{
			struct hostent *remoteHost = gethostbyname( ip.c_str() );
#ifdef WIN32
			address.s_addr = *(ULONG*)remoteHost->h_addr_list[0];	//address = *((in_addr * )remoteHost->h_addr);
#else
			address.s_addr = *(in_addr_t*)remoteHost->h_addr_list[0];	//address = *((in_addr * )remoteHost->h_addr);
#endif
		} else {
			//in_addr in_address;	address = in_address.s_addr;
			address.s_addr = inet_addr( ip.c_str() );
			//inet_aton ( ip.c_str(), &address );
		}

		uint8_t *tmp = (uint8_t*)(&address.s_addr);
		db_printf(3, "%s -> ip-address: %i.%i.%i.%i\n", ip.c_str(), tmp[0], tmp[1], tmp[2], tmp[3] );

		this->ip   = (uint32_t)address.s_addr;
		this->port = port;

		format = 'm';
		nChannels = 2;
		return true;
	}
	else
	{
		// Assume it's a (local) file:
		fileInfo finfo(fname);

		if( finfo.isAudioFile )
		{
			title  = finfo.tags["title"];
			artist = finfo.tags["artist"];
			album  = finfo.tags["album"];

			format = '?';
			if( finfo.mime == "audio/mpeg")
				format = 'm';
			if( finfo.mime == "audio/flac")
				format = 'f';
			nChannels	= finfo.nrChannels;
			nBits		= finfo.nrBits;
			sampleRate	= finfo.sampleRate;
			length      = finfo.length;
			gainTrack	= finfo.gainTrack;
			gainAlbum	= finfo.gainAlbum;

			url			= fname;
			ip			= 0;	//localhost
			port 		= defaultPort;
		}
		return finfo.isAudioFile;
	}
}



// init from a comma separated list of values
/*musicFile::musicFile(string csv, uint16_t port)
{
	*//*int i=0;
	vector<string> items = pstring::split(csv, ',');
	if( items.size() >= 9 )
	{
		title  = items[i++];
		artist = items[i++];
		album  = items[i++];
		format = items[i++][0];
		nChannels = atoi( items[i++].c_str() );
		nBits	  = atoi( items[i++].c_str() );
		sampleRate= atoi( items[i++].c_str() );
		length    = atoi( items[i++].c_str() );
		url = items[i++];
	} *//*
	url = csv;
	setFromFile( url.c_str() );

	this->port = port;
	ip = 0;
}*/


musicFile::operator string()
{
	stringstream out;
//	out << title  << "," << artist << "," << album << ",";
//	out << format << "," << nChannels << "," << nBits << "," << sampleRate << ",";
//	out << length << ",";
	out << url;
	return out.str();
}



/// Make playlist entries from filename/directory name
std::vector<musicFile> makeEntries(string url)
{
	std::vector<musicFile> entries;
	url = path::normalize(url);
	if( path::isdir(url) )
	{
		std::vector<string> items = path::listdir( url , true );	//list files, sorted
		for(size_t i=0; i< items.size(); i++)
		{
			string fullFname = path::join(url,items[i]);
			entries.push_back( musicFile( fullFname.c_str()) );
			//only allow valid music files:
			if( entries.back().url.size() < 1 )
				entries.pop_back();
		}
	} else {
		entries.push_back( musicFile( url.c_str() ) );
		//only allow valid music files:
		if( entries.back().url.size() < 1 )
			entries.pop_back();
	}
	return entries;
}


/// Generate playlist entries from a sub-selection of a query result:
std::vector<musicFile> makeEntries( musicDB *db, dbQuery& query, size_t uniqueIndex )
{
	std::vector<musicFile> entries;

	dbField field = query.getField();
	dbEntry matchEntry = (*db)[ query.uIndex(uniqueIndex) ];
	string  matchStr   =  matchEntry.getField(field);
	//re-search in results, to get everything matching the unique index:
	dbQuery	matches(db, query, field, matchStr.c_str() );

	for( std::vector<uint32_t>::iterator it= matches.begin(); it != matches.end(); it++)
	{
		dbEntry entry = (*db)[ *it ];
		string url =  path::join(path::join(db->getBasePath(),entry.relPath),  entry.fileName);
		musicFile f( url.c_str() );
		entries.push_back( f );
	}
	return entries;
}



musicFile playList::get(size_t index) const
{
	musicFile ret;
	if( index < items.size() )
		ret = items[index];
	return ret;
}

//----------------- external interface -------------------------

slimIPC::slimIPC(musicDB *db, configParser	*config): db(db)
{
	this->config = config;
	playListFile = path::join( config->get("config","path",".") , "slimIPC.ini" );
	this->slimServer  = NULL;
	this->shoutServer = NULL;

	pthread_mutex_init( &mutex.group, NULL);

	//Load all playlists from disk
	load( config->get("shout","port", 0)  );

	if(  group.size() < 1)
	{
		//for debug: initialize the playlist:
		playList plist;

		//fill the default playlist with some data:
		const char *Q = "w";
		dbQuery qTitle( db, DB_TITLE, Q );
		for( std::vector<uint32_t>::iterator it= qTitle.begin(); it != qTitle.end(); it++)
		{
			dbEntry entry = (*db)[ *it ];
			string url =  path::join(path::join(db->getBasePath(),entry.relPath),  entry.fileName);
			//std::auto_ptr<fileInfo> info = getFileInfo( entry.fileName.c_str() );
			musicFile f( url.c_str() );

			//these are also set by the musicFile constructor:
			f.artist = entry.artist;
			f.album  = entry.album;
			f.title  = entry.title;

			plist.items.push_back( f );
		}
		group["all"] = plist;
		save( );
	}
}



slimIPC::~slimIPC(void)
{
	pthread_mutex_destroy( &mutex.group );
}


// load status from disk
void slimIPC::load( int shoutPort )
{
	configParser config( playListFile.c_str() );

	vector<string> groupNames = pstring::split(config.get("general","groupNames", "" ),',');

	for( size_t i=0; i< groupNames.size(); i++)
	{
		if( groupNames[i].size() < 1)
			continue;
		string groupName = "group." + groupNames[i];
		int nr = 0;
		char itemName[80];
		sprintf(itemName, "item%03i", nr);
		while( config.hasOption(groupName, itemName ) )
		{
			string item =  config.get(groupName, itemName );
			musicFile f = musicFile( item.c_str() , shoutPort );
			group[groupNames[i]].items.push_back( f );
			sprintf(itemName, "item%03i", ++nr);
		}
		group[groupNames[i]].currentItem = (int)config.get(groupName, "position" , 0 );
	}
}



void slimIPC::save( void )
{
	string groupNames;
	configParser config;

	map<string, playList>::iterator itGroup;
	for(itGroup = group.begin(); itGroup != group.end(); itGroup++)
	{
		groupNames += itGroup->first + ",";
		string groupName = "group." + itGroup->first;
		playList &list = itGroup->second;
		char itemName[80];

		config.set(groupName, "position", configValue(list.currentItem) );
		for(size_t i=0; i < list.items.size(); i++)
		{
			sprintf(itemName, "item%03llu", (LLU)i);
			config.set(groupName, itemName, configValue(list.items[i]) );
		}
	}
	groupNames.erase( groupNames.size()-1 );	//remove trailing comma
	config.set("general", "groupNames", groupNames);

	config.write( playListFile.c_str() );
}



int slimIPC::getShoutPort(void)
{
	return shoutServer->getPort();
}



// Device control, for slimProto
int slimIPC::addDevice(string clientName, client* dev)	//add a device to the current list
{
	//TODO: get group-name from a configuration file.
	playList* group = &(this->group["all"]);
	devices.push_back( dev_s(clientName, dev, group  ) );
	return devices.size();
}


// forget about it
int slimIPC::delDevice(string clientName)
{
	std::vector<dev_s>::iterator it = devByName( clientName );
	if( it != devices.end() )
	{
		//store current setting:
		string sectionName = "player." + clientName;
		config->set( sectionName, "volume", *(it->device->volume()) );
		config->set( sectionName, "group" , it->device->currentGroup() );
		config->set( sectionName, "brightness" , *it->device->brightness() );
		config->write( );

		devices.erase( it );
	}
	return devices.size();
}



// Get group name for a given client
string slimIPC::getGroup(string clientName)
{
	string groupName;
	size_t idx;
	for( idx=0; idx < devices.size(); idx++)
		if( devices[idx].name == clientName )
			break;
	if( idx < devices.size() )
		groupName = devices[idx].name;
	return groupName;
}



// set a new playlist
int slimIPC::setGroup(string groupName, vector<musicFile>& items)
{
	if( group.find(groupName) == group.end() )
		return -1;
	playList *list = &group[groupName];
	list->items.assign( items.begin(), items.end() );

	list->currentItem = 0;

	save();		//store status to disk
	return list->items.size();
}


// add items to a playlist, default to add at end
int slimIPC::addToGroup(string groupName, vector<musicFile>& items, int offset)
{
	if( group.find(groupName) == group.end() )
		return -1;
	playList *list = &group[groupName];
	vector<musicFile>::iterator it = list->items.end();
	if( (offset >= 0) && ( (size_t)offset < list->items.size() ) )
		it = list->items.begin() + offset;

	list->items.insert( it, items.begin(), items.end() );

	save();		//store status to disk
	return list->items.size();
}


int slimIPC::seekList(string groupName, int offset, int origin, bool stopCurrent)
{
	//check if group is known:
	if( group.find(groupName) == group.end() )
		return -1;
	playList *list = &group[groupName];
	size_t newIndex;

	switch(origin)
	{
	case SEEK_SET:
		newIndex = offset;
		break;
	case SEEK_END:
		newIndex = list->items.size() - offset;
		break;
	case SEEK_CUR:
		newIndex = list->currentItem + offset;
		break;
	default:
		newIndex = list->currentItem;
	}
	bool isAtEnd = (newIndex >= list->items.size());
	newIndex = util::clip<size_t>( newIndex, 0, list->items.size()-1);

	if( (list->repeat) && ( isAtEnd ))
	{
		newIndex = 0;
		isAtEnd  = false;
	}

	//send out play commands:
	//if( list->currentItem != newIndex)
	{
		list->currentItem = newIndex;
		//tell all players to start a new song:
		for(size_t i=0; i< devices.size(); i++)
			if( devices[i].group == list)
			{
				//stop current stream. for gapless playback, this should not be done at the
				// end of a song
				if( stopCurrent )
					devices[i].device->stop();
				//start a new song, unless we've reached the end of the playlist.
				if( !isAtEnd  )
					devices[i].device->play();
			}
	}
	return list->currentItem;
}



// Get current song
musicFile slimIPC::getSong(string groupName)
{
	musicFile f;
	if( group.find(groupName) != group.end() )
		f = group[groupName].items[ group[groupName].currentItem ];
	return f;
}
