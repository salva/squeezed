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
/// TODO: unsynchronization is not yet supported
/// TODO: unicode strings are not yet supported.
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
	size_t n = fread( &hdr, 10, 1, f);
	//if(n != 1) return -1;

	hdr.size = ID3size( hdr.size );

	//check for ID3 tag:
	if( memcmp( hdr.ID, "ID3", 3)  )
		return -1;

	if( hdr.version[0] > 5)
		return -2;
	if( hdr.version[0] < 3)	//uses a different frame structure.
		return -3;

	//bool sync      = (hdr.flags & (1<<7)) != 0;
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

	while( fpos < hdr.size )
	{
		std::string text;

		frame = (frame_s*)(frames+fpos);
		frame->size = ntohl( frame->size ); // or : ntohl ???
		if(frame->size == 0)	//this does happen sometimes, don't know how to proceed
			break;

		fpos += 10 + frame->size;

		if( frame->data[0] > 0x20)			//ISO-8859-1, 1 byte
			text.assign(frame->data  , frame->size  );
		else if( frame->data[0] == 0)		//ISO-8859-1, 1 byte, zero terminated
			text.assign(frame->data+1, frame->size-1);		//also 1-byte encoding
#ifndef __CYGWIN__  //cygwin 1.5 doesn't have unicdoe support
		else if( frame->data[0] == 1)		//UTF-16, unicode, zero-terminated
		{
			//std::wstring wtext = std::wstring((wchar_t*)(frame->data+3), (frame->size-3)/2); //unicode
			//text.resize( wtext.length() );
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
#endif
		else if( frame->data[0] == 3)		//UTF-8 [UTF-8] encoded Unicode [UNICODE]
			text.assign(frame->data+1, frame->size-1);


		for(size_t i=0; i< array_size(frameIDs); i++)
			if( memcmp( frame->ID, frameIDs[i][0], 4) == 0)
			{
				tags[frameIDs[i][1]] = text;
				break;
			}
	} //while (ftell(f))
	return 0;
}



/// Get mime-type, and eventually also tags from the audio data
std::auto_ptr<fileInfo> getFileInfo(const char *fname)
{
	std::auto_ptr<fileInfo> info(new fileInfo);

	// get mime type, now only based on extension:
	const char *ext = strrchr(fname,'.');
	info->mime = getMime(ext);

	// if the extension doesn't indicate audio, don't even try to open it:
	if( info->mime.compare(0,5,"audio") != 0)
		return info;

	FILE *f = fopen(fname, "rb" );
	if(f == NULL)
		return info;

	if( info->mime == "audio/mpeg" )
	{
		info->nrBits = '?';	//slim default
		info->nrChannels = 2;
		info->sampleRate = '?';

		//this matches winamp's behaviour if ID3v2 only contains
		//	replay-gain tags, but song info is in ID3v1:
		int r1 = tagID3v1(f, info->tags);	//try ID3v1
		int r2 = tagID3v2(f, info->tags);	//overwrite results with ID3v2, if exists.

		//try ID3v2 first, if that doesn't work, try ID3v1
		//if(r != 0)	r = tagID3v1(f, info->tags);
		//if(r == 0)	info->isAudioFile = true;
		if( (r1>=0) || (r2>=0) )
			info->isAudioFile = true;
	}

	fclose(f);
	return info;
}


