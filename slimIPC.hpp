

/**
 * \defgroup slimIPC slimIPC
 * Synchronization between slim-server and shoutcast/data/web-server
 *
 * This also handles synchronization of multiple clients using the same playlist.
 *
 *@{
 */

#ifndef _SLIMIPC_HPP_
#define _SLIMIPC_HPP_

#include <vector>
#include <map>

//declare the public class, so header inclusion works:
class slimIPC;
//class configParser;
//class configValue;
#include "configParser.hpp"
#include "slimProto.hpp"
#include "serverShoutCast.hpp"
#include "musicDB.hpp"



using namespace std;


class slimConnectionHandler;
class TCPserverShout;				// Nasty, but couldn't make it work otherwise
class TCPserverSlim;
typedef slimConnectionHandler client;



/// Basic definition of items in the database:
class musicFile {
public:
    //song info (this should be a key-value pairs):
    std::string title;
    std::string artist;
    std::string album;

    //datafile info:
    char format;		//compression format: [m]p3, [p]cm, [f]lac, ogg
    int nChannels;      //number of channels
    int nBits;          //uncompressed number of bits;
    int sampleRate;     //samples per second

    //storage location:
    std::string url;    //path/filename to use in a stream request.
	uint32_t ip;
	uint16_t port;

	/// empty initialization
	musicFile(void)
	{
		format = '?';
		nChannels = 0; nBits = 0; sampleRate = 0;
		ip =0; port = 0;
	}

	//	initialize from a music file
	musicFile(const char *fname, uint16_t port=9000);

	/// init from a comma separated list of values
	musicFile(string csv, uint16_t port=9000)
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

	/// convert to comma separated list of values
	operator string()
	{
		stringstream out;
		out << title << "," << artist << "," << album << ",";
		out << format << "," << nChannels << "," << nBits << "," << sampleRate << ",";
		out << url;
		return out.str();
	}
};


/// Make playlist entries from filename/directory name
std::vector<musicFile> makeEntries(string url);

/// Generate playlist entries from a sub-selection of a query result:
std::vector<musicFile> makeEntries( musicDB *db, dbQuery& query, size_t uniqueIndex );



///---------------------------- IPC interface -------------------------------


/// Playlist status, per group
class playList
{
public:
	vector<musicFile> items;
	size_t	currentItem;		// current seletect item, req
	uint32_t lastUpdate;		// time of last update
	bool		repeat;			//repeat list, after it's completed

	//how to handle transition to next song for multiple clients:
	// first client to reach end-of-song, requests next.
	// then request will pass a new play command to all clients.


	//mutex??

	//default constructor:
	playList(): currentItem(0), lastUpdate(0), repeat(false)
	{}

	//allow read-only acces. is not very thread safe.
	vector<musicFile>::const_iterator begin(void) const
	{	return items.begin();	}

	vector<musicFile>::const_iterator end(void) const
	{	return items.end();	}


	/// This can be made thread-safe:
	musicFile get(size_t index)
	{
		musicFile ret;
		if( index < items.size() )
			ret = items[index];
		return ret;
	}

};





/// Handle information flow between multiple slim- and shout-client
/// connections.
class slimIPC
{
private:
	/// Configuration data
	string		playListFile;
	configParser	*config;

	/// Playlist groups
	map<string, playList> group;

	// All devices, controlled by the slim-server
	struct dev_s {
		string name;
		client* device;
		playList* group;

		dev_s(string name, client* device, playList* group):
			name(name), device(device), group(group)
		{}
	};
	std::vector<dev_s> devices;


	std::vector<dev_s>::iterator devByName( string clientName )
	{
		std::vector<dev_s>::iterator it = devices.begin();
		for( ; it != devices.end(); it++)
			if(it->name == clientName)
				break;
		return it;
	}


public:
	TCPserverShout	*shoutServer;	//these both shouldn't be neccesary.
	TCPserverSlim	*slimServer;

	/// provide searches both for shout and slim proto
	musicDB	   *db;

	slimIPC(musicDB *db, configParser	*config);
	~slimIPC(void);

	// load an save status from/to disk.
	// both group and deviceGroups need to be stored
	void load(void);
	void save(void);

	/// Acces to the main configuration file
	/// TODO: mutex this.
	configValue getConfig(string section, string option, configValue defaultValue)
	{
		//store a default for everything that's read:
		if( !config->hasOption(section,option) )
		{
			config->set(section, option, defaultValue);
			config->write();	//TODO: write this to disk, but not always
		}
		return config->get(section,option,defaultValue);
	}

	/// TODO: mutex this
	void setConfig(string section, string option, configValue newValue)
	{
		//store a default for everything that's read:
		config->set(section, option, newValue);
		//TODO: write this to disk, but not always
		config->write();
	}


	/// Device control, for slimProto
	int addDevice(string clientName, client* dev);

	/// forget about it
	int delDevice(string clientName);


	/// Device reading, for shoutProto
	const client* getDevice(string clientName)
	{
		const client *dev =  NULL;
		std::vector<dev_s>::iterator client = 	devByName( clientName );
		if(client != devices.end() )
			dev = client->device;
		return dev;
	}


	/// Get group name for a given client
	string getGroup(string clientName)
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


	/// Get a list of all connected devices
	vector<string> listDevices()
	{
		vector<string> out;
		for(size_t i=0; i< devices.size(); i++)
			out.push_back( devices[i].name );
		return out;
	}

	/// Device control, for shoutProto
	void cmdDevice(string devName, string cmd);	//'play','pause','stop'


	/// Seek time into current song
	void seekSong(string groupName, float seconds);	//seek into current song


	/// Set current song, all listeners get a 'play' command.
	/// When one client needs the next song, it calls seekList also.
	int seekList(string groupName, int offset, int origin, bool stopCurrent=true);


	// Playlist control, for both servers:
	/// Replace all items in the playlist
	int setGroup(string groupName, vector<musicFile>& items);
	/// Add items to a playlist, default to add at end
	int addToGroup(string groupName, vector<musicFile>& items, int offset=-1);


	/// Get current playlist for a device:
	const playList* getList(string clientName)
	{
		const playList *ret = NULL;
		std::vector<dev_s>::iterator it = devByName( clientName );
		if( it != devices.end() )
			ret	= it->group;
		return ret; //->items;
	}


	/// Get current song
	musicFile getSong(string groupName)
	{
		musicFile f;
		if( group.find(groupName) != group.end() )
			f = group[groupName].items[ group[groupName].currentItem ];
		return f;
	}

	/// Link groups to clients
	int connect(string clientName, string groupName);
	int disconnect(string clientName, string groupName);

};

#endif

/**@}
 *end of doxygen group
 */
