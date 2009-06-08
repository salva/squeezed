/**
\mainpage
SqueezeD is a tiny server for slim-devices' squeezeBox.
It consists of a single application, which runs two threads:

The \ref slimProto and the \ref shoutProto.

The \ref slimProto talks to the hardware; it receives remote-control codes,
updates the display, sets the volume and prepares the client for streaming.

The \ref shoutProto is the file server

Both servers use \ref slimIPC to talk to each other. This module also handles
communications between multiple clients (synchronizing playlists).

A separate part is the \ref musicDB. At this moment it is very simple, it handles
basic searches, but indexing is not very fast.

More detailed documentation can be found on the <a href="modules.html">Modules</a> page.
*/

/** file implementation of the SLIM Protocol
 * \date 4-16-09
 * This file implements the Slim protocol, which does the low-level
 * control of the slim clients. It sets the volume, controls the display
 * and accepts IR codes.
 */

/**
 * \defgroup slimProto The Slim protocol
 *@{*/



#ifndef _SLIMPROTO_HPP_
#define _SLIMPROTO_HPP_

#include <stdint.h>

#include <memory.h>
#include <string>
#include <cstdlib>
#include <assert.h>
#include <math.h>

//for network-byte-order conversion:
#ifdef WIN32
	//#define NOMINMAX
	#include <Winsock2.h>
#else
	//#include <netinet/in.h>
	#include <arpa/inet.h>
#endif


//#include "slimDisplay.hpp"
#include "TCPserver.hpp"
#include "slimIPC.hpp"
//#include "musicDB.hpp"
#include "util.hpp"
#include "debug.h"


//msvc madness:
#ifdef WIN32
#pragma warning (disable: 4996)
#endif



//--------------------------- interal functions ------------------------------



inline uint64_t ntohll(uint64_t n)
{
	return (uint64_t(ntohl(uint32_t(n))) << 32) | uint64_t(ntohl(uint32_t(n >> 32)));
}





enum commands_e
{
    cmd_invalid,
    cmd_0, cmd_1, cmd_2, cmd_3, cmd_4, cmd_5, cmd_6, cmd_7, cmd_8, cmd_9,
    cmd_Vup, cmd_Vdown,
    cmd_left, cmd_right, cmd_up, cmd_down,
	cmd_play, cmd_pause, cmd_rewind, cmd_forward,
	cmd_shuffle, cmd_repeat,
	cmd_add, cmd_search, cmd_favorites, cmd_browse, cmd_playing,
	cmd_size, cmd_brightness,
	cmd_pwr, cmd_sleep, cmd_pwrOn, cmd_pwrOff,
};

extern const char *commandsStr[];	//string lookup table for commands_e

//map IR-codes to device independent commands:
struct IRmap {
    uint32_t   code;
    commands_e cmd;
};



//--------------------------- External functions ----------------------------





/// Read from packet-buffer, and perform endiannes conversion
class netBuffer
{
private:

public:
    char *data; //pointer to start of the buffer
    int  idx;	//current index into the buffer


	netBuffer(void *data=NULL)
    {
        reset(data);
    }

    void reset(void *data)
    {
        this->data = (char*)data;
        idx = 0;
    }


    //write functions:
	void write(const void *src, size_t n)
	{
        char *p = (char*)src;
        for(size_t i=0; i< n; i++)
		{
            data[idx] = p[i];
			idx++;
		}
	}

	void write(char v)
	{
		data[idx++] = v;
	}

	void write(uint8_t v)
    {
		data[idx++] = v;
    }


	void write(uint16_t v)
    {
        uint16_t *p = (uint16_t*)(&data[idx]);
        idx += sizeof(uint16_t);
        *p = htons(v);
    }

	void write(uint32_t v)
	{
        uint32_t *p = (uint32_t*)(&data[idx]);
        idx += sizeof(uint32_t);
        *p = htonl(v);
	}


    //read array/string, without byte-order conversion
    void read(void *dst, size_t n)
    {
        char *p = (char*)dst;
        for(size_t i=0; i< n; i++)
		{
            p[i] = data[idx];
			idx++;
		}
    }

    // implement read functions using the conversion operator
    operator char()
    {
        return (char)data[idx++];
    }

    operator uint8_t()
    {
        return (uint8_t)data[idx++];
    }

    operator uint16_t()
    {
        uint16_t *p = (uint16_t*)(&data[idx]);
        idx += 2;
        return ntohs( *p );
    }

    operator uint32_t()
    {
        uint32_t *p = (uint32_t*)(&data[idx]);
        idx += sizeof(uint32_t);
        return ntohl( *p );
    }

    operator uint64_t()
    {
        uint64_t *p = (uint64_t*)(&data[idx]);
        idx += sizeof(uint64_t);
        return ntohll( *p );
    }

};




const unsigned char s_volumes[] =
{
	  0,   1,   1,   1,   2,   2,   2,   3,
	  3,   4,   5,   5,   6,   6,   7,   8,
	  9,   9,  10,  11,  12,  13,  14,  15,
	 16,  16,  17,  18,  19,  20,  22,  23,
	 24,  25,  26,  27,  28,  29,  30,  32,
	 33,  34,  35,  37,  38,  39,  40,  42,
	 43,  44,  46,  47,  48,  50,  51,  53,
	 54,  56,  57,  59,  60,  61,  63,  65,
	 66,  68,  69,  71,  72,  74,  75,  77,
	 79,  80,  82,  84,  85,  87,  89,  90,
	 92,  94,  96,  97,  99, 101, 103, 104,
	106, 108, 110, 112, 113, 115, 117, 119,
	121, 123, 125, 127, 128
};


class slimScreen;
class playList;
class musicFile;



/// Low-level slim-protocol handling
class slimConnectionHandler : public connectionHandler
{
	//friend class slimScreen;
	friend class slimPlayingMenu;
	//friend class slimPlayListMenu;	//need to change this...
	//friend class slimVolumeScreen;
	friend class slimIPC;
	friend class slimDisplay;
public:
	slimIPC *ipc;		    ///< connection to the data-server

	struct state_s;
	struct menu_s;
private:


	struct rxBuf_s {
		char    data[1<<10];   ///< input buffer, doens't need to be big for the slim-protocol
		size_t  start;		///< current start(read) position
		size_t	end;		///< current end(write) position
		rxBuf_s(): start(0), end(0)
			{};	//initialize to zero
	} rxBuf;


	/// IR/keyboard status
    struct {
        uint32_t prevCode;
        uint32_t prevTime;
    } IRdata;



	/// Menu-system
	struct menu_s *menu;


	/// Connection state
	struct state_s *state;


	/// Streaming status
    class Stream
	{
	public:
        char	command;		// {S,P,U,Q,T}  (start,pause,unpause,quit,timer}
        char	autostart;		// {'1'=streaming} || {'2' direct streaming }
        char	format;			// [p]cm, [f]lac, [m]p3
		char	pcmSampleSize;		//'0'=8, '1'=16, '2' = 24, '3' = 32
        char	pcmSampleRate;		//'0'=11kHz, '1'=22kHz, '2'=32kHz, '3'=44.1, '4' = 48, '5' = 8, '6' = 12, '7' = 16, '8' = 24, '9' = 96
        char	pcmChannels;		// '1' = mono, '2' = stereo
        char	pcmEndian;			//'0' = big, '1' = little
        uint8_t preBufferSilence;	//Kb of input buffer data before we autostart or notify the server of buffer fullness
        char	spdif;				//'0' = auto, '1' = on, '2' = off
        uint8_t transitionDuration;
        char	transitionType;		// [1]	'0' = none, '1' = crossfade, '2' = fade in, '3' = fade out, '4' fade in & fade out
        uint8_t flags;				//0x80:  loop, 0x40: gapless, 0x01: invert left, 0x02: invert right
		uint8_t outputThreshold;	// [1]	Amount of output buffer data before playback starts in tenths of second.
		uint8_t reserved;
		uint32_t replayGain;	//16.16 bits integer
		/*uint16_t visport;   //default 0;
        uint32_t visip;     //default 0;*/
        uint16_t serverPort;	// default 9000
        uint32_t serverIP;		// 0 implies control server, 1 means squeezenetwork?

        std::string url;		//for network streaming, the url used in the GET command

		void setSampleSize(int nrBits)
		{
			if(nrBits == 8)	pcmSampleSize = '0';
			if(nrBits ==16)	pcmSampleSize = '1';
			if(nrBits ==24)	pcmSampleSize = '2';
			if(nrBits ==32)	pcmSampleSize = '3';
		}

		void setSampleRate(int Hz)
		{
			if(Hz == 11025)	pcmSampleRate = '0';
			if(Hz == 22050)	pcmSampleRate = '1';
			if(Hz == 44100)	pcmSampleRate = '2';
			if(Hz == 48000)	pcmSampleRate = '3';
			if(Hz ==  8000)	pcmSampleRate = '5';
			if(Hz == 12000)	pcmSampleRate = '6';
			if(Hz == 16000)	pcmSampleRate = '7';
			if(Hz == 24000)	pcmSampleRate = '8';
			if(Hz == 96000)	pcmSampleRate = '9';
		}

		// Setup default values (for mp3):
		Stream( )
		{
			command = 'S';
			autostart = '1';
			format = 'm';
			pcmSampleSize = '?';
			pcmSampleRate = '?';
			pcmChannels = '?';
			pcmEndian = '?';
			preBufferSilence = 255;
			spdif ='0';
			transitionDuration = 0;
			transitionType = '0';
			flags = 0;
			outputThreshold = 0;
			reserved = 0;
			replayGain = 0;
			serverPort = 9000;
			serverIP = 0;
		}
    } stream;


    //internal functions:

	/// Main parser routine:
	bool parseInput(uint32_t opU32, netBuffer& buf, int len);

    /// Parse a 'helo' packet
    void helo(netBuffer& buf, size_t len);

    /// Parse a remote-control packet:
    void ir(netBuffer& buf, int len);

    /// Parse a stat-packet:
    void stat(netBuffer& buf, int len);

    /// handle RESP message
	void resp(netBuffer& buf, int len)
	{
		buf.data[len-1] = 0;
		db_printf(3,"got http response(%i): %s\n\n", len, buf.data + 4);
	}



protected:
	/// Hardware info (non-changing)
    struct {
        char        ID;
        char        revision;
        uint8_t     mac[6];
        uint8_t     uuid[16];
        uint16_t    channels;		//wlan channels
        uint64_t    recv;			//number of data-stream bytes received
        char        lang[2];
        char        capabilities[64];  //optional, only for newer devices.
		bool		hasGraphics;
		bool		reconnect;	//don't know what this means
    } device;


	/// Hardware status (changing)
    struct {
        uint32_t eventCode;
        uint8_t  nrCrLF;
        uint8_t  masInit;	//'m' or 'p'
        uint8_t  masMode;
        uint32_t bufSize;	//player buffer size
        uint32_t bufData;	//bytes in player buffer
        uint64_t nrRecv;	//# bytes received
        uint16_t signal;	//wireless signal strength
        uint32_t jiffies;   //player timestamp
        uint32_t bufSizeOut;	///< Output buffer size
        uint32_t bufDataOut;    ///< Output data in buffer
        uint32_t songMSec;      ///< Elapsed millise seconds of current stream
        uint16_t voltage;
    } status;

    /// Low-level streaming command.
	/// See squeezeCenter\Slim\Player\SqueezeBox.pm, sub stream_s
	/// look for "my $frame = pack"
    void STRM(uint32_t skipMs = 200);


public:
    slimConnectionHandler(SOCKET socketFD, slimIPC *ipc);
    ~slimConnectionHandler();


	// Functions that allow display to change the state:
	enum anim_e {ANIM_NONE, ANIM_HW, ANIM_SW};

	/// set another menu, redraw the screen
	void setMenu(slimScreen *newMenu, char transition = 'c' );

	const std::string& currentGroup(void);

	const char* uuid(void);

	char *volume(void);

	anim_e *anim(void);

	char *brightness(void);

	const musicFile& currentSong(void);


    /// This is were the data comes in
    bool processRead(const void *data, size_t len);

    /// Low-level send command
    int send(const char cmd[4], uint16_t len, void *data);


	/// wrapper around all visualization modes:
	/*struct visu_s
	{
	private:
		int currentMode;
        static const int nrParam = 7;
        static const char data[2 + 4*nrParam];
        uint32_t *param;

	public:
        visu_s(void)
        {
        	param = (uint32_t*)(&data[2]);
        }
	};*/

    /// Set visualization
    /// Can chose from a limited number of pre-defined modes.
    void setVisualization(uint8_t mode);

    /// Enable audio outputs
    void setAudio(bool spdif, bool dac);

    /// Set brightness 0..4
    void setBrightness(char b);

	/// Volume, ranging from 0..100
	void setVolume(uint8_t newVol);

    /// Various commands the server can perform:
    void play();

    void pause();

	/// strm('u') is also used to indicate to the client that all song-data is sent to the buffer
	void unpause();

	void stop();

    /// Seek in the current playlist
    //void seek(int offset,int origin);

};



/// The base server class
///	This gives every connectionhandler acces to *ipc
class TCPserverSlim : public TCPserver
{
private:
	slimIPC *ipc;
public:
	TCPserverSlim(slimIPC *ipc, int port=3483, int maxConnections=10);

	connectionHandler* newHandler(SOCKET socketFD);
};


/**@}
 *end of doxygen slim-group
 */

#endif

