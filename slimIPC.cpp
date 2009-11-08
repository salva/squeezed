

#ifdef WIN32
	#include <Winsock2.h>
#else
	#include <netdb.h>	//to lookup shout streams
#endif

#include "socket.hpp"

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

void printMutexError(int err)
{
	if(err==0)
		return;

	fprintf(stderr, "Mutex error %i: %s\n", err, strerror(err) );
#ifdef WIN32
	fprintf(stderr,"\tWin32 error-code: %i\n", GetLastError());
#endif
	assert(err==0);
}



slimIPC::slimIPC(musicDB *db, configParser	*config): db(db)
{
	this->config = config;
	playListFile = path::join( config->get("config","path",".") , "slimIPC.ini" );
	this->slimServer  = NULL;
	this->shoutServer = NULL;

#ifdef WIN32
	pthread_mutexattr_t *mattr = NULL;
#else
	pthread_mutexattr_t mattr_storage;
	pthread_mutexattr_t *mattr = &mattr_storage;
	pthread_mutexattr_init( mattr );
	//Allow the same thread to access the mutex multiple time:
	pthread_mutexattr_settype( mattr, PTHREAD_MUTEX_RECURSIVE);
	//pthread_mutexattr_settype( &mattr, PTHREAD_MUTEX_ERRORCHECK);
#endif
	pthread_mutex_init( &mutex.group, mattr);
	pthread_mutex_init( &mutex.others, mattr);
	pthread_mutex_init( &mutex.client, mattr);
	pthread_mutex_init( &mutex.client, mattr);
	pthread_mutex_init( &mutex.config, mattr);

#ifndef WIN32
	pthread_mutexattr_destroy( mattr );
#endif

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
	pthread_mutex_destroy( &mutex.others );
	pthread_mutex_destroy( &mutex.client );
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
	int e = pthread_mutex_lock( &mutex.client );
	if( e!=0)	printMutexError(e);

	//TODO: get group-name from a configuration file.
	playList* group = &(this->group["all"]);
	devices.push_back( dev_s(clientName, dev, group  ) );

	e = pthread_mutex_unlock( &mutex.client );
	if( e!=0)	printMutexError(e);
	return devices.size();
}


// forget about it
int slimIPC::delDevice(string clientName)
{
	int e = pthread_mutex_lock( &mutex.client );
	if( e!=0)	printMutexError(e);

	std::vector<dev_s>::iterator it = devByName( clientName );
	if( it != devices.end() )
	{
		//store current setting:
		string sectionName = "player." + clientName;
		config->set( sectionName, "volume", it->device->volume() );
		config->set( sectionName, "group" , it->device->currentGroup() );
		config->set( sectionName, "brightness" , *it->device->brightness() );
		config->write( );

		devices.erase( it );
	}

	e = pthread_mutex_unlock( &mutex.client );
	if( e!=0)	printMutexError(e);
	return devices.size();
}



void slimIPC::setDevice(const string& devName, const string& cmd, const string& cmdParam)
{   //'play','pause','stop'
	int e = pthread_mutex_lock( &mutex.client );
	if( e!=0)	printMutexError(e);

    std::vector<dev_s>::iterator dev = devByName(devName);
    string groupName = this->getGroup(devName);

    if( dev == devices.end() )
        return;

    if (cmd == "play" )			// TODO: This logic should be moved to whomever is calling this
		if( dev->device->isPlaying() )
			dev->device->pause();
		else
			dev->device->play();
    else if(cmd == "pause")		// TODO: This logic should be moved to whomever is calling this
		if( dev->device->isPlaying() )
			dev->device->pause();
		else
			dev->device->unpause();
    else if(cmd == "stop")
        dev->device->stop();
    else if(cmd == "volume")
        dev->device->setVolume( atoi(cmdParam.c_str()) );
    else if(cmd == "next")
        seekList(groupName, +1, SEEK_CUR, true);
    else if(cmd == "prev")
        seekList(groupName, -1, SEEK_CUR, true);
	else if(cmd == "repeat")
		dev->group->repeat = !(dev->group->repeat);

	e = pthread_mutex_unlock( &mutex.client );
	if( e!=0)	printMutexError(e);
}



std::string slimIPC::getDevice(const string& devName, const string& field)
{
	char tmp[20];
	std::vector<dev_s>::iterator dev = devByName(devName);
    string groupName = this->getGroup(devName);

	if( dev == devices.end() )
        return string();

	if( field == "volume" )
		sprintf(tmp, "%i", dev->device->volume());
	else if( field == "elapsed" )
	{
		int msec = dev->device->elapsed();
		sprintf(tmp, "%i", msec / 1000);
		//int sec = (msec/1000) % 60;
		//int min = (msec/(1000*60));
		//sprintf(tmp, "%i:%02i", min,sec);
	}
	else if( field == "playstate")
	{
		if( dev->device->isPlaying() )
			sprintf(tmp,"play");
		else
			sprintf(tmp,"pause");
	}
	return tmp;	//this will create string with a copy of 'tmp'
}



// Get group name for a given client
string slimIPC::getGroup(string clientName)
{
	playList *group = NULL;
	string groupName;
	size_t idx;
	for( idx=0; idx < devices.size(); idx++)
		if( devices[idx].name == clientName )
			break;
	if( idx < devices.size() )
		group = devices[idx].group;

	// lookup group name:
	map<string, playList>::iterator it;
	for( it=this->group.begin(); it != this->group.end(); it++)
	{
		if( &(it->second) == group )
			groupName = it->first;
	}
	return groupName;
}



// Set a new playlist
int slimIPC::setGroup(string groupName, vector<musicFile>& items)
{
	int e = pthread_mutex_lock( &mutex.group );
	if( e!=0)	printMutexError(e);

	if( group.find(groupName) == group.end() )
		return -1;
	playList *list = &group[groupName];
	list->items.assign( items.begin(), items.end() );

	list->currentItem = 0;

	save();		//store status to disk

	e = pthread_mutex_unlock( &mutex.group );
	if( e!=0)	printMutexError(e);
	return list->items.size();
}



// add items to a playlist, default to add at end
int slimIPC::addToGroup(string groupName, vector<musicFile>& items, int offset)
{
	int e= pthread_mutex_lock( &mutex.group );
	if( e!=0)	printMutexError(e);

	if( group.find(groupName) == group.end() )
		return -1;
	playList *list = &group[groupName];
	vector<musicFile>::iterator it = list->items.end();
	if( (offset >= 0) && ( (size_t)offset < list->items.size() ) )
		it = list->items.begin() + offset;

	list->items.insert( it, items.begin(), items.end() );

	save();		//store status to disk

	e = pthread_mutex_unlock( &mutex.group );
	if( e!=0)	printMutexError(e);
	return list->items.size();
}



/// Get current playlist for a device:
/// TODO: make a copy, else it's not thread-safe.
const playList* slimIPC::getList(string clientName, int *checksum)
{
	const playList *ret = NULL;
	std::vector<dev_s>::iterator it = devByName( clientName );
	if( it != devices.end() )
		ret	= it->group;

	/// Return a checksum of the playlist, to quickly check for changes.
	if( checksum != NULL )
	{
		if( ret == NULL )
			*checksum = 0;
		else
		{
			util::fletcher_state_t state = util::fletcher_init;
			//this contains current index, repeat mode:
			util::fletcher_update( (uint8_t*)ret, sizeof(*ret), &state );
			for( size_t i=0; i < ret->items.size(); i++)
			{
					util::fletcher_update( (uint8_t*)&(ret->items[i]), sizeof( musicFile ), &state );
			}
			*checksum = util::fletcher_finish( state );
		}
	}
	return ret; //->items;
}



void slimIPC::setRepeat(string groupName,bool state)
{
	playList *targetGroup = &group[groupName];

	bool *v = &group[groupName].repeat;
	if( *v != state )
	{
		*v = state;
		for(size_t i=0; i< devices.size(); i++)
		{
			if( devices[i].group == targetGroup )
				notifyClientUpdate( devices[i].device );
		}
	} // *v != state
}



bool slimIPC::getRepeat(string groupName)
{
	return group[groupName].repeat;
}



int slimIPC::seekList(string groupName, int offset, int origin, bool stopCurrent)
{
	//check if group is known:
	if( group.find(groupName) == group.end() )
		return -1;

	int e = pthread_mutex_lock( &mutex.group );
	if( e!=0)	printMutexError(e);

	playList *list = &group[groupName];
	int newIndex;	//need to be able to handle negative numbers

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
	bool isAtEnd = ((size_t)newIndex >= list->items.size());
	newIndex = util::clip<int>( newIndex, 0, list->items.size()-1);

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

	e = pthread_mutex_unlock( &mutex.group );
	if( e!=0)	printMutexError(e);
	return list->currentItem;
}



// Get current song
musicFile slimIPC::getSong(string groupName)
{
	musicFile f;
	if( group.find(groupName) != group.end() )
    {
        int size = group[groupName].items.size();
        int idx  = group[groupName].currentItem;
        if( size > idx )
		    f = group[groupName].items[ idx ];
    }
	return f;
}



// Register a callback function, to get an update once
// the client status changes:
void slimIPC::registerCallback(callbackFcn *callback, string clientName)
{
	pthread_mutex_unlock( &mutex.others );

	pair<string,callbackFcn*> cb(clientName,callback);
	callbacks.push_back(cb);

	pthread_mutex_unlock( &mutex.others );
}


// Remove the callback again
void slimIPC::unregisterCallback(callbackFcn *callback)
{
	int e = pthread_mutex_lock( &mutex.others );
	if( e!=0)	printMutexError(e);
	//vector<pair<string,callbackFcn*>>::iterator it;
	for( size_t i=0; i < callbacks.size(); i++)
		//for(it = callbacks.begin(); it != callbacks.end(); it++)
	{
		if( callback == callbacks[i].second )
		{
			//Nasty, since callback-destructor can unregister itself (not anymore)
			//pthread_mutex_lock( &mutex.others );
			callbacks.erase( callbacks.begin() + i);
			i--;	//update index w.r.t removal
			//pthread_mutex_unlock( &mutex.others );

		}
	} //for it=callbacks
	e = pthread_mutex_unlock( &mutex.others );
	if( e!=0)	printMutexError(e);
} // unregisterCallback



void slimIPC::notifyClientUpdate(client* device)
{
	int e = pthread_mutex_lock( &mutex.others );
	if( e!=0)	printMutexError(e);
	int nrc = 0;
	///Execute all callback functions:
	//vector<pair<string,callbackFcn*>>::iterator it;
	//for(it = callbacks.begin(); it != callbacks.end(); it++)

	//Since the call to the client might actually remove it, the
	// callbacks.size() call in the for-statement is really required.
	for( size_t i=0; i < callbacks.size(); i++)
	{
		std::vector<dev_s>::iterator dev = devByName( callbacks[i].first );
		if( dev != devices.end() )
		{
			//Call the callback, and remove it, if it's done:
			if ( !callbacks[i].second->call() )
			{
				callbacks.erase( callbacks.begin() + i);
				i--;	//update index w.r.t removal
			}
			nrc++;
		}
	}
	e = pthread_mutex_unlock( &mutex.others );
	if( e!=0)	printMutexError(e);

	//Open/close both servers, to wake them up out of the select() wait.
	// So this would be the main motivation to make the whole thing single-threaded.
	// It is indeed a bit ugly.
	if( nrc > 0 )	//only if callbacks have been executed
	{
		const char *home = "127.0.0.1";
		Socket s;
		s.setBlocking(false);
		s.Connect(home, shoutServer->getPort() );
		s.Close();
		s.Connect(home, slimServer->getPort() );
		s.Close();
	}
}
