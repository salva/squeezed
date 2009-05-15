/**
 * \defgroup musicDB music database
 *
 * Functions to scan and search a music collection
 *
 * fileInfo determines which files types are supported
 *
 *@{
 */



#ifndef musicDB_H_
#define musicDB_H_



#include <vector>
#include <list>
#include <string>
#include <stdint.h>
#include <limits.h>
#include <string.h> //for strlen()

#include "debug.h"
#include "fileInfo.hpp"


/// enum of fields which can be indexed
enum dbField {
	DB_INVALID=0,
	DB_ANY,
	DB_ALL,
	DB_TITLE,
	DB_ALBUM,
	DB_ARTIST,
	DB_YEAR,
	DB_GENRE,
};

/// name for all of the fields in dbField
extern const char *dbFieldStr[];


///dbEntry only contains information which can be searched,
/// file-format is not stored.
struct dbEntry {
	std::string relPath;		//path relative to base
	std::string fileName;

	//basic song info, on which can be searched:
	std::string title;
	std::string album;
	std::string artist;
	std::string year;
	std::string genre;

	std::string getField(dbField field) const
	{
		std::string ret;
		switch(field)
		{
		case DB_TITLE:	ret = title; break;
		case DB_ALBUM:	ret = album; break;
		case DB_ARTIST:	ret = artist; break;
		case DB_YEAR:	ret = year;  break;
		case DB_GENRE:	ret = genre; break;
		default:		break;
		}
		return ret;
	}

	dbEntry() {}		// also allow an empty constructor
	dbEntry(std::string relPath, std::string fname, fileInfo *fInfo);
	dbEntry(FILE *f);	// load from a file handle;

	int pickle(FILE *f);	//store to file
};





/// Music database. simple version just browses paths..
class musicDB {
	 friend class dbQuery;
private:
	enum IDX_ID { IDX_TITLE, IDX_ALBUM, IDX_ARTIST, IDX_GENRE, IDX_END };
	typedef std::vector<uint32_t> vec32_t;

	std::string				basePath;		///< absolute path, to base of music-storage
	size_t					nrEntries;		///< size of the Database (same as offset.size() )
	FILE					*f_db;			///< file handle to the database file.
	std::vector<uint32_t>	offset;			///< offset into *.db file, one per entries
	std::vector<uint32_t>	idx[IDX_END];	///< per string in dbEntry, a array of sorted indices into *offset.

	/// Comparison functions for sorting and searching:
	class compare
	{
	protected:
		const static char emptyStr;
		FILE		*f_db;
		vec32_t		*offset;
		const char*	invalidStr;	//string to use for invalid indices
		size_t		maxStrLen;
		const dbEntry operator[](const int idx)
		{
			fseek(f_db, (*offset)[idx], SEEK_SET);
			return dbEntry(f_db);
		}
	public:
		compare(FILE *f_db, vec32_t *offset, const char* s): f_db(f_db), offset(offset), invalidStr(s)
		{
			if( invalidStr == NULL)
			{
				invalidStr = &emptyStr;
				maxStrLen = INT_MAX;	//max. length of a string for strcasecmp
			} else
				maxStrLen = strlen(s);
		}
		virtual bool operator() (uint32_t idxA, uint32_t idxB) = 0;
	};

	class compareTitle : public compare
	{
	public:
		compareTitle(FILE *f_db, vec32_t *offset, const char* s=NULL): compare(f_db,offset,s) {}
		bool operator() (uint32_t idxA, uint32_t idxB);
	};

	struct compareArtist : public compare
	{
		compareArtist(FILE *f_db, vec32_t *offset, const char* s=NULL): compare(f_db,offset,s) {}
		bool operator() (uint32_t idxA, uint32_t idxB);
	};

	struct compareAlbum : public compare
	{
		compareAlbum(FILE *f_db, vec32_t *offset, const char* s=NULL): compare(f_db,offset,s) {}
		bool operator() (uint32_t idxA, uint32_t idxB);
	};

	struct compareGenre : public compare
	{
		compareGenre(FILE *f_db, vec32_t *offset, const char* s=NULL): compare(f_db,offset,s) {}
		bool operator() (uint32_t idxA, uint32_t idxB);
	};

public:
	musicDB(const char *path):
		basePath(path),
		nrEntries(0),
		f_db(NULL)
    {
		//offset(NULL)
		//for(size_t i=0; i<array_size(idx); i++)	idx[i] = NULL;
        db_printf(2,"music DB at: %s\n", basePath.c_str() );
    }

	~musicDB()
	{
		if( f_db != NULL)
			fclose(f_db);
		//for(size_t i=0; i<array_size(idx); i++)	delete idx[i];
		//delete offset;
	}

	/// Build the list of files (*f_db and *offset)
	void scan(const char *dbName = "SqueezeD.db");

	/// Create sorted indices for the datasets, store the to disk
	void index(const char *idxName);

	/// Load an existing database from file
	int load(const char *dbName, const char *idxName);

	size_t size(void) {return nrEntries; }

	std::string getBasePath(void) {return basePath; }

	/// databas acces. TODO: mutex this with
	/// scan() and index()
	const dbEntry operator[](const uint32_t idx)
	{
		dbEntry ret;
		if( idx < offset.size() ) {
			fseek(f_db, offset[idx], SEEK_SET);
			ret = dbEntry(f_db);
		}
		return ret;
	}
};





/// Class to perform searches within the database
class dbQuery {
public:

protected:
	musicDB *db;
	const char *match;

	/// Search results:
	std::vector<uint32_t>::iterator itBegin, itEnd;

	size_t ibegin, iend, ilen;	//indices
	dbField			field;		//field on which the search was performed

	//TODO: make proper use of iterators, so a search can be refined by
	// adding a charachter to *match, and starting a new query on that.

	//indices into the musicDB::offset table of the results of this search.
	// this makes it possible to return an iterator of the results..
	// copy is also required to re-sort if the previous query was on another field.
	const std::vector<uint32_t> *idx;
	bool ownIdx;		///< false if *idx was copied from somewhere else (a previous query)

	struct uIdx {
		uint32_t idx;
		size_t count;
		uIdx(uint32_t idx, size_t count): idx(idx), count(count) { }
	};
	std::vector<uIdx> uniqueIdx;	//unique indices, with number of objects per index. for album browsing.

	/// Update itBegin and itEnd, such that only items which match the query are retained.
	void find();

public:
	/// Start a new query
	dbQuery(musicDB *db, dbField field, const char *match);

	/// Query on results of another Query
	dbQuery(musicDB *db, dbQuery &base, dbField field, const char *match);


	/// Acces to the search results
	/// With db[idx[i]] the song data can be retrieved.
	std::vector<uint32_t>::iterator begin() { return itBegin; }
	std::vector<uint32_t>::iterator end()  { return itEnd; }

	size_t size(void) { return ilen; }

	/// Field on which the search was performed
	dbField	getField(void) { return field; }

	/// index, sorted on current field
	const musicDB::vec32_t* getIdx(void) { return idx; }


	///	number of unique results:
	size_t uSize(void)
	{
		return uniqueIdx.size();			
	}


	/// db index into, for a unique search result
	uint32_t uIndex(size_t index)
	{	
		uint32_t idx = 0;
		if( index < uniqueIdx.size() )
			idx = uniqueIdx[index].idx;
		return idx;		
	}

	/// Number of items within this unique result
	size_t uCount(size_t index)
	{	
		size_t count = 0;
		if( index < uniqueIdx.size() )
			count = uniqueIdx[index].count;
		return count;	
	}


	const dbEntry operator[](const size_t resultIdx)
	{
		dbEntry ret;
		if( resultIdx < size() )
		{
			uint32_t dbIdx = *(itBegin + resultIdx);
			ret = (*db)[ dbIdx ];
		}
		return ret;
	}
};



/**@}
 *end of doxygen group
 */

#endif
