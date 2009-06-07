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
private:
	const static std::string strEmpty;
public:


	std::string url;
	std::string mime;	// mime-type
	bool isAudioFile;	// Whether it is a valid file

	//audio info (needed by slim, for non-mp3 files):
	int nrBits;
	int nrChannels;
	int sampleRate;		///< Samples per second
	int length;			///< Music length, in seconds

	float gainTrack, gainAlbum;	///< replay gains

	std::map< std::string, std::string> tags;	//possible tags

	/// Zero constructor
	fileInfo(): isAudioFile(0),
				nrBits(0), nrChannels(0), sampleRate(0),
				length(0),
				gainTrack(0), gainAlbum(0)
	{ }

	/// Initialize from a file:
	fileInfo(const char *fname);

	/// Wrapper to fix std::map's unexpected behaviour for non-existing keys:
	/*const std::string& getTag(std::string& key) const
	{
		std::map< std::string, std::string>::const_iterator v = tags.find(key);
		if( v == tags.end() )
			return strEmpty;
		else
			return v->second;
	}*/
};


//Get mime type, based on the extension of a filename
std::string getMime(const char *extension);


#endif
