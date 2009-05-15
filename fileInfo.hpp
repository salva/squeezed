/** 
 * @file 
 * Functions to provide file information.
 * Currently the mime-type and audio-tag data
 *
 */

#ifndef _FILEINFO_HPP_
#define _FILEINFO_HPP_

#include <string>
#include <map>
#include <memory>


struct fileInfo
{
	std::string url;
	std::string mime;	// mime-type
	bool isAudioFile;		// Whether it is a valid file

	//audio info (needed by slim, for non-mp3 files):
	int nrBits;
	int nrChannels;
	int sampleRate;		///< Samples per second
	int length;			///< Music length, in seconds

	std::map< std::string, std::string> tags;	//possible tags

	fileInfo(): isAudioFile(0),
				nrBits(0), nrChannels(0), sampleRate(0),
				length(0)
	{ }
};



//Get mime type, based on the extension of a filename
std::string getMime(const char *extension);

//std::auto_ptr<fileInfo> 
void getFileInfo(const char *fname, fileInfo& info);

#endif
