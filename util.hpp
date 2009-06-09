
#ifndef _UTIL_HPP_
#define _UTIL_HPP_


#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <algorithm>

#include <sys/stat.h>



//platform dependend stuff:
#if defined(WIN32)
// && !defined(__CYGWIN__)
	//#define NOMINMAX
	#include <windows.h>
	#define strcasecmp stricmp
	#define strncasecmp strnicmp
	//msvc madness:
	#pragma warning (disable: 4996)
#endif

//used for 32/64-bit compatible printing of size_t:
typedef unsigned long long LLU;


/// Very basic shared_prt
template <class T> 
class shared_ptr
{
private:
	T	*data;
	int *refcount;

	void release(void)
	{
		if(refcount != NULL)
		{
			(*refcount)--;
			if( *refcount < 1)
			{
				delete data;
				delete refcount;
			}
		}
	}
public:
	shared_ptr(void)
	{
		refcount = NULL;
		data = NULL;
	}

	/// init from pointer, take ownership
	shared_ptr(T* ptr)
	{
		refcount = new int;
		*refcount = 1;
		data = ptr;
	}

	/// init from other shared pointer
	shared_ptr( shared_ptr<T>& src)
	{
		data	 = src.data;
		refcount = src.refcount;
		(*refcount)++;
	}

	~shared_ptr()
	{
		release();
	}

	shared_ptr<T>& operator=(shared_ptr<T>& src)
	{
		//prevent copy to self
		if (this == &src)
			return *this;
		//release own data:
		release();
		//acquire new:
		data     = src.data;
		refcount = src.refcount;
		(*refcount)++;

		return *this;
	}
};


/// Really low-level things
namespace util
{
	/// minimum of two values
	template <class T>
	T min(T a, T b)
	{
		return a<b ? a : b;
	}

	/// maximum of two values
	template <class T>
	T max(T a, T b)
	{
		return a>b ? a : b;
	}

	/// returns a value forced in the range [min,max]
	template <class T>
	T clip(T x, T min, T max)
	{
		x = x < min ? min: x;
		return x > max ? max: x;
	}

	/// helper function for sort()
	/*template <class T>
	static bool lessThan( T a, T b) 
	{
		return a < b;
	}

	/// sort a list
	template <class T>
	void sort( std::vector<T> &vec)
	{
		std::sort( vec.begin(), vec.end(), lessThan<T> );
	}*/
}


/// Returns the number of elements in a statically allocated array
template <typename T, unsigned int i> unsigned int array_size(T (&a)[i])
{
	return sizeof a / sizeof a[0];
}


/// python-like string functions
namespace pstring
{
	/// Strip both leading and trailing 'delim' characters
	std::string strip( std::string str, char delim=' ');

	/// Split string, given a delimiter.
	std::vector<std::string> split(const std::string& str, char delimiter = ' ');
}


/// routines to fix path names, similar to python's os.path modules.
namespace path {
	///note: a '/' is not escaped, even though it should
	const char urlEscapes[] = " #$%&:;<=>?@[\\]^`{|}~";

	/// hex to int
	int htoi(char c);

	/// put url/http escapes codes in to fname
	std::string escape(const std::string& fname);

	/// Replace url-escape codes by their actual ascii characters
	std::string unescape(const std::string& fname);

	std::string normalize(std::string fname);

	bool isdir(const std::string path);

	bool isfile(const std::string path);

	/// List the contenst of a directory. in python, this is os.listdir()
	std::vector<std::string> listdir(const std::string path, bool doSort=false);

	/// Combine two strings into a single pathname.
	std::string join(const std::string& p1, const std::string& p2);

	/// Split path and fileanme
	std::vector<std::string> split(const std::string& path);

} //namespace path


namespace nbuffer {

	/// Generic buffer, derived classes read from memory, file or network.
	class buffer {
	private:
	protected:
		size_t _size;
		size_t _pos;
	public:
		//allow derived classes to override the destructor, for cleanup
		virtual ~buffer()
		{ }

		/// Indicate end-of-stream
		char eof(void);
		/// Number of bytes
		size_t size(void);
		/// current position
		size_t pos(void);
		size_t seek(int offset) { _pos += offset; return _pos; }
		
		/// returns pointer to start of data, if possible, otherwise returns NULL
		virtual const char* ptr(void)	{return NULL; }
		
		/// read data into *dst, returns number of bytes read
		virtual int read(void *dst, size_t len)=0;
		
		/// close all handles
		virtual int close(void)=0;
	};


	/// Buffer from a file
	class bufferFile : public buffer
	{
	private:
		FILE *handle;
		std::string fname;	//for debug
	public:
		bufferFile(const char *fname);
		~bufferFile();
		int read(void *dst, size_t len);
		int close(void);
	};


	/// Buffer from a std::string
	class bufferString :  public buffer
	{
	private:
		std::string data;
	public:
		bufferString(std::string str);
		const char* ptr(void) { return data.c_str(); }
		int read(void *dst, size_t len);
		int close(void);
	};


	/// buffer from a memory location
	class bufferMem :  public buffer
	{
	private:
		char *data;
		bool ownsData;	//does this class new/delete *data?
	public:
		bufferMem(const void* data, size_t size, bool doCopy);
		~bufferMem();
		const char* ptr(void) { return data; }
		int read(void *dst, size_t len);
		int close(void);
	};

} //namespace nbuffer


#endif
