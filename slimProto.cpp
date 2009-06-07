/* Slim squuezebox protocol
*
*/

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include "slimProto.hpp"
#include "slimDisplay.hpp"
#include "slimIPC.hpp"
#include "util.hpp"

#include "debug.h"


//--------------------------- Internal functions -----------------------------


const char *sqDeviceNames[] = {"?","?", "squeezebox", "softSqueeze", "SqueezeBox2",
"transport", "softSqueeze3", "Receiver", "SqueezeSlave", "Controller" };

const char *sqDeviceName(uint16_t idx)
{
	if( idx < array_size(sqDeviceNames) )
		return sqDeviceNames[idx];
	return sqDeviceNames[0];
}



//Squeeze-box mapping of IR-codes, see:
// squeezecenter/IR/slim_devices_remote.ir
IRmap IRmap_SB[] = {
	{0x76899867, cmd_0},
	{0x7689f00f, cmd_1},
	{0x768908f7, cmd_2},
	{0x76898877, cmd_3},
	{0x768948b7, cmd_4},
	{0x7689c837, cmd_5},
	{0x768928d7, cmd_6},
	{0x7689a857, cmd_7},
	{0x76896897, cmd_8},
	{0x7689e817, cmd_9},

	{0x7689807f, cmd_Vup},
	{0x768900ff, cmd_Vdown},
	{0x7689906f, cmd_left},
	{0x7689d02f, cmd_right},
	{0x7689e01f, cmd_up},
	{0x7689b04f, cmd_down},

	{0x768910ef, cmd_play},
	{0x768920df, cmd_pause},
	{0x7689c03f, cmd_rewind},
	{0x7689a05f, cmd_forward},
	{0x7689d827, cmd_shuffle},
	{0x768938c7, cmd_repeat},

	{0x7689609f, cmd_add},
	{0x768958a7, cmd_search},
	{0x768918e7, cmd_favorites},
	{0x7689708f, cmd_browse},
	{0x76897887, cmd_playing},
	{0x7689f807, cmd_size},
	{0x768904fb, cmd_brightness},

	{0x768940bf, cmd_pwr},
	{0x7689b847, cmd_sleep},
	{0x76898f70, cmd_pwrOn},
	{0x76898778, cmd_pwrOff},

	// debugged from hardware (squeezebox v3)
};


//Lookup-table to convert the 'commands_e' enum to strings:
const char *commandsStr[] =
{
	"",
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"vol+","vol-",
	"left","right","up","down",
	"play","pause","rewind","forward",
	"shuffle","repeat",
	"add", "search", "favorites", "browse", "now playing",
	"size","brightness",
	"power","sleep","power On","power Off",
};



//--------------------------- Utility functions ------------------------------




//--------------------------- Tie connection to the server -------------------


TCPserverSlim::TCPserverSlim(slimIPC *ipc, int port, int maxConnections) :
		TCPserver(port,maxConnections),
		ipc(ipc)
{
	ipc->registerSlimServer(this);
	printf("slimProto: port %i, data port %i\n", port, ipc->getShoutPort() );
}


connectionHandler* TCPserverSlim::newHandler(SOCKET socketFD)
{
	return new slimConnectionHandler(socketFD,ipc);
}


//--------------------------- SlimProto --------------------------------------


//Callback to connect to squeezeNetwork:
class squeezeNetwork: public slimGenericMenu::callback
{
private:
	slimConnectionHandler *connection;
public:
	squeezeNetwork(slimConnectionHandler *connection):
		connection(connection)
	{}

	void run(void)
	{
		uint32_t ip = htonl( 0x01 );	//device interprets this a squeezenetwork
		connection->send( "serv", 4, &ip );
	}
};


//internal members:
struct slimConnectionHandler::menu_s
{
    slimDisplay		*display;
	slimMenu		*main;
	slimMenu		*searchMenu;	//sub-menu with all search options
	slimSearchMenu	*searchAlbum, *searchArtist;
	slimBrowseMenu	*folderMenu;	//needs to be written
	slimPlayingMenu	*nowPlayingScreen;
	slimPlayListMenu *menuPlayList;
	slimVolumeScreen *volumeScreen;
	favoritesMenu	 *favorites;

	squeezeNetwork	*sqNet;

	menu_s(slimConnectionHandler *parent)
	{
		display = new slimDisplay(parent);
		sqNet   = new squeezeNetwork(parent);

		//Initialize the menus:
		// for refernece, slim's menu can be found at:
		//	http://wiki.slimdevices.com/index.php/UserInterfaceHierarchy
		main = new slimMenu(display, "Squeezebox Home");

		folderMenu		= new slimBrowseMenu(display, "Folder", main);
		nowPlayingScreen= new slimPlayingMenu(display, main);
		favorites       = new favoritesMenu(display,"Favorites", main);

		searchMenu		= new slimMenu(display, "Search", main);
		searchAlbum		= new slimSearchMenu(display, searchMenu, DB_ALBUM);
		searchArtist	= new slimSearchMenu(display, searchMenu, DB_ARTIST);
		menuPlayList	= new slimPlayListMenu(display, nowPlayingScreen);
		volumeScreen	= new slimVolumeScreen(display);

		searchMenu->addItem("Albums", searchAlbum);
		searchMenu->addItem("Artist", searchArtist);
		//searchMenu->addItem("Title", searchTitle);
		nowPlayingScreen->setPlayMenu(menuPlayList);

		//TODO: read favorites/web-radio from a configuration file:
		favorites->playlist.push_back( favoritesMenu::playListItem("SKY.FM - Alt. Rock", "http://88.191.69.42:8002/") );
		favorites->playlist.push_back( favoritesMenu::playListItem("KinK FM", "http://81.173.3.20:80/") );
		//favorites->playlist.push_back( favoritesMenu::playListItem("Arrow Classic Rock", "http://81.23.251.55/ArrowAudio02") );//use WMA, not mp3
		favorites->playlist.push_back( favoritesMenu::playListItem("SKY.FM - Best of 80's", "http://scfire-dtc-aa02.stream.aol.com:80/stream/1013") );
		favorites->playlist.push_back( favoritesMenu::playListItem("SKY.FM - Top Hits Music", "http://scfire-mtc-aa03.stream.aol.com:80/stream/1014") );


		//	Tie them together:
		main->addItem("Now Playing",	nowPlayingScreen);
		main->addItem("Folders",		folderMenu );
		main->addItem("Search",			searchMenu );
		main->addItem("Favorites",		favorites );
		main->addItem("Squeeze Network",sqNet );
		//main->addItem("Settings",		(slimGenericMenu::callback*) NULL );		//todo: write menu
	}

	~menu_s()
	{
		delete sqNet;
		delete main;
		delete display;
	}
};





namespace visu
{
	struct visu_s
	{
		enum which {VISU_NONE, VISU_VUMETER, VISU_SPECTRUM, VISU_WAVE };

		const char *desc;
		int bar;	//display bar with seconds elapsed/remaining
		char secs;	//display elapsed or remaining
		int width;	//width remaining for text display
		uint32_t nrParam;	//number of parameters
		const uint32_t *param;	//parameters
	};

	//These settings are copied from squeezecenter-7.3.2/Slim/Display/SqueezeBox2.pm:
	const uint32_t pNone[]     	= {visu_s::VISU_NONE};
	const uint32_t pVUsmall[]	= {visu_s::VISU_VUMETER, 0, 0, 280, 18, 302, 18};
	const uint32_t pSpectrum[]	= {visu_s::VISU_SPECTRUM,0, 0, 0x10000, 0, 160, 0, 4, 1, 1, 1, 1, 160, 160, 1, 4, 1, 1, 1, 1};
	const uint32_t pWave[]		= {visu_s::VISU_WAVE,    0, 0, 0x10000, 0, 160, 0, 4, 1, 1, 1, 1, 160, 160, 1, 4, 1, 1, 1, 1};

	const struct visu_s defVisu[] = {
		{"BLANK"			, 0, 0, 320, array_size(pNone),		pNone },
		{"VU_SMALL_ELAPSED"	, 1, 1, 278, array_size(pVUsmall),	pVUsmall  },
		{"SPEC_ELAPSED"		, 1, 1, 320, array_size(pSpectrum), pSpectrum },
//		{"WAVE_ELAPSED"		, 1, 1, 320, array_size(pWave), 	pWave },
	};
}



void slimConnectionHandler::setVisualization(uint8_t mode)
{
	// The packet data:
	const static int maxParam = 25;
	char data[2 + 4*maxParam];
	uint32_t *param = (uint32_t*)(&data[2]);

	// Get visualization parameters:
	mode = mode % array_size(visu::defVisu);
	int nrParam = visu::defVisu[mode].nrParam;
	const uint32_t *vparam = visu::defVisu[mode].param;

	// First two are bytes:
	data[0] = vparam[0];	//'which' a.k.a visu::vis_e
	data[1] = nrParam - 1;	//'which' doesn't count as parameter

	db_printf(30, "setVisualization(): mode %i, %i parameters\n", data[0], data[1] );

	// The rest are words:
	for(int i=1; i < nrParam; i++)
		param[ i-1 ] = htonl( vparam[i] );

	send("visu", 2 + 4*(nrParam - 1) , data);
}




struct slimConnectionHandler::state_s
{
	bool 			power;			///< on or off?
	std::string		currentGroup;	///< Current playlist
	slimScreen		*currentScreen;	///< Current visible menu
	char			uuid[18];		///< unique device identifier
	char			volume;
	char			brightness;
	enum {PL_STOP, PL_PAUSE, PL_PLAY} playState;
	enum anim_e 	anim;			///< animation state
	char			currentViso;	///< current visualization mode

	/// The song currently playing. ipc->getList() gives access to the song
	/// which is currently send. At the end of a song, those are different.
	musicFile		currentSong;


	// Constructor, set defaults
	state_s():	power(true),
				currentGroup("all"),
				currentScreen(NULL),	//this doens't work if mainMenu isn't initialized
				volume(90),
				brightness(5),
				playState(PL_STOP),
				anim(ANIM_NONE),
				currentViso(0)
	{ }
};



slimConnectionHandler::slimConnectionHandler(SOCKET socketFD, slimIPC *ipc) :
								connectionHandler(socketFD),
								ipc(ipc)
{
	//Initialize the display (display needs device information):
	menu    = new menu_s(this);
	state   = new state_s;

	state->currentScreen = menu->main;

	//Some other data:
	memset( &IRdata, 0, sizeof(IRdata) );
	memset( &device, 0, sizeof(device) );
	memset( &status, 0, sizeof(status) );
}



/// Deconstructor, closes the connection, deregisters the player
slimConnectionHandler::~slimConnectionHandler()
{
	ipc->delDevice( state->uuid );
	delete menu;
}


void slimConnectionHandler::setMenu(slimScreen *newMenu, char transition)
{
	if( state->currentScreen != newMenu)
		newMenu->draw(transition, 24 );
	state->currentScreen = newMenu;
}



const std::string& slimConnectionHandler::currentGroup(void)
{
	return state->currentGroup;
}

const char* slimConnectionHandler::uuid(void)
{
	return state->uuid;
}

char * slimConnectionHandler::volume(void)
{
	return &(state->volume);
}

enum slimConnectionHandler::anim_e* slimConnectionHandler::anim(void)
{
	return &(state->anim);
}

char *slimConnectionHandler::brightness(void)
{
	return &(state->brightness);
}


const musicFile& slimConnectionHandler::currentSong(void)
{
	return state->currentSong;
}


// This is were the network data comes in
bool slimConnectionHandler::processRead(const void *data, size_t len)
{
	bool keepConnection = true;
	bool canAppend = rxBuf.end + len <= sizeof(rxBuf.data);   //new data can be appended to the buffer

	if(!canAppend)
	{
		db_printf(1,"message is too long, dropping it\n");  //not so nice to do
		return true;
	}

	memcpy(rxBuf.data + rxBuf.end, data, len);    //append data to the input buffer
	rxBuf.end += len;

	bool canValid  = rxBuf.end >= 8;   //might be a valid message
	while(canValid)
	{
		uint32_t opU32 = *(uint32_t*)(rxBuf.data+rxBuf.start);
		netBuffer buf( (char*)(rxBuf.data + rxBuf.start + 4 ) );
		uint32_t  mlen = buf;   //length of message excluding the operation code and lenght (so total is mlen+8 bytes)

		// Check if we have the whole message:
		if( rxBuf.end - rxBuf.start < (mlen+8) )
			break;

		// Do something with it:
		keepConnection = this->parseInput(opU32, buf, mlen);
		if(!keepConnection)
			return false;

		// If there's remaining data, copy it to front:
		memcpy( rxBuf.data, rxBuf.data + rxBuf.start + 8 + mlen, rxBuf.end - rxBuf.start - (mlen+8) );
		rxBuf.end -= (mlen+8);

		//In case there are multiple messages send at once:
		canValid  = rxBuf.end >= 8;   //might be a valid message
	}
	return keepConnection;
}



//---------------------------- Receive functions -----------------------------


// macro definition, so it can be used in switch/case statements
#ifdef WIN32
#define __BYTE_ORDER 1
#define __LITTLE_ENDIAN 1
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#define slimOpCode( ch0, ch1, ch2, ch3 )				\
		( (uint32_t)(uint8_t)(ch3) | ( (uint32_t)(uint8_t)(ch2) << 8 ) |	\
		( (uint32_t)(uint8_t)(ch1) << 16 ) | ( (uint32_t)(uint8_t)(ch0) << 24 ) )
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define slimOpCode( ch0, ch1, ch2, ch3 )				\
		( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |	\
		( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )
#endif


// Handle a complete message
bool slimConnectionHandler::parseInput(uint32_t opU32, netBuffer& buf, int len )
{

	bool keepConnection = true;
	//determine operation type:
	switch(opU32)
	{
	case slimOpCode('H','E','L','O'):
		helo(buf,len);  break;
	case slimOpCode('B','Y','E','!'):
		keepConnection = false;
		break;
	case slimOpCode('I','R',' ',' '):
		ir(buf,len);	break;
	case slimOpCode('S','T','A','T'):
		stat(buf,len);  break;
	case slimOpCode('R','E','S','P'):
		resp(buf,len);	break;
	case slimOpCode('A','N','I','C'):
		state->anim = ANIM_NONE;
		if( menu->display->refreshAfterAnim )
			menu->display->draw('c');	//update screen, if required
		//db_printf(3,"< Animation complete\n");
		break;
	default:
		//debug:
		char *op = (char*)(&opU32);
		db_printf(1,"< Operation %c%c%c%c, length %i\n",
			op[0], op[1], op[2], op[3], len);
		//db_printf(1,"unknown op: %c%c%c%c\n", op[0], op[1], op[2], op[3]);
	} //switch(opU32)

	return keepConnection;
}




// Handle a helo message, parse device information, and initialize the device:
void slimConnectionHandler::helo(netBuffer& buf, size_t len)
{

	device.ID       = buf;
	device.revision = buf;
	for(int i=0; i<6; i++)
		device.mac[i] = buf;

	//some newer devices send this:
	if( len > 20)
		for(int i=0; i<6; i++)
			device.uuid[i] = buf;
	//all devices send the wlan-channels:
	uint16_t channels = buf;
	device.channels    = channels & 0x3FFF;
	device.hasGraphics = (channels & 0x8000) != 0;
	device.reconnect   = (channels & 0x4000) != 0;

	//most devices send this:
	if( len > 10 )
	{
		device.recv     = buf;
		device.lang[0]  = buf;
		device.lang[1]  = buf;
	}

	//TODO: read capabilities (comma separated string)
	if(len > 36) { }


	//Initialize the display:
	sprintf( state->uuid, "%02x:%02x:%02x:%02x:%02x:%02x",
		device.mac[0], device.mac[1], device.mac[2], device.mac[3], device.mac[4], device.mac[5] );
	//register to the central system:
	ipc->addDevice( state->uuid, this);

	//debug output:
	db_printf(1,"helo(%llu): Id = %s.%i, uuid = %s, #recv = %llu\n",
		(LLU)len, sqDeviceName(device.ID), device.revision, state->uuid, (LLU)device.recv );


	//code from Slim/networking/slimproto.pm, line 1260:
	setAudio(true,true);
	setVolume(state->volume);

	char version[] = "7.5";    //high version, to make the client not complain
	send("VERS", sizeof(version), version);	//not required ??

	setBrightness(state->brightness);
	//visualization(false, 0 ); //not complete yet
	menu->display->print("Welcome");
	menu->main->draw('c');


	//init: stop all previous streams
	stream.command   = 'q';
	stream.autostart = '1';
	STRM();

	/*
	//for debug: start streaming a shoutcast stream:
	stream.command = 's';	//start the stream
	//stream.serverIP   = playlist[0].ip;
	//stream.serverPort = playlist[0].port;
	stream.serverIP   = 0;
	stream.serverPort = ipc->shoutServer->getPort();
	stream.autostart  = '1';
	STRM();
	*/

	//send("stat",0,0);	//request status
}



// Parse a remote-control packet:
void slimConnectionHandler::ir(netBuffer& buf, int len)
{
	uint32_t timeOn;
	uint8_t  format;
	uint8_t  longCode;
	uint32_t code;
	commands_e cmd = cmd_invalid;

	timeOn = buf;
	format = buf;
	longCode = buf; //16 or 32 bits IR-codes ?
	if(longCode == 0)
		code = (uint16_t)buf;
	else
		code = (uint32_t)buf;

	//Convert from device-specific to generic code:
	cmd = cmd_invalid;
	switch(format)
	{
	case 0:	//squeezebox v3
	case 1:	//softsqueeze
		for(unsigned i=0; i < array_size(IRmap_SB); i++)
			if( IRmap_SB[i].code == code)
			{ cmd = IRmap_SB[i].cmd; break; }
			break;
	default:
		break;
	}

	//Filter out double-presses:
	uint32_t dt = timeOn - IRdata.prevTime;   //let it underflow on purpose
	//TODO:  enable key-repeat, if it is hold for a longer time.

	//Volume keys are handled specially, for quick response:
	if( (cmd == cmd_Vup) || (cmd == cmd_Vdown) )
	{
		if( state->currentScreen != this->menu->volumeScreen)
		{
			menu->volumeScreen->setParent( state->currentScreen );
			state->currentScreen = menu->volumeScreen;
		}
		menu->volumeScreen->command(cmd);
		//break;
	}
	else if( (code == IRdata.prevCode) && (dt < 200) )
	{
		//double key-press
	}
	else
	{
		db_printf(1,"IR: time %u, format %i, 32bits: %i, code 0x%x, cmd %s. dt = %u\n",
			timeOn, format, longCode, code, commandsStr[cmd], dt);

		char str[30];
		rect_t win = {0,160,0,16};
		bool needsRedraw = true;

		//Make sure some commands are always handled here:
		if( cmd == cmd_pwr )
		{
			state->power = !state->power;
			if( state->power ) {
				setBrightness(state->brightness - 1);	//turn backlight on
				setAudio(true,true);					//turn on D/A converters
				setVisualization( state->currentViso );	//restore visualization
			} else {
				stop();					//stop playback
				setBrightness( -1 );	//turn display off
				setAudio(false,false);	//turn off D/A converters
				setVisualization( 0 );	//disable visualization
			}
		}

		//Let the current menu override all others keys, if desired:
		bool screenHandledIt = state->currentScreen->command(cmd);

		if( screenHandledIt ) {
			needsRedraw = false;
		} else {
			switch(cmd)
			{
			//menu system
			case cmd_browse:
				state->currentScreen = menu->folderMenu;		break;
			case cmd_search:
				state->currentScreen = menu->searchMenu;		break;
			case cmd_playing:
				if( state->currentScreen != menu->nowPlayingScreen)
				{
					db_printf(1,"current = %p, nowPlating = %p\n", state->currentScreen , menu->nowPlayingScreen );
					state->currentScreen = menu->nowPlayingScreen;
				}
				else
				{	//toggle between visualization modes
					state->currentViso = (state->currentViso + 1) % array_size(visu::defVisu);
					//db_printf(1,"Setting visualization %i\n", state->currentViso );
					setVisualization( state->currentViso );
				}
				break;
			case cmd_favorites:
				state->currentScreen = menu->favorites;			break;


				//playback:
			case cmd_play:
				//TODO: move this into menus.
				//	while searching/browsing this modifies playlist. in now-Playing it does pause/unpause
				if( state->playState == state_s::PL_PLAY)
					pause();
				else
					play();
				break;
			case cmd_pause:
				if( state->playState == state_s::PL_PLAY)
					pause();
				else if( state->playState == state_s::PL_PAUSE)
					unpause();
				break;
			case cmd_rewind:
				//pass a command to the IPC, IPC will send out play commands to all connected clients
				this->ipc->seekList( state->currentGroup, -1, SEEK_CUR);
				break;
			case cmd_forward:
				this->ipc->seekList( state->currentGroup,  1, SEEK_CUR);
				break;


				//others:
			case cmd_brightness:
				state->brightness = ( (state->brightness|1) + 2) % 6;
				setBrightness(state->brightness - 1);	//minus one to allow display to be fully off
				break;
			case cmd_0:
				//test if the display works:
				sprintf(str,"0 key %u = %s\n", code, commandsStr[cmd] );
				menu->display->gotoxy(0,0);
				menu->display->rect( win , 0);
				menu->display->print(str);
				break;
			case cmd_invalid:
				sprintf(str,"unknown code 0x%x\n", code );
				menu->display->gotoxy(0,0);
				menu->display->rect( win , 0);
				menu->display->print(str);
				break;
			default:
				sprintf(str,"key %u = %s\n", code, commandsStr[cmd] );
				menu->display->gotoxy(0,0);
				menu->display->rect( win , 0);
				menu->display->print(str);	//print, but don't send image data.
				break;
			} //switch(cmd)
		}

		// Update here, for now. should be done more intelligently
		if(needsRedraw)
			state->currentScreen->draw();
	}
	IRdata.prevCode = code;
	IRdata.prevTime = timeOn;
}



void slimConnectionHandler::stat(netBuffer& buf, int len)
{

	buf.read( &status.eventCode,4);	//disable byte-order conversion
	status.nrCrLF		= buf;
	status.masInit		= buf; //'m' or 'p'
	status.masMode		= buf;
	status.bufSize		= buf; //player buffer size
	status.bufData		= buf; //bytes in player buffer
	status.nrRecv		= buf; //# bytes received
	status.signal		= buf; //wireless signal strength
	status.jiffies		= buf; //player timestamp
	status.bufSizeOut	= buf;
	status.bufDataOut	= buf;
	uint32_t songSec	= buf;	//number of seconds into the song
	status.voltage		= buf;
	status.songMSec		= buf;
	//uint32_t timeStamp = buf;

	(void)songSec;		//remove compiler warning against unused variable
	//songSec = max( songSec, status.songMSec );	//not sure if they are always both given..

	if( status.eventCode == slimOpCode('S','T','M','h') )
	{
		db_printf(2,"<\tHeader has been parsed\n");
	}
	else if( status.eventCode == slimOpCode('S','T','M','s') )
	{
		db_printf(2,"<\tplayback has started\n");
		state->currentSong = ipc->getSong( state->currentGroup );
	}
	else if( status.eventCode == slimOpCode('S','T','M','t') )
	{
		db_printf(2,"<\theartbeat: pos = %.1f sec\n", (status.songMSec+500) / 1e3 );
	}
	else if( status.eventCode == slimOpCode('S','T','M','c') )
	{
		db_printf(2,"<\tConnect???\n");
	}
	else if( status.eventCode == slimOpCode('S','T','M','d') )
	{
		db_printf(2,"<\tplayer is ready for the next track\n");
		ipc->seekList( state->currentGroup, 1, SEEK_CUR, false);	//start a new song, but don't stop the current one yet
	}
	else if( status.eventCode == slimOpCode('S','T','M','u') )
	{
		db_printf(2,"<\tdata underrun\n");
		ipc->seekList( state->currentGroup, 1, SEEK_CUR, true);	//abort current stream, start something new.
	} else {
		char event[5] = {0,0,0,0,0};
		*((uint32_t*)event) = status.eventCode;
		db_printf(2,"<Stat(%i): %s. song %u ms, %lli recv\n", len, event, status.songMSec, (long long)status.nrRecv);
		db_printf(2,"<\tbufSize %u, bufOut %u/%u, #crlf %i\n", status.bufSize, status.bufDataOut, status.bufSizeOut, status.nrCrLF);
	}

	//update the display:
	state->currentScreen->draw();
}



//--------------------------- Send functions ----------------------------


/// Send function.
///  TODO: check for cmd='grfe' and add LZF compression: http://home.schmorp.de/marc/liblzf.html
///  See squeezeCenter\Slim\Player\SqueezeBox.pm, sub sendFrame {}
int slimConnectionHandler::send(const char cmd[4], uint16_t len, void *data)
{
	//setup the header:
	char hdr[6];
	netBuffer buf(hdr);
	buf.write( (uint16_t)(4 + len) );	//write length of cmd+data
	buf.write(cmd,4);

	db_printf(3,"sending %c%c%c%c: %i bytes\n", cmd[0], cmd[1], cmd[2], cmd[3], len);

	write( new nbuffer::bufferMem(hdr,    6, true) );
	write( new nbuffer::bufferMem(data, len, true) );
	return 0;
}


void slimConnectionHandler::setAudio(bool spdif, bool dac)
{
	char data[2];
	data[0] = spdif;
	data[1] = dac;
	send("aude", 2, data);
}


void slimConnectionHandler::setBrightness(char brightness)
{
	uint16_t br = htons(brightness);
	send("grfb", 2, &br );
}


/// Convert volume from floating point dB's to the hardware fixed point 16.16 representation
uint32_t dBToFixed(float db)
{
	uint32_t fixed = 0;
	float floatmult = powf(10.0f, db / 20.0f);

	if ((db >= -30.0f) && (db <= 0.0f))
		fixed = static_cast<unsigned int>(floatmult * 256.0f + 0.5f) << 8;
	else
		fixed = static_cast<unsigned int>((floatmult * 65536.0f) + 0.5f);
	return fixed;
}



/// Volume, ranging from 0..100
void slimConnectionHandler::setVolume(uint8_t newVol)
{
	float localVol = util::clip<uint8_t>(newVol,0, 100);	// range 0..100
	uint32_t oldGain = s_volumes[static_cast<uint32_t>(localVol)];

	uint32_t newGain = 0;
	if (localVol > 0.0f)
	{
		float db = (localVol - 100.0f) / 2.0f;
		newGain = dBToFixed(db);
	}

	char data[18];
	netBuffer buf(data);
	buf.write( (uint32_t) oldGain ); //left
	buf.write( (uint32_t) oldGain ); //right
	buf.write( (uint8_t)  1 );			//digital volume control
	buf.write( (uint8_t)  255 );		//pre-amp
	buf.write( (uint32_t) newGain );	//left, new format???
	buf.write( (uint32_t) newGain );	//right, new format
	assert( buf.idx == sizeof(data) );
	send("audg", sizeof(data), data);
}


// Low-level streaming command.
// See squeezeCenter\Slim\Player\SqueezeBox.pm, sub stream_s{} and sub stream{}
// look for "my $frame = pack"
void slimConnectionHandler::STRM()
{
	char cmd[100];

	netBuffer buf(cmd);
	//if( stream.command == 's')
	{
		buf.write( stream.command );
		buf.write( stream.autostart );
		buf.write( stream.format );
		buf.write( stream.pcmSampleSize );
		buf.write( stream.pcmSampleRate );
		buf.write( stream.pcmChannels );
		buf.write( stream.pcmEndian );
		buf.write( stream.preBufferSilence );
		buf.write( stream.spdif );
		buf.write( stream.transitionDuration );
		buf.write( stream.transitionType );
		buf.write( stream.flags );
		buf.write( stream.outputThreshold );
		//buf.write( stream.visport );
		//buf.write( stream.visip );
		buf.write( stream.reserved );

		//when pausing, this is interpreted as 'unpauseAt'
		//	for command 'a' and 'p' it's ms, for 'u' it's seconds ???
		//	't' is a timestamp, but what does it do?
		if( stream.command == 's')
			buf.write( stream.replayGain );
		else
			buf.write( (uint32_t)0 );

		buf.write( stream.serverPort );
		buf.write( (uint32_t)ntohl(stream.serverIP) );	//convert back and forth: network->host->network
	}
	assert( buf.idx == 24);

	//Send the data:
	if( stream.command == 's')
	{
		char *http = &buf.data[buf.idx];

		// Append the http-request-header to the end of the stream:
		if( stream.serverIP == 0)
		{	// Local file, rename the url
			sprintf(http,"GET /stream.mp3?player=%s HTTP/1.0\r\n\r\n", state->uuid );
		} else
		{	// Remote file, keep the url, but do request meta-data:
			sprintf(http,"GET %s HTTP/1.0\r\nIcy-MetaData: 1\r\n\r\n", stream.url.c_str() );
		}
		printf(">STRM(%s)\n", http );

		send("strm", buf.idx + strlen(http), cmd);
	}
	else
	{
		send("strm", buf.idx, cmd);
	}
}



//--------------------------- Playback control ----------------------------------------


void slimConnectionHandler::play()
{
	int currentItem = ipc->getList( state->uuid )->currentItem;
	menu->menuPlayList->currentItem = currentItem;
	musicFile data = ipc->getList( state->uuid )->get(  currentItem  );

	stream.format = data.format;
	stream.pcmChannels = data.nChannels + '0';
	stream.setSampleSize( data.nBits );
	stream.setSampleRate( data.sampleRate );

	stream.url = data.url;
	stream.serverIP  = data.ip;
	stream.serverPort= data.port;
	db_printf(1,"play(): %08x:%i GET %s\n", data.ip, data.port, stream.url.c_str() );

	// 'smart' replay-gain:
	bool sameAlbum = false;
	if( currentItem > 0 )
		if( data.album == ipc->getList(state->uuid)->get(currentItem-1).album )
			sameAlbum = true;
	if( currentItem < (int)ipc->getList(state->uuid)->items.size() - 2 )
		if( data.album == ipc->getList(state->uuid)->get(currentItem+1).album )
			sameAlbum = true;

	if( sameAlbum )
		stream.replayGain = dBToFixed( data.gainAlbum );
	else
		stream.replayGain = dBToFixed( data.gainTrack );


	stream.command    = 's';	//start the stream
//	stream.serverIP   = 0;		//inet_addr( "127.0.0.1" );
//	stream.serverPort = ipc->getShoutPort();
	stream.autostart  = '1';	//quite important, don't know exactly what it means
	STRM();
	state->playState = state_s::PL_PLAY;
}


void slimConnectionHandler::pause()
{
	stream.command    = 'p';
	STRM();
	state->playState = state_s::PL_PAUSE;
}


/// This is also used to indicate to the client that all song-data is sent to the buffer.
void slimConnectionHandler::unpause()
{
	stream.command    = 'u';
	STRM();
	state->playState = state_s::PL_PLAY;
}


void slimConnectionHandler::stop()
{
	stream.command    = 'q';
	STRM();
	state->playState = state_s::PL_STOP;
}

