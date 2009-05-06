


#include "configParser.hpp"
#include "serverShoutCast.hpp"
#include "slimProto.hpp"


#include "slimIPC.hpp"


//----------------- small helper functions ---------------------


musicFile::musicFile(void)
{
	format = '?';
	nChannels = 0; nBits = 0; sampleRate = 0;
	ip =0; port = 0;
}


musicFile::musicFile(const char *fname, uint16_t port)
{
	std::auto_ptr<fileInfo> finfo = getFileInfo(fname);
	if( finfo->isAudioFile )
	{
		title  = finfo->tags["title"];
		artist = finfo->tags["artist"];
		album  = finfo->tags["album"];

		format = '?';
		if( finfo->mime == "audio/mpeg")
			format = 'm';
		if( finfo->mime == "audio/flac")
			format = 'f';
		nChannels	= finfo->nrChannels;
		nBits		= finfo->nrBits;
		sampleRate	= finfo->sampleRate;

		url			= fname;
		ip			= 0;	//localhost
		this->port = port;
	} else
		url			= "";
}


// init from a comma separated list of values
musicFile::musicFile(string csv, uint16_t port)
{
	int i=0;
	vector<string> items = pstring::split(csv, ',');
	title  = items[i++];
	artist = items[i++];
	album  = items[i++];
	format = items[i++][0];
	nChannels = atoi( items[i++].c_str() );
	nBits	  = atoi( items[i++].c_str() );
	sampleRate= atoi( items[i++].c_str() );
	url = items[i++];

	this->port = port;
	ip = 0;
}


musicFile::operator string()
{
	stringstream out;
	out << title << "," << artist << "," << album << ",";
	out << format << "," << nChannels << "," << nBits << "," << sampleRate << ",";
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
		std::vector<string> items = path::listdir( url );
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
	load( );

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
void slimIPC::load( void )
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
			musicFile f= musicFile(item);
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
			sprintf(itemName, "item%03zu", i);
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
		config->set( sectionName, "volume", it->device->state.volume );
		config->set( sectionName, "group" , it->device->state.currentGroup );
		config->set( sectionName, "brightness" , it->device->state.brightness );
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
