/* Functions to provide file information.
 * Currently the mime-type and audio-tag data
 *
 */

#if defined(WIN32)
// && !defined(__CYGWIN__)
	#pragma warning (disable: 4996)	//msvc madness on strncpy being unsafe
	#pragma warning (disable: 4244)	//w_char to char conversion
	#define _align1_ __declspec(align(1))
	#define _align_pre_(n)  __declspec(align(n))
	#define _align_post_(n)

	#include <winsock2.h>		//for ntohl under cygwin
#else
	#define _align_pre_(n)
//	#define _align_post_(n) __attribute__((aligned(n)))
	#define _align_post_(n) __attribute__((packed))


	#include <arpa/inet.h>	//for ntohl
#endif


//--------------------------- Includes -----------------------------------------

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <bitset>	//for mpeg header

#include "debug.h"
#include "util.hpp"
#include "fileInfo.hpp"


/// Get mime type, based on the extension of a filename
std::string getMime(const char *extension)
{
	const char *mimeTable[][2] = {
		{"mp3" , "audio/mpeg"},
		{"aif" , "audio/x-aiff"},
		{"wav" , "audio/x-wav"},
		{"flac", "audio/flac"},
		{"ogg" , "audio/ogg"},
		{"txt" , "text/plain"},
		{"html", "text/html"},
		{"htm" , "text/html"},
		{"png" , "image/png"},
		{"jpg" , "image/jpeg"},
		{"svg" , "image/svg+xml"},
		{"z"   , "application/x-compress"},
		{"m3u" , "audio/x-playlist"},		//not really an official type..
	};

	std::string mime;

	size_t mimeIdx = array_size(mimeTable);
	if( extension != NULL)
	{
		if( *extension == '.')
			extension++;

		for(mimeIdx=0; mimeIdx< array_size(mimeTable); mimeIdx++)
			if( strcasecmp(extension, mimeTable[mimeIdx][0]) == 0)
				break;
	}
	if( mimeIdx < array_size(mimeTable))
		mime = mimeTable[mimeIdx][1];
	else
		mime = "application/octet-stream";
	return mime;
}


//as defined by http://www.id3.org/id3v2-00
const char *id3v1Genres[] = {
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz",
	"Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", //techno = 18
	"Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno",
	"Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental",
	"Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul",
	"Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", //gothic = 49
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock",
	"Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
	"Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk",
	"Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
//	The following genres are Winamp extensions (starting from index 80)
	"Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic",
	"Bluegrass", "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock",
	"Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera",
	"Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", "Slow Jam",
	"Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock",
	"Drum Solo", "A capella", "Euro-House", "Dance Hall"
};



std::string clipString(const char *str, int maxLen)
{
	const char *end = (const char*)memchr(str, 0, maxLen);
	size_t len;
	if(end == NULL)
		len = maxLen;
	else
		len = end-str;

	//remove trailing whitspace
	for(end = str+len-1; end >= str; end--)
		if( *end != ' ')
			break;
	len = end-str+1;

	//convert to std::string
	std::string out(str, len);
	//out.assign(str, len);
	return out;
	//return std::string(str, len);
}

/// extract ID3v1 tags
int tagID3v1(FILE *f, std::map<std::string, std::string> &tags )
{
	_align_pre_(1) struct {
		char tag[3];
		char title[30];
		char artist[30];
		char album[30];
		char year[4];
		char comment[29];
		char trackNumber;
		uint8_t genre;
	} _align_post_(1) id3v1;
	assert( sizeof(id3v1) == 128);

	if( fseek( f, -128, SEEK_END) != 0)
		return -1;
	size_t n = fread( &id3v1, 128, 1, f);
	if( n != 1)		return -2;

	if( memcmp(id3v1.tag, "TAG", 3) != 0)
		return -3;

	tags["title"] = clipString(id3v1.title,   sizeof(id3v1.title) );
	tags["artist"]= clipString(id3v1.artist, sizeof(id3v1.artist) );
	tags["album"] = clipString(id3v1.album,   sizeof(id3v1.album) );
	tags["year"]  = clipString(id3v1.year,     sizeof(id3v1.year) );
	if( id3v1.genre < array_size(id3v1Genres) )
		tags["genre"] = id3v1Genres[id3v1.genre];
	return 0;
}


//convert 4*7bits size to single 32-bit value
uint32_t ID3size(uint32_t sizeIn)
{
	char *sizeBytes = (char*)&sizeIn;
	return (sizeBytes[0]<<21) + (sizeBytes[1]<<14) + (sizeBytes[2]<<7) + (sizeBytes[3]);
}

//very dirty:
std::string utf16_to_char(char *data, size_t len)
{
	std::string out;
	out.resize(len/2);
	for(size_t i=0; i < len; i+=2)
		out[i/2] = data[i];
	return out;
}


/// extract ID3v2 tags
/// TODO: unicode strings are not nicely
/// returns size of the header, in bytes, or <0 on error
int tagID3v2(FILE *f, std::map<std::string, std::string> &tags )
{
	_align_pre_(1) struct {
		char		ID[3];
		uint8_t		version[2];
		uint8_t		flags;
		uint32_t	size;
	} _align_post_(1) hdr;
	//db_printf(20,"sizeof(uint32_t) = %lu\n", sizeof(uint32_t));
	//db_printf(20,"sizeof(version) = %lu\n", sizeof(hdr.version));
	//db_printf(20,"sizeof(hdr) = %lu\n", sizeof(hdr));
	assert( sizeof(hdr) == 10);	//verify that struct-alignment is compiled correctly

	_align_pre_(1) struct {
		uint32_t	size;
		uint16_t	flags;
		uint32_t	paddingSize;
	} _align_post_(1) extHdr;

	_align_pre_(1) struct frame_s {
		char ID[4];
		uint32_t size;
		uint16_t flags;
		char	 data[1];
	} _align_post_(1) *frame;
	assert( sizeof(*frame) == 10 + sizeof(frame->data) );

	//re-mapping of frameID's
	const char *frameIDs[][2] = {
		{"TPE1", "artist"}, {"TALB","album"},
		{"TIT2", "title" }, {"TRCK","track"},
		{"TYER", "year"  }, {"TCON","genre"},
	};


	fseek( f, 0, SEEK_SET);
	hdr.size = 0;
	size_t n = fread( &hdr, 10, 1, f);
	if(n != 1) return -1;

	//printf("raw size: %llx\n", (LLU) hdr.size);
	hdr.size = ID3size( hdr.size );
	//printf("ID3 size: %llx\n", (LLU) hdr.size);

	//check for ID3 tag:
	if( memcmp( hdr.ID, "ID3", 3)  )
		return -1;

	if( hdr.version[0] > 5)
		return -2;
	if( hdr.version[0] < 3)	//uses a different frame structure.
		return -3;

	bool sync      = (hdr.flags & (1<<7)) != 0;
	bool hasExtHdr = (hdr.flags & (1<<6)) != 0;
	bool flagsOk   = (hdr.flags & 31    ) == 0;
	if( !flagsOk )
		return -3;

	if( hasExtHdr )
	{
		n=fread( &extHdr, 4, 1, f);
		if( extHdr.size > 10 )
			return -4;
		n=fread( &extHdr.flags, extHdr.size, 1, f);	//read the full extended header
	}

	//read all header-frames:
	char *frames = new char[hdr.size];
	size_t fpos  = 0;
	n = fread(frames, hdr.size, 1, f);
	if( n < 1)
		hdr.size = 0;	//couldn't read it.

	//reverse the protection against synchronization bytes:
	size_t unSyncLength = hdr.size;
	if( sync )
	{
		char *out = frames, *in  = frames;
		for(size_t i=0; i < hdr.size-1; i++)
		{
			*out++ = *in;
			if( (in[0] == '\xFF') && (in[1] == '\x00') )
				in++;
			in++;
		}
		*out = *in;	//copy last byte
		unSyncLength = (out+1 - frames);
	}


	//printf("hdr.size = %llu, unSyncLength: %llu\n", (LLU) hdr.size, (LLU) unSyncLength);
	while( fpos < unSyncLength-5 )	//make sure the whole frame->size is valid
	{
		std::string text;

		frame = (frame_s*)(frames+fpos);
		frame->size = ntohl( frame->size ); // or : ntohl ???
		if(frame->size == 0)	//this does happen sometimes, don't know how to proceed
			break;

		fpos += 10 + frame->size;

		/*{
			char IDstr[] = {0,0,0 ,0,0};
			memcpy( IDstr, frame->ID, 4);
			printf("fpos %i, ID %s, size %u\n", fpos, IDstr, frame->size);
		}*/

		if( frame->data[0] > 0x20)			//ISO-8859-1, 1 byte
			text.assign(frame->data  , frame->size  );
		else if( frame->data[0] == 0)		//ISO-8859-1, 1 byte, zero terminated
			text.assign(frame->data+1, frame->size-1);		//also 1-byte encoding
		else if( frame->data[0] == 1)		//UTF-16, unicode, zero-terminated
		{
			
			//std::wstring wtext = std::wstring((wchar_t*)(frame->data+3), (frame->size-3)/2); //unicode
			//text.resize( wtext.length() );
			if(frame->size > 3)
				text = utf16_to_char( frame->data+3, frame->size-3 );
			db_printf(15,"frame %s, text = '%s'\n", frame->ID, text.c_str() );
			//std::copy(wtext.begin(), wtext.end(), text.begin() );
		}
		else if( frame->data[0] == 2)		//UTF-16BE [UTF-16] encoded Unicode [UNICODE] without BOM.  Terminated with $00 00.
		{
			//std::wstring wtext = std::wstring((wchar_t*)(frame->data+1), (frame->size-1)/2); //unicode
			//text.resize( wtext.length() );
			//std::copy(wtext.begin(), wtext.end(), text.begin() );
			text = utf16_to_char( frame->data+1, frame->size-1 );
			db_printf(15,"frame %s, text = '%s'\n", frame->ID, text.c_str() );
		}
		else if( frame->data[0] == 3)		//UTF-8 [UTF-8] encoded Unicode [UNICODE]
			text.assign(frame->data+1, frame->size-1);


		for(size_t i=0; i< array_size(frameIDs); i++)
			if( memcmp( frame->ID, frameIDs[i][0], 4) == 0)
			{
				tags[frameIDs[i][1]] = text;
				break;
			}
	} //while (hdr.size)

	delete frames;
	return hdr.size + sizeof(hdr);	//start of the data
}

std::string dec2bin(uint32_t v)
{
	std::string out;
	out.resize(32);
	for(int i=0; i<32; i++)
	{
		bool s = ( (v & (1<<i)) != 0 );
		out[i] = (char)('0' + s) ;
	}
	return out;
}

std::string dec2bin(char v)
{
	std::string out="12345678";
	for(int i=0; i<8; i++)
	{
		bool s = ( (v & (1<<i)) != 0 );
		out[i] = (char)('0' + s) ;
	}
	return out;
}

int bitmask(int nrBits)
{
	return (1<<nrBits) - 1;
}

namespace flac
{
	enum blockType{
		btSteamInfo = 0,
		btPadding, btApplication, btSeektable, btVorbisComment,
		btCueSheet, btPicture
	};

	_align_pre_(1) struct metadataBlock {
		int isLast: 1;
		int blockType: 7;
		int length: 24;
	} _align_post_(1);

	_align_pre_(1) struct blkStreamInfo {
		metadataBlock hdr;
		uint16_t blkSizeMin;
		uint16_t blkSizeMax;
		uint32_t frmSizeMin;
		uint32_t frmSizeMax;
		uint32_t sampleRate;	//in Hz
		uint8_t nrChannels;		//number of channels 
		uint8_t bits;			//bits per sample 
		uint64_t nrSamples;		//stream length in samples
		char md5[16];
		//unsigned short md5: 128;		//md5 of uncompressed data

	} _align_post_(1);


	struct blkStreamInfo2 {
		//metadataBlock hdr;
		char hdr[4];
		char data[34];

		metadataBlock* getHdr(void)
		{
			return (metadataBlock*)hdr;
		}

		uint16_t getU16(const char *data) const
		{
			return ntohs( *( (uint16_t*)(data) ));
		}

		uint32_t getU32(const char *data) const
		{
			return ntohl( *( (uint32_t*)(data) ));
		}


		//header interpretation + endiannes conversion:
		blkStreamInfo parse(void)
		{
			blkStreamInfo r;
			r.hdr = *(getHdr());

			r.blkSizeMin = getU16(data+0);
			r.blkSizeMax = getU16(data+2);

			r.frmSizeMin = getU32(data+4) & bitmask(24);
			r.frmSizeMax = getU32(data+7) & bitmask(24);

			uint32_t tmp = getU32(data+10);
			int nrSamplesLo = (tmp    ) & bitmask( 4);
			r.bits			 = ((tmp>> 4) & bitmask( 5)) + 1;
			r.nrChannels	 = ((tmp>> 9) & bitmask( 3)) + 1;
			r.sampleRate     = (tmp>>12); // & bitmask(20);

			//std::string bits = dec2bin(tmp);
			//printf("tmp = %s\n", bits.c_str() );

			int nrSamplesHi = getU32(data+14);
			r.nrSamples = (nrSamplesHi<<4) + nrSamplesLo;

			return r;
		}

	};


	_align_pre_(1) struct stream {
		char id[4];
		blkStreamInfo2 streamInfo;
	} _align_post_(1);

	int parseHeader(FILE *f, fileInfo* info)
	{
		stream strm;

		//check if structs are compiled correctly:
		assert( sizeof(metadataBlock) == 4);

		//fseek(f, 0, SEEK_SET);
		fread( &strm, sizeof(strm), 1, f);

		//check header:
		if( memcmp( strm.id, "fLaC", 4) )
			return -1;
		if( strm.streamInfo.getHdr()->blockType != btSteamInfo)
			return -2;

		info->isAudioFile = true;

		blkStreamInfo blkHdr = strm.streamInfo.parse();
		info->nrBits = blkHdr.bits;
		info->nrChannels = blkHdr.nrChannels;
		info->sampleRate = blkHdr.sampleRate;

		return 0;
	}

}


///shamelessly copied from libtag:
int readMpegHeader(FILE *f, fileInfo& info)
{
	static const int bitrates[2][3][16] = {
		{ // Version 1
			{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0 }, // layer 1
			{ 0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384, 0 }, // layer 2
			{ 0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 0 }  // layer 3
		},
		{ // Version 2 or 2.5
			{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0 }, // layer 1
			{ 0, 8,  16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, 0 }, // layer 2
			{ 0, 8,  16, 24, 32, 40, 48, 56,  64,  80,  96,  112, 128, 144, 160, 0 }  // layer 3
		}
	};

	static const int sampleRates[3][4] = {
		{ 44100, 48000, 32000, 0 }, // Version 1
		{ 22050, 24000, 16000, 0 }, // Version 2
		{ 11025, 12000, 8000,  0 }  // Version 2.5
	};
	
	static const int samplesPerFrame[3][2] = {
		// MPEG1, 2/2.5
		{  384,   384 }, // Layer I
		{ 1152,  1152 }, // Layer II
		{ 1152,   576 }  // Layer III
	};

	enum mpegVersion_e {Version1=0,Version2, Version2_5};

	struct {
		enum mpegVersion_e version;
		char layer;
		bool protection;
		int  bitrate;
		bool isPadded;
		int  frameLength;
		int sampleRate;
		int samplesPerFrame;
	} hdr;
	memset(&hdr, 0, sizeof(hdr) );

	//seek possible start:
	bool hdrFound=false;
	while( !feof(f) )
	{
		uint8_t sync;
		fread(&sync, 1, 1, f);
		if( sync != 0xFF)
			continue;
		else 
		{
			uint8_t data[4];
			data[0] = 0xFF;
			fread(data+1, 3, 1, f);

			std::bitset<32> flags( ntohl( *((uint32_t*)data) ) );
			if(!flags[23] || !flags[22] || !flags[21]) 
				continue;

			//db_printf(1,"mpeg header at offset %llu\n", (LLU)(ftell(f)-4) );

			if(!flags[20] && !flags[19])		hdr.version = Version2_5;
			else if(flags[20] && !flags[19])	hdr.version = Version2;
			else if(flags[20] && flags[19])		hdr.version = Version1;

			if(!flags[18] && flags[17])			hdr.layer = 3;
			else if(flags[18] && !flags[17])	hdr.layer = 2;
			else if(flags[18] && flags[17])		hdr.layer = 1;

			hdr.protection = !flags[16];
			const int versionIndex = hdr.version == Version1 ? 0 : 1;
			const int layerIndex = hdr.layer > 0 ? hdr.layer - 1 : 0;

			uint8_t brate = (data[2]) >> 4;
			hdr.bitrate = bitrates[versionIndex][layerIndex][brate];

			uint8_t srate = (data[2]) >> 2 & 0x03;
			hdr.sampleRate = sampleRates[hdr.version][srate];
			if( hdr.sampleRate == 0)
				continue;

			//hdr.channelMode = ChannelMode((uchar(data[3]) & 0xC0) >> 6);
			hdr.isPadded = flags[9];

			if(hdr.layer == 1)
				hdr.frameLength = 24000 * 2 * hdr.bitrate / hdr.sampleRate + int(hdr.isPadded);
			else
				hdr.frameLength = 72000 * hdr.bitrate / hdr.sampleRate + int(hdr.isPadded);

			hdr.samplesPerFrame = samplesPerFrame[layerIndex][versionIndex];


			//very coarse music length, in seconds:
			//(proper way is to seek through all the frames, and count them)
			//(and use the Xing header for VBR mp3's)
			fseek(f, 0, SEEK_END);
			float flen = ftell(f);
			float length = flen / (float)(hdr.bitrate * 125.f);


			info.sampleRate = hdr.sampleRate;
			info.length     = (length+.5f);
			hdrFound = true;
			break;
		}
	}
	return hdrFound;
}

/// Get mime-type, and eventually also tags from the audio data
//std::auto_ptr<fileInfo>
void getFileInfo(const char *fname, fileInfo &info)
{
//	std::auto_ptr<fileInfo> info(new fileInfo);

	// get mime type, now only based on extension:
	const char *ext = strrchr(fname,'.');
	info.mime = getMime(ext);

	// if the extension doesn't indicate audio, don't even try to open it:
	if( info.mime.compare(0,5,"audio") != 0)
		return; // info;

	FILE *f = fopen(fname, "rb" );
	if(f == NULL)
		return; // info;

	//printf("scanning %s\n",fname);

	if( info.mime == "audio/mpeg" )
	{
		info.nrBits = '?';	//slim default
		info.nrChannels = 2;
		info.sampleRate = '?';

		//this matches winamp's behaviour if ID3v2 only contains
		//	replay-gain tags, but song info is in ID3v1:
		int r1 = tagID3v1(f, info.tags);	//try ID3v1
		int r2 = tagID3v2(f, info.tags);	//overwrite results with ID3v2, if exists.

		fseek(f, util::max(0, r2 -1 ), SEEK_SET);
		readMpegHeader(f, info);

		//try ID3v2 first, if that doesn't work, try ID3v1
		//if(r != 0)	r = tagID3v1(f, info->tags);
		//if(r == 0)	info->isAudioFile = true;
		if( (r1>=0) || (r2>=0) )
			info.isAudioFile = true;
	}
	else if( info.mime == "audio/flac" )
	{
		int r2 = tagID3v2(f, info.tags);
		if( r2 > 0 )
			fseek( f, r2, SEEK_SET);	//id3 found, seek to start of other data
		else
			fseek( f, 0 , SEEK_SET);	//nothing found, seek back
//		flac::parseHeader(f, &info );
	}

	fclose(f);
//	return info;
}


