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
	"add","search","browse","now playing",
	"size","brightness",
	"power","sleep","power On","power Off",
};



//--------------------------- Utility functions ------------------------------




//--------------------------- Tie connection to the server -------------------


TCPserverSlim::TCPserverSlim(slimIPC *ipc, int port, int maxConnections) :
	TCPserver(port,maxConnections),
		ipc(ipc)
	{
		ipc->slimServer = this;
		printf("slimProto: port %i, data port %i\n", port, ipc->shoutServer->getPort());
	}


connectionHandler* TCPserverSlim::newHandler(SOCKET socketFD)
{
	return new slimConnectionHandler(socketFD,ipc);
}


//--------------------------- SlimProto --------------------------------------


slimConnectionHandler::slimConnectionHandler(SOCKET socketFD, slimIPC *ipc) :
connectionHandler(socketFD),
ipc(ipc)
{
	//Initialize the display (display needs device information):
	display = new slimDisplay(this);

	//Initialize the menus:
	// for refernece, slim's menu can be found at:
	//	http://wiki.slimdevices.com/index.php/UserInterfaceHierarchy
	mainMenu = new slimMenu(display, "Squeezebox Home");
	state.currentScreen = mainMenu;

	folderMenu		= new slimBrowseMenu(display, mainMenu);
	nowPlayingScreen= new slimPlayingMenu(display, mainMenu);

	searchMenu		= new slimMenu(display, "Search", mainMenu);
	searchAlbum		= new slimSearchMenu(display, searchMenu, DB_ALBUM);
	searchArtist	= new slimSearchMenu(display, searchMenu, DB_ARTIST);
	menuPlayList	= new slimPlayListMenu(display, nowPlayingScreen);

	searchMenu->addItem("Albums", searchAlbum);
	searchMenu->addItem("Artist", searchArtist);
	//searchMenu->addItem("Title", searchTitle);
	nowPlayingScreen->setPlayMenu(menuPlayList);

	//	tie them together:
	mainMenu->addItem("Now Playing", nowPlayingScreen);
	mainMenu->addItem("Folders",	folderMenu );
	mainMenu->addItem("Search",		searchMenu );
	mainMenu->addItem("Settings",	mainMenu );		//todo: write menu

	//Some other data:
	memset( &IRdata, 0, sizeof(IRdata) );
	memset( &device, 0, sizeof(device) );
	memset( &status, 0, sizeof(status) );
}



/// Deconstructor, closes the connection, deregisters the player
slimConnectionHandler::~slimConnectionHandler()
{
	ipc->delDevice( this->state.uuid );
	delete mainMenu;
	delete display;
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


// Handle a complete message
bool slimConnectionHandler::parseInput(uint32_t opU32, netBuffer& buf, int len )
{
	bool keepConnection = true;
	//determine operation type:
	switch(opU32)
	{
	case mmioFOURCC('H','E','L','O'):
		helo(buf,len);  break;
	case mmioFOURCC('B','Y','E','!'):
		keepConnection = false;
		break;
	case mmioFOURCC('I','R',' ',' '):
		ir(buf,len);	break;
	case mmioFOURCC('S','T','A','T'):
		stat(buf,len);  break;
	case mmioFOURCC('R','E','S','P'):
		resp(buf,len);	break;
	case mmioFOURCC('A','N','I','C'):
		db_printf(3,"< Animation complete\n");
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
	sprintf(this->state.uuid, "%02x:%02x:%02x:%02x:%02x:%02x",
		device.mac[0], device.mac[1], device.mac[2], device.mac[3], device.mac[4], device.mac[5] );
	//state.currentGroup =
	ipc->addDevice( this->state.uuid, this);

	//debug output:
	db_printf(1,"helo(%zu): Id = %s.%i, uuid = %s, #recv = %lli\n",
		len, sqDeviceName(device.ID), device.revision, state.uuid, (long long)device.recv );


	//code from Slim/networking/slimproto.pm, line 1260:
	setAudio(true,true);
	setVolume(state.volume);

	char version[] = "7.5";    //high version, to make the client not complain
	send("VERS", sizeof(version), version);	//not required ??

	setBrightness(state.brightness);
	//visualization(false, 0 ); //not complete yet
	display->print("Welcome",true);
	mainMenu->draw();


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

	if( (code == IRdata.prevCode) && (dt < 200) )
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

		//All other keys are handled by the current menu:
		bool screenHandledIt = state.currentScreen->command(cmd);

		if( !screenHandledIt )
		{
			switch(cmd)
			{
				//menu system
			case cmd_browse:
				state.currentScreen = this->folderMenu;		break;
			case cmd_search:
				state.currentScreen = this->searchMenu;		break;
			case cmd_playing:
				state.currentScreen = this->nowPlayingScreen;	break;

				//playback:
			case cmd_play:
				//TODO: move this into menus.
				//	while searching/browsing this modifies playlist. in now-Playing it does pause/unpause
				if( state.playState == state.PL_PLAY)
					pause();
				else
					play();
				break;
			case cmd_pause:
				if( state.playState == state.PL_PLAY)
					pause();
				else if( state.playState == state.PL_PAUSE)
					unpause();
				break;
			case cmd_rewind:
				//pass a command to the IPC, IPC will send out play commands to all connected clients
				this->ipc->seekList( state.currentGroup, -1, SEEK_CUR);
				break;
			case cmd_forward:
				this->ipc->seekList( state.currentGroup,  1, SEEK_CUR);
				break;
			case cmd_Vup:
				state.volume = util::clip(state.volume + 10, 0, 100);
				setVolume(state.volume);
				break;
			case cmd_Vdown:
				state.volume = util::clip(state.volume - 10, 0, 100);
				setVolume(state.volume);
				break;

				//others:
			case cmd_brightness:
				state.brightness = (state.brightness+1) % 5;
				setBrightness(state.brightness);
				break;
			case cmd_0:
				//test if the display works:
				sprintf(str,"0 key %u = %s\n", code, commandsStr[cmd] );
				display->gotoxy(0,0);
				display->rect( win , 0);
				display->print(str,false);
				break;
			case cmd_invalid:
				sprintf(str,"unknown code 0x%x\n", code );
				display->gotoxy(0,0);
				display->rect( win , 0);
				display->print(str,false);
				break;
			default:


				sprintf(str,"key %u = %s\n", code, commandsStr[cmd] );
				display->gotoxy(0,0);
				display->rect( win , 0);
				display->print(str,false);	//print, but don't send image data.
				break;
			}
		} //if (!screenHandledIt)

		// Update here, for now. should be done more intelligently
		if(needsRedraw)
			state.currentScreen->draw();
	}
	IRdata.prevCode = code;
	IRdata.prevTime = timeOn;
}



void slimConnectionHandler::stat(netBuffer& buf, int len)
{
	char event[5];
	event[4] = 0;

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
	status.songMSec		= ((uint32_t)buf) + songSec*1000;
	//uint32_t timeStamp = buf;

	*((uint32_t*)event) = status.eventCode;

	if( status.eventCode == mmioFOURCC('S','T','M','h') )
	{
		db_printf(2,"<\tHeader has been parsed\n");
	}
	else if( status.eventCode == mmioFOURCC('S','T','M','s') )
	{
		db_printf(2,"<\tplayback has started\n");
	}
	else if( status.eventCode == mmioFOURCC('S','T','M','t') )
	{
		db_printf(2,"<\theartbeat: pos = %.1f sec\n", status.songMSec / 1e3 );
	}
	else if( status.eventCode == mmioFOURCC('S','T','M','c') )
	{
		db_printf(2,"<\tConnect???\n");
	}
	else if( status.eventCode == mmioFOURCC('S','T','M','d') )
	{
		db_printf(2,"<\tplayer is ready for the next track\n");
		ipc->seekList( state.currentGroup, 1, SEEK_CUR, false);	//start a new song, but don't stop the current one yet
	}
	else if( status.eventCode == mmioFOURCC('S','T','M','u') )
	{
		db_printf(2,"<\tdata underrun\n");
		ipc->seekList( state.currentGroup, 1, SEEK_CUR, true);	//abort current stream, start something new.
	} else {
		db_printf(2,"<Stat(%i): %s. song %u ms, %lli recv\n", len, event, status.songMSec, (long long)status.nrRecv);
		db_printf(2,"<\tbufSize %u, bufOut %u/%u, #crlf %i\n", status.bufSize, status.bufDataOut, status.bufSizeOut, status.nrCrLF);
	}

	//update the display:
	state.currentScreen->draw();
}



//--------------------------- Send functions ----------------------------


/// Send function.
///  TODO: check for cmd='grfe' and add LZF compression: http://home.schmorp.de/marc/liblzf.html
///  TODO: don't block on write
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
	send("GRFB", 2, &br );
}



// Low-level streaming command.
// See squeezeCenter\Slim\Player\SqueezeBox.pm, sub stream_s
// look for "my $frame = pack"
void slimConnectionHandler::STRM()
{
	char cmd[100];

	netBuffer buf(cmd);
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
	buf.write( stream.replayGain );
	buf.write( stream.serverPort );
	buf.write( stream.serverIP );

	assert( buf.idx == 24);

	if( stream.command == 's')
	{
		char *http = &buf.data[buf.idx];
		// Append the http-request-header to the end of the stream:
		sprintf(http,"GET /stream.mp3?player=%s HTTP/1.0\r\n\r\n", state.uuid );
		send("strm", buf.idx + strlen(http), cmd);
	}
	else
		send("strm", buf.idx, cmd);
}



//--------------------------- Playback control ----------------------------------------


void slimConnectionHandler::play()
{
	menuPlayList->currentItem = ipc->getList( state.uuid )->currentItem;

	//TODO: use ipc-> commands, so all player in the current group start playing.
	stream.command    = 's';	//start the stream
	stream.serverIP   = 0; //inet_addr( "127.0.0.1" );
	stream.serverPort = ipc->shoutServer->getPort();
	stream.autostart  = '1';
	STRM();
	state.playState = state.PL_PLAY;
}


void slimConnectionHandler::pause()
{
	stream.command    = 'p';
	STRM();
	state.playState = state.PL_PAUSE;
}

/// This is also used to indicate to the client that all song-data is sent to the buffer.
void slimConnectionHandler::unpause()
{
	stream.command    = 'u';
	STRM();
	state.playState = state.PL_PLAY;
}

void slimConnectionHandler::stop()
{
	stream.command    = 'q';
	STRM();
	state.playState = state.PL_STOP;
}

