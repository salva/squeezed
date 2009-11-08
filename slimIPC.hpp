

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



/// Basic definition of items in the database
/// TODO: inherit from fileinfo.hpp::fileInfo, and add some
///			squeezebox specific info (url,ip,port)
class musicFile
{
private:
	/// Re-set all data:
	void clear(void);

	///	Initialize from a file or url.
	/// For files, the address is 0.0.0.0:defaultPort
	bool setFromFile(const char *fname, uint16_t defaultPort=9000);
public:
    // Song info (this should be a key-value pairs):
    std::string title;
    std::string artist;
    std::string album;

    // Datafile info:
    char format;		///< Compression format: [m]p3, [p]cm, [f]lac, ogg
    int nChannels;      ///< Number of channels
    int nBits;          ///< Uncompressed number of bits;
    int sampleRate;     ///< Samples per second
	int length;			///< File length, in number of seconds
	float gainTrack, gainAlbum;	// Replay-gain

    // Storage location:
    std::string url;    //path/filename to use in a stream request.
	uint32_t ip;
	uint16_t port;

	/// Empty initialization
	musicFile(void);

	///	Initialize from a file or url.
	/// For files, the address is 0.0.0.0:defaultPort
	musicFile(const char *fname, uint16_t defaultPort=9000);

	// Init from a comma separated list of values (unpickle)
	//musicFile(string csv, uint16_t port=9000);

	/// Convert to comma separated list of values (pickle)
	operator string();


	/// Generate a title to display:
	std::string displayTitle(void) const
	{
		std::string out;
		if( title.size() > 0)
		{
			out = title;
			if( artist.size() > 0 )
				out += " (" + artist + ")";
		} else {
			std::vector<std::string> pf = path::split( url );
			out = pf[1];
		}
		return out;
	}


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
	size_t		currentItem;		// current seletect item, req
	uint32_t	lastUpdate;		// time of last update
	bool		repeat;			//repeat list, after it's completed

	//To handle transition to next song for multiple clients:
	// first client to reach end-of-song, requests next.
	// then request will pass a new play command to all clients.

	// Default constructor:
	playList(): currentItem(0), lastUpdate(0), repeat(false)
	{}

	//allow read-only acces. is not thread safe.
	vector<musicFile>::const_iterator begin(void) const
	{	return items.begin();	}

	vector<musicFile>::const_iterator end(void) const
	{	return items.end();	}


	//get entry from the playlist
	musicFile get(size_t index) const;
};




//for mutexes, see:
//http://opengroup.org/onlinepubs/007908775/xsh/pthread_mutex_lock.html
#include <stdio.h>
void printMutexError(int err);



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
	// Returns devices.end() if device is not found.
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
		pthread_mutex_t config;
	} mutex;


public:
	/// Provide searches both for shout and slim proto
	/// the musicDB itself should be thread safe.
	musicDB	   *db;


	slimIPC(musicDB *db, configParser	*config);
	~slimIPC(void);

	// load an save status from/to disk.
	// both group and deviceGroups need to be stored
	void load( int shoutPort );
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
	configValue getConfig(string section, string option, configValue defaultValue)
	{
		pthread_mutex_lock( &mutex.config );

		//store a default for everything that's read:
		if( !config->hasOption(section,option) )
		{
			config->set(section, option, defaultValue);
			config->write();	//TODO: write this to disk, but not always
		}
		configValue ret = config->get(section,option,defaultValue);

		pthread_mutex_unlock( &mutex.config );
		return ret;
	}

    vector<std::string> getConfigSections(void)
    {
		pthread_mutex_lock( &mutex.config );
		vector<std::string> slist = config->listSections();
		pthread_mutex_unlock( &mutex.config );
		return slist;
    }

	void setConfig(string section, string option, configValue newValue)
	{
		pthread_mutex_lock( &mutex.config );

		config->set(section, option, newValue);		//store a default for everything that's read
		//TODO: write this to disk, but not always
		config->write();

		pthread_mutex_unlock( &mutex.config );
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
	/// Commands are:  play,pause,stop,prev,next,volume
	void setDevice(const string& devName, const string& cmd, const string& cmdParam="");

	/// Device info
	/// fields are: volume,elapsed
	std::string getDevice(const string& devName, const string& field);


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
	/// Note that the checksum is not very robust, since part of the
	/// data that is checksummed consist of pointers (&std::string).
	const playList* getList(string clientName, ///<I: name of the client to fetch the playlist
							int *checksum=NULL ///<O: optional, checksum of all data
							);


	void setRepeat(string groupName,bool state);


	bool getRepeat(string groupName);


	/// Get current song
	musicFile getSong(string groupName);

	/// Link groups to clients
	int connect(string clientName, string groupName);
	int disconnect(string clientName, string groupName);

	/// Callback functions for updates: (web-gui)
	class callbackFcn {
	public:
        // Should return false to unregister it.
		virtual bool call(void)=0;
	};

	/// Register a callback function, to get an update once
	/// the client status changes:
	void registerCallback(callbackFcn *callback, string clientName);

	/// Remove the callback again
	void unregisterCallback(callbackFcn *callback);

	/// Notify all sub-systems that the status of a device has changed:
	void notifyClientUpdate(client* device);


private:
	vector<pair<string,callbackFcn*> > callbacks;
};


#endif

/**@}
 *end of doxygen group
 */
