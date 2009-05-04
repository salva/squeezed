

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
#include <pthread.h>

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
	musicFile(void);

	///	initialize from a music file
	musicFile(const char *fname, uint16_t port=9000);

	/// init from a comma separated list of values
	musicFile(string csv, uint16_t port=9000);

	/// convert to comma separated list of values
	operator string();
};


/// Make playlist entries from filename/directory name
std::vector<musicFile> makeEntries(string url);

/// Generate playlist entries from a sub-selection of a query result:
std::vector<musicFile> makeEntries( musicDB *db, dbQuery& query, size_t uniqueIndex );



///---------------------------- IPC interface -------------------------------


/// Playlist status, per group
/// This class is not thread-safe.
class playList
{
public:
	vector<musicFile> items;
	size_t	currentItem;		// current seletect item, req
	uint32_t lastUpdate;		// time of last update
	bool		repeat;			//repeat list, after it's completed

	//To handle transition to next song for multiple clients:
	// first client to reach end-of-song, requests next.
	// then request will pass a new play command to all clients.

	//default constructor:
	playList(): currentItem(0), lastUpdate(0), repeat(false)
	{}

	//allow read-only acces. is not thread safe.
	vector<musicFile>::const_iterator begin(void) const
	{	return items.begin();	}

	vector<musicFile>::const_iterator end(void) const
	{	return items.end();	}


	//get entry from the playlist
	musicFile get(size_t index);
};




/// Handle information flow between multiple slim- and shout-client
/// connections.
/// TODO: add mutexes to make it thread safe.
class slimIPC
{
private:
	// Configuration data
	string			playListFile;
	configParser	*config;

	// Connection to both servers:
	TCPserverSlim	*slimServer;
	TCPserverShout	*shoutServer;

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


	/// Device reading, 
	//	must be private for this class to be thread-safe
	std::vector<dev_s>::iterator devByName( string clientName )
	{
		std::vector<dev_s>::iterator it = devices.begin();
		for( ; it != devices.end(); it++)
			if(it->name == clientName)
				break;
		return it;
	}


	//TODO: use real mutexes
	struct {
		pthread_mutex_t group;	//all playlist changes
		pthread_mutex_t client;	//all client changes
		pthread_mutex_t others;	//for slimServer and shoutServer
	} mutex;


public:
	/// Provide searches both for shout and slim proto
	/// the musicDB itself should be thread safe.
	musicDB	   *db;


	slimIPC(musicDB *db, configParser	*config);
	~slimIPC(void);

	// load an save status from/to disk.
	// both group and deviceGroups need to be stored
	void load(void);
	void save(void);

	void registerSlimServer(TCPserverSlim *slimServer)
	{
		this->slimServer = slimServer;
	}

	void registerShoutServer(TCPserverShout *shoutServer)
	{
		this->shoutServer = shoutServer;
	}

	int getShoutPort(void);

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

	/// Get group name for a given client
	string getGroup(string clientName);

	// Playlist control, for both servers:
	/// Replace all items in the playlist
	int setGroup(string groupName, vector<musicFile>& items);
	/// Add items to a playlist, default to add at end
	int addToGroup(string groupName, vector<musicFile>& items, int offset=-1);


	/// Get current playlist for a device:
	/// TODO: make a copy, else it's not thread-safe.
	const playList* getList(string clientName)
	{
		const playList *ret = NULL;
		std::vector<dev_s>::iterator it = devByName( clientName );
		if( it != devices.end() )
			ret	= it->group;
		return ret; //->items;
	}


	/// Get current song
	musicFile getSong(string groupName);

	/// Link groups to clients
	int connect(string clientName, string groupName);
	int disconnect(string clientName, string groupName);

};

#endif

/**@}
 *end of doxygen group
 */
