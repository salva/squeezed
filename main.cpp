
#include "configParser.hpp"

#include "serverShoutCast.hpp"
#include "musicDB.hpp"



#if defined(WIN32) && !defined(__CYGWIN__)
#include "pthread.h"
#else
#include <pthread.h>
#endif


//#if defined(WIN32)
const char configFile[] = "squeezed.ini";
//#else
//const char configFile[] = "~/.squeezed.ini";
//#endif


//the thread function for the shout-server:
void *shoutThread( void *ptr )
{
	TCPserverShout *shoutServer = (TCPserverShout *)ptr;
	int i = shoutServer->runNonBlock();
	printf("shoutServer returned : %i\n", i);
	return 0;
}


configParser loadConfig(void)
{
	configParser config(configFile);
	string dbPath = config.getset("musicDB", "path", "." );	//path to music files
	string dbFile = config.getset("musicDB", "dbFile", "SqueezeD.db");
	config.write(configFile);
	return config;
}



void testDB(void)
{
	configParser config(configFile);
	string dbPath = config.getset("musicDB", "path", "." );	//path to music files
	string dbFile = config.getset("musicDB", "dbFile", "SqueezeD.db");
	config.write(configFile);

	musicDB db( dbPath.c_str() );
	db.scan( dbFile.c_str() );
	printf("scanning: found %llu items\n", (LLU)(db.size()) );

	db.index();

	//make a test query:
	char match[] = "t";
	dbField field = DB_TITLE;
	dbQuery query( &db, field,  match);

	printf("found %llu %s starting with %s\n", (LLU)query.size(), dbFieldStr[field], match);
}


void testShout(void)
{
	//load configuration:
	configParser config(configFile);
	string dbPath = config.getset("musicDB", "path", "." );	//path to music files
	string dbFile = config.getset("musicDB", "dbFile", "SqueezeD.db");
	int shoutPort	= config.getset("shout", "port", 9000 );
	int shoutConn	= config.getset("shout", "maxConnections", 10 );
	int slimPort	= config.getset("slim",	 "port", 3483);
	config.write(configFile);

	// Resolve home directory ('~'), if required.
	dbPath = path::normalize(dbPath);	

	// Initialize music databse:
	musicDB db( dbPath.c_str() );
	db.scan( dbFile.c_str() );
	printf("scanning: found %llu items\n", (LLU)db.size() );
	db.index();

	// Initialize servers
	slimIPC			ipc(&db, &config);
	TCPserverShout	shoutServer( &ipc, shoutPort, shoutConn);
	TCPserverSlim	slimServer( &ipc, slimPort);

	// Run only the shoutcast server
	shoutServer.runNonBlock();
}


void testFlac(void)
{
	const char fname[] = "test.flac";
	std::auto_ptr<fileInfo> fInfo = getFileInfo(fname );
	int dummy=1;
}


//one thread per port:
void startThreads()
{
	// Default settings:
	configParser::config_t defaults;
	defaults["config"]["path"]	= configValue(".");		//path to configuration data of sub-modules
	defaults["musicDB"]["path"]	= configValue("." );	//path to music files
	defaults["musicDB"]["dbFile"] = configValue("SqueezeD.db");
	defaults["musicDB"]["dbIdx"]  = configValue("SqueezeD.idx");

	// Load configuration data
	configParser config(configFile, defaults);

	int shoutPort	= config.getset("shout", "port", 9000 );
	int shoutConn	= config.getset("shout", "maxConnections", 10 );
	int slimPort	= config.getset("slim",	 "port", 3483);
	string cfgPath= config.get("config", "path");
	string dbPath = config.get("musicDB", "path");
	string dbFile = config.get("musicDB", "dbFile");
	string dbIdx  = config.get("musicDB", "dbIdx");

	// Write back the default values, in case some are missing:
	config.write(configFile);

	// Initialize the database
	dbPath = path::normalize(dbPath);		// Resolve home directory ('~'), if required.
	musicDB db( dbPath.c_str() );

	// only scan if index is missing
	if( !path::isfile( dbFile ) )
		db.scan(dbFile.c_str() );
	else
		db.load(dbFile.c_str() , dbIdx.c_str() );
	db.index();
	db_printf(1,"found %llu files\n", (LLU)db.size() );

	//Both shout- and slim-server need to share some info.
	//	the IPC block will provide the mutexes for safe multi-threading
	//   slim relies on shout to keep it's playlist, and get stream-audio-formats??
	//   shout needs playback control from slim, and also status info.
	//	IPC keeps a playlist per group, and device status per device.
	//  for now, slim IPC can have only one client and one server
	//  since all initiative is from both servers, they both get an handle to the IPC
	slimIPC			ipc(&db, &config);
	TCPserverShout	shoutServer( &ipc, shoutPort, shoutConn);
	TCPserverSlim	slimServer( &ipc, slimPort);

	//shoutcast server in one thread:
	pthread_t thread;
	pthread_create( &thread, NULL, shoutThread, &shoutServer);

	//slim server in the main trhead:
	int i = slimServer.runNonBlock();
	printf("slimServer returned : %i\n", i);

	//stop the shoutcast server, when slim-server stops:
	shoutServer.stop = true;

	//wait for the thread to finish:
	pthread_join(thread, NULL);
}



int main()
{
	//testDB();
	//testShout();
	//testFlac();

	startThreads();

	printf("Exit\n");
    return 0;
}
