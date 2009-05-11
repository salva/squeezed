/* Some small functions, which make life a bit easier
 *
 */


#include <string.h>
#include <stdint.h>

#if defined(WIN32) && !defined(__CYGWIN__)
	#define S_ISDIR(a) (((a) & _S_IFDIR)!=0)
	#define S_ISREG(a) (((a) & _S_IFREG)!=0)
#else
	#include <stdlib.h>     //for realpath
#endif


#include "debug.h"
#include "util.hpp"
#include <dirent.h>	//for path::listdir()


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
}


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
            return std::string("");
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

		//strip trailing backslash:
		if( tmp[reqLen-2] == os::sep )
			tmp[reqLen-2] = 0;
		//convert to lower case
		//for(size_t i=0; i< reqLen-1; i++)	tmp[i] = tolower(tmp);

		std::string out( tmp );
		delete tmp;
#else
        //use a glibc-specific implementation, which does not have the memory allocation
        //	problems of the standard version
        char *tmp = realpath( fname.c_str() , 0 );
        //note that *tmp is malloc'd by realpath.
        if(tmp == NULL)
            return std::string();
		//strip trailing backslash:
		int reqLen = strlen(tmp);
		if( tmp[reqLen-2] == os::sep )
			tmp[reqLen-2] = 0;

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
			out =  p2;
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
		std::vector<std::string> files;	//file/directory names for each depth
		struct dirent * de;

		DIR *dir =  opendir( path.c_str() );
		if (!dir)
			return files;

		while( (de=readdir(dir)) )
		{
			if( de->d_name[0] != '.' )	//dont do ".", ".." and any hidden files
				files.push_back( de->d_name );
		}
		closedir( dir );

		if(doSort)
		{
			std::sort( files.begin(), files.end(), fnameLessThan );	// Sort case insensitive
		}

		return files;
	}


} //namespace path


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
			n = fread(dst, nrCopy, 1, handle);
			if( n != 1) {
				db_printf(1,"bufferFile(): error reading %s: %llu\n", fname.c_str(), (LLU)n );
			} else
				_pos += nrCopy;
		}

		return n;
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
