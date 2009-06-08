

//must include server first, since it uses winsock under win32:
#include "serverShoutCast.hpp"
#include "configParser.hpp"
#include "musicDB.hpp"

#include <pthread.h>


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
	string dbIdx  = config.getset("musicDB", "dbIdx", "SqueezeD.idx");
	config.write(configFile);

	//Create a new DB:
	musicDB db( dbPath.c_str() );
	//db.scan( dbFile.c_str() );
	//printf("scanning: found %llu items\n", (LLU)(db.size()) );
	//db.index( dbIdx.c_str() );

	//Close and re-open it:
	//int loadResult = db.load(dbFile.c_str() , dbIdx.c_str() );
	db.init( dbFile.c_str() , dbIdx.c_str() );


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
	string dbFile = config.getset("musicDB", "dbFile", "SqueezeD.db" );
	string dbIdx  = config.getset("musicDB", "dbIdx",  "SqueezeD.idx");
	int shoutPort	= config.getset("shout", "port", 9000 );
	int shoutConn	= config.getset("shout", "maxConnections", 10 );
	int slimPort	= config.getset("slim",	 "port", 3483);
	config.write(configFile);

	// Resolve home directory ('~'), if required.
	dbPath = path::normalize(dbPath);

	// Initialize music databse:
	musicDB db( dbPath.c_str() );
	//db.scan( dbFile.c_str() );
	//printf("scanning: found %llu items\n", (LLU)db.size() );
	//db.index( dbIdx.c_str() );
	db.init( dbFile.c_str(), dbIdx.c_str() );

	// Initialize servers
	slimIPC			ipc(&db, &config);
	TCPserverShout	shoutServer( &ipc, shoutPort, shoutConn);
	TCPserverSlim	slimServer( &ipc, slimPort);

	// Run only the shoutcast server
	shoutServer.runNonBlock();
}


void testFlac(void)
{
	const char path[] = ".";

	std::vector<std::string> dir = path::listdir( path );

	db_printf(1,"Found %llu files\n", (LLU)dir.size() );
	for(size_t i=0; i< dir.size(); i++)
	{
		std::string ffull = path::join( path, dir[i] );
		fileInfo fInfo( ffull.c_str() );

		if( fInfo.isAudioFile )
		{
			db_printf(1,"%s (%s)\t", fInfo.tags["title"].c_str(), fInfo.tags["artist"].c_str() );
			int sec = fInfo.length % 60;
			int min = fInfo.length / 60;
			printf("%i Hz, %i bits, %i channels. %i:%02i\n\n", fInfo.sampleRate, fInfo.nrBits, fInfo.nrChannels, min, sec );
		} else {
			db_printf(1,"%s: not a valid audio-file\n", ffull.c_str() );
		}
	}
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
	db_printf(6,"Loading confugaration data from %s\n", configFile);
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
	db_printf(6,"checking path '%s'\n", dbPath.c_str() );
	dbPath = path::normalize(dbPath);		// Resolve home directory ('~'), if required.

	db_printf(6,"opening musicDB '%s'\n", dbPath.c_str() );
	musicDB db( dbPath.c_str() );

	//Load database, or scan if it's missing:
	db.init( dbFile.c_str() , dbIdx.c_str() );

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

	//db_printf(3,"Testing tagging library on current dir:\n");
	//testFlac();

	//Test remote streaming:
	//const char *shoutStream = "http://scfire-dtc-aa02.stream.aol.com:80/stream/1013";
	//musicFile f(shoutStream);

	startThreads();

	printf("Exit\n");
    return 0;
}
