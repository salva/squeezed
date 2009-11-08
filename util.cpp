/* Some small functions, which make life a bit easier
 *
 */


#include <string.h>
#include <stdint.h>
//#include <ctype.h> //for tolower()
#include <cctype>

#if defined(WIN32) && !defined(__CYGWIN__)
	#define S_ISDIR(a) (((a) & _S_IFDIR)!=0)
	#define S_ISREG(a) (((a) & _S_IFREG)!=0)
#else
	#include <stdlib.h>     //for realpath
#endif


#include "debug.h"
#include "util.hpp"
#include <dirent.h>	//for path::listdir()


namespace util
{
	const fletcher_state_t fletcher_init = {0xFF, 0xFF, 0};


	/// Update Fletcher state with new data:
	/// TODO: properly take into account that if len%21 != 0, multiple calls
	/// to fletcher_update do not give the same result as a single call.
	size_t fletcher_update(const uint8_t *data, size_t len, fletcher_state_t *state)
	{
		uint16_t sum1 = state->sum1;
		uint16_t sum2 = state->sum2;
		uint8_t modlen=0;
		while (len) {
                size_t tlen = len > 21 ? 21 : len;
                modlen = 21-len;	//remaining length to store
                len -= tlen;
                do {
                        sum1 += *data++;
                        sum2 += sum1;
                } while (--tlen);
                sum1 = (sum1 & 0xff) + (sum1 >> 8);
                sum2 = (sum2 & 0xff) + (sum2 >> 8);
        }
        state->sum1 = sum1;
        state->sum2 = sum2;
        state->modlen = modlen;
        return len;
	}



	/// update fletcher state with a single byte.
	/// (hopefully this function is inlined)
	void fletcher_update_single(uint8_t data, fletcher_state_t& state)
	{
		state.sum1 += data;
		state.sum2 += state.sum2;
		state.modlen++;
		if( state.modlen > 21)
		{
			state.modlen = 0;
			state.sum1 = (state.sum1 & 0xff) + (state.sum1 >> 8);
			state.sum2 = (state.sum2 & 0xff) + (state.sum2 >> 8);
		}
	}



	/// Generate a hash code from the current state
	uint16_t fletcher_finish(fletcher_state_t state)
	{
		uint16_t checksum;
		uint16_t sum1 = state.sum1;
		uint16_t sum2 = state.sum2;
		sum1 = (sum1 & 0xff) + (sum1 >> 8);
        sum2 = (sum2 & 0xff) + (sum2 >> 8);
        checksum  = (sum1 & 0xFF) << 8;
        checksum |= (sum2 & 0xFF);
        return checksum;
	}

} //namespace util


namespace pstring
{
	// Strip both leading and trailing 'delim' characters
	std::string strip( std::string str, char delim)
	{
		size_t begin,end;
		for(begin = 0; begin < str.size(); begin++)
			if( str[begin] != delim)
				break;
		for(end = str.size()-1; end >= 0; end--)
			if( str[end] != delim)
				break;
		return str.substr( begin, end-begin+1 );
	}


	// Split string, given a delimiter
	std::vector<std::string> split(const std::string& str, char delimiter)
	{
		std::vector<std::string> ret;
		size_t start=0;
		for(size_t i=0; i < str.length(); i++)
		{
			if( str[i] == delimiter)
			{
				ret.push_back( str.substr(start, i-start ) );
				start = i+1;
			}
		}
		ret.push_back( str.substr(start) );
		return ret;
	}


	/// Convert a string to lower case
	void tolower(std::string& str)
	{
		for(size_t i=0; i < str.size(); i++)
			str[i] = std::tolower( (char)str[i] );
	}

    bool startswith(std::string& str, std::string& pattern)
    {
        //bool ret = false;
        if( str.size() < pattern.size() )
            return false;
        return memcmp( str.data(), pattern.data(), pattern.size() ) == 0;
    }

} //namespace string


namespace os
{
#ifdef WIN32
	char sep = '\\';
#else
	char sep = '/';
#endif
}


namespace path
{
		//hex to int:
	int htoi(char c)
	{
		int x = -1;
		c = tolower(c);
		if( (c>='0') && (c<='9'))
			x= c - '0';
		if( (c>='a') && (c<='f'))
			x = c-'a'+10;
		return x;
	}

	//put url/http escapes codes in to fname
	std::string escape(const std::string& fname)
	{
		std::ostringstream out;
		int n = fname.size();
		for(int i=0; i < n; i++)
		{
			bool isEscape = false;
			for(size_t j=0; j<sizeof(urlEscapes); j++)
				if( fname[i] == urlEscapes[j] )
				{
					isEscape = true;
					break;
				}
			if(isEscape)
				out << "%" << std::hex << ((int)fname[i]);
			else
				out << fname[i];
		}
		return out.str();
	}


	// Replace url-escape codes by their actual ascii characters
	std::string unescape(const std::string& fname)
	{
		std::string out;
		int n = fname.size();
		for(int i=0; i < n; i++)
		{
			if( (fname[i] == '%') && (i < n-2)  )
			{
				int c = htoi(fname[i+1])*16 + htoi(fname[i+2]);
				i+=2;
				out.push_back(c);
			} else
				out.push_back(fname[i]);
		}
		return out;
	}


	std::string normalize(std::string fname)
	{
		db_printf(30,"normalize()\n");

		//std::string out;
		/*size_t n = fname.size();
		for(size_t i=0; i< fname.size()-1; i++)
		{
			if( fname[i] == '/' )
				out.push_back( '\\');
			else
				out.push_back( tolower(fname[i]) );
		}*/
		/*strip trailing backslash:
		if( out[out.size()-1] == '\\' )
			out.resize( out.size()-1 );*/

		//normalize pathname, to prevent tricks like "..\..\.."
		// (also converts '/' to '\', removes trailing backslashes

        if( fname.size() < 1 )
            return "";
#if !defined(WIN32)
        // Parse home-dir, since opendir() doesn't seem to know about that.
        if( fname[0] == '~')
        {
            std::string home = getenv("HOME");
            std::string fnameRel = fname.substr(1);
            fname = path::join(home, fnameRel  );
            db_printf(2,"modified home dir to: %s\n", fname.c_str() );
        }
#endif

#if defined(WIN32) //&& !defined(__CYGWIN__)
		int reqLen = GetFullPathNameA( fname.c_str(), 0, NULL, NULL);	//request required size
		char *tmp = new char[ reqLen ];

		GetFullPathNameA( fname.c_str(), reqLen, tmp, NULL);			//do the work

		//strip trailing backslash, unless it's at the root of a drive letter:
		if( tmp[reqLen-2] == os::sep )
			tmp[reqLen-2] = 0;
		if( reqLen > 3 )
			if( tmp[reqLen-3] == ':')
				tmp[reqLen-2] = os::sep;
		//convert to lower case
		//for(size_t i=0; i< reqLen-1; i++)	tmp[i] = tolower(tmp);

		std::string out( tmp );
		delete tmp;
#elif defined(__UCLIBC__)
		//for openwrt: don't use glibc specific extension:
		//db_printf(1,"SKIPPING realpath()\n");
		char tmp[PATH_MAX];
		char *res = realpath( fname.c_str() , tmp );
		if( res == NULL)
			return "";
		std::string out(tmp);
#else
        //use a glibc-specific implementation, which does not have the memory allocation
        //	problems of the standard version
		db_printf(30,"calling realpath() on '%s'\n", fname.c_str());
        char *tmp = realpath( fname.c_str() , NULL );
        //note that *tmp is malloc'd by realpath.
        if(tmp == NULL)
            return "";

		//strip trailing backslash:
		int reqLen = strlen(tmp);
		if( (tmp[reqLen-2] == os::sep) && (reqLen > 2) )
			tmp[reqLen-2] = 0;

		db_printf(30,"normalize(): converting back to string\n");
		std::string out( tmp );
		free(tmp);	//it's malloced, don't use delete
#endif
		return out;
	}


	bool isdir(const std::string path)
	{
		bool ret = false;
#if defined(WIN32) && !defined(__CYGWIN__)
		//struct _stat64 fstat;
		struct stat fstat;
		int e = stat( path.c_str(), &fstat);
#else
		struct stat fstat;
		int e = stat( path.c_str(), &fstat);
#endif
		if( (e==0) && S_ISDIR(fstat.st_mode) )
			ret = true;
		return ret;
	}

	bool isfile(const std::string path)
	{
		bool ret = false;
#if defined(WIN32) && !defined(__CYGWIN__)
		//struct _stat64 fstat;
		struct stat fstat;
		int e = stat( path.c_str(), &fstat);
#else
		struct stat fstat;
		int e = stat( path.c_str(), &fstat);
#endif
		if( (e==0) && S_ISREG(fstat.st_mode) )
			ret = true;
		return ret;
	}


	//join two parts of a path, make sure there's 1 separator in the middle;
	std::string join(const std::string& p1, const std::string& p2)
	{
		std::string out;
		if( p1.length() == 0)
			out = p2;
		else if( p2.length() == 0)
			out = p1;
		else
		{
			out  = p1;
			size_t ie = out.size()-1;
			if( (out[ie] == '/') || (out[ie] == '\\') )
				out[ie] = os::sep;
			else // if (out[ie] != os::sep )
				out.push_back(os::sep);
			//now 'out' ends with a correct path-separator, append p2 to it:
			if( (p2[0] == '/') || (p2[0] == '\\') )
				out.append( p2.substr(1) );
			else
				out.append(p2);
		}
		db_printf(55,"path::join(): '%s' + '%s' = '%s'\n", p1.c_str(), p2.c_str(), out.c_str() );
		return out;
	}


	/// sorting criterium for filenames. TODO: directories first
	static bool fnameLessThan( std::string a,  std::string b)
	{
		std::transform(a.begin(), a.end(), a.begin(), toupper);
		std::transform(b.begin(), b.end(), b.begin(), toupper);
		return a < b;
	}

	std::vector<std::string> listdir(std::string path, bool doSort)
	{
		std::vector<std::string> files;	//file names for each depth
		std::vector<std::string> dirs;  //keep directories separate, so they are on top in the result
		struct dirent * de;

		DIR *dir =  opendir( path.c_str() );
		if (!dir)
			return files;

		while( (de=readdir(dir)) )
		{
			if( de->d_name[0] != '.' )	//dont do ".", ".." and any hidden files
			{
				if( (de->d_type & DT_DIR)!=0 )
					dirs.push_back( de->d_name );
				else
					files.push_back( de->d_name );
			}
		}
		closedir( dir );

		if(doSort)
		{
			std::sort( files.begin(), files.end(), fnameLessThan );	// Sort case insensitive
			std::sort( dirs.begin(), dirs.end(), fnameLessThan );
		}
		dirs.insert( dirs.end(), files.begin(), files.end() );

		return dirs;
	}


	// split path and filename
	std::vector<std::string> split(const std::string& path)
	{
		std::vector<std::string> out;
		int pos = path.rfind( os::sep );
		out.push_back( path.substr(0, pos  ) );
		out.push_back( path.substr(pos+1) );
		return out;
	}

} //namespace path



namespace file
{
	char* readfile(const char *fname,  size_t *size, const char *mode)
	{
		char *out=NULL;
		FILE *handle = fopen( fname, mode);
		if(handle==NULL)
			return out;

		size_t n;

		n = fseek( handle, 0, SEEK_END);
		*size = ftell(handle);
		n = fseek( handle, 0, SEEK_SET);

		out = (char*)malloc( *size + 1 );	//zero-terminate it.
		n=0;
		if( out != NULL )
			n = fread( out, 1, *size, handle);	//swap 1 and *size to be faster, but that doesn't work in ascii mode
		*size = n;
		out[*size] = 0;	//zero-terminate it. malloc() has reserved space for this

		fclose( handle );
		return out;
	}

} //namespace file


namespace nbuffer
{

	/// Generic buffer, derived classes read from memory, file or network.
	char buffer::eof(void)
	{
		return _pos >= _size;
	}

	size_t buffer::size(void)
	{
		return _size;
	}

	size_t buffer::pos(void)
	{
		return _pos;
	}


	bufferFile::bufferFile(const char *fname)
	{
		this->fname = std::string(fname);
		handle = fopen(fname,"rb");
		if(handle != NULL)
		{
			fseek(handle, 0, SEEK_END);
			_size = ftell(handle);
			fseek(handle, 0, SEEK_SET);
		} else
			_size = 0;
		_pos = 0;
	}

	bufferFile::~bufferFile()
	{
		fclose(handle);
	}

	int bufferFile::read(void *dst, size_t len)
	{
		int32_t nrCopy = util::min<int32_t>(len, _size - _pos );
		size_t n=0;
		//if( (nrCopy < 0) || (nrCopy > (1<<16)) )	db_printf(1,"bufferFile(): nrCopy = %i, size = %lu, pos = %lu\n", nrCopy, _size, _pos);

		if( handle != NULL)
		{
			//TODO: verify _pos and ftell(handle), since seek() is not inherited correctly
			n = fread(dst, nrCopy, 1, handle);
			if( n != 1) {
				db_printf(1,"bufferFile(): error reading %s: %llu\n", fname.c_str(), (LLU)n );
			} else
				_pos += nrCopy;
		}

		return n;
	}


	char bufferFile::eof(void)
	{
		if (handle==NULL)
			return true;
		else
			return (feof(handle)!=0)||(_pos>=_size);
	}

	int bufferFile::close(void)
	{
		_size = 0;
		return fclose(handle);
	}



	bufferString::bufferString(std::string str)
	{
		data = str; //copy the string
		_pos = 0;
		_size = str.size();	//not zero-terminated
	}

	int bufferString::read(void *dst, size_t len)
	{
		size_t nrCopy = util::min<int32_t>(len, _size - _pos );
		memcpy(dst, data.c_str() + _pos, nrCopy);
		_pos += nrCopy;
		return nrCopy;
	}

	int bufferString::close(void)
	{
		data = "";
		_size = 0;
		_pos  = 0;
		return 0;
	}


	/// Buffer from a memory location.
	bufferMem::bufferMem(const void* data,
						size_t size,	//< size (in bytes) of *data
						bool doCopy		//< determines if a local copy of *data should be made
						)
	{
		if(doCopy) {
			this->data = new char[size];
			memcpy(this->data, data, size);
		} else
			this->data = (char*)data;
		this->ownsData = doCopy;
		_size = size;
		_pos = 0;
	}

	bufferMem::~bufferMem()
	{
		close();
	}

	int bufferMem::read(void *dst, size_t len)
	{
		size_t nrCopy = util::min<int32_t>(len, _size - _pos);
		memcpy(dst, data+_pos, nrCopy);
		_pos += nrCopy;
		return nrCopy;
	}

	int bufferMem::close(void)
	{
		if( ownsData )
			delete data;
		ownsData = false;
		data = NULL;
		_size = 0;
		_pos  = 0;
		return 0;
	}


} //namespace buffer
