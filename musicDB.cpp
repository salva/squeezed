

#include <list>
#include <vector>
#include <dirent.h>
#include <stdint.h>
#include <algorithm>

#include "util.hpp"
#include "fileInfo.hpp"

#include "musicDB.hpp"


using namespace std;



//--------------------------- Local types --------------------------------------------------


/// Read a zero-terminated string from a file
void readCstring(char *dest,	///< O: destination buffer
				 size_t len,	///< I: max. length to read
				 FILE *file		///< I: file to read from
				 )
{
	char c;
	size_t i = 0;
	if( feof(file) || (len==0) )
		return;
	do
	{
		c = fgetc(file);
		*dest++ = c;
		i++;
	}
	while( (c != 0) && !feof(file) && (i<len) );
}


/// Write std::string to a file as zero-terminated string, return length written
size_t writeCstring(const std::string& str, FILE *file)
{
	size_t len = strlen( str.c_str() );	//stop writing after first zero.
	size_t n = fwrite( str.c_str(),  len+1,  1, file);	//write data + terminating zero;
	return n*len;
}


dbEntry::dbEntry(std::string relPath, ///< path, relative to basepath of the databse
				 std::string fname,	///< filename only, without any paths
				 fileInfo *fInfo)
{
	this->relPath  = relPath;
	this->fileName = fname;		//filename only, without any paths

	/*title = fInfo->getTag(string("title"));
	artist= fInfo->getTag(string("artist"));
	album = fInfo->getTag(string("album"));
	year  = fInfo->getTag(string("year"));
	genre = fInfo->getTag(string("genre"));*/
	title = fInfo->tags["title"];
	artist= fInfo->tags["artist"];
	album = fInfo->tags["album"];
	year  = fInfo->tags["year"];
	genre = fInfo->tags["genre"];
}


/// initialize an entry from a file database
dbEntry::dbEntry(FILE *f)
{
	uint16_t len;

	size_t n = fread( &len, sizeof(uint16_t), 1, f);
	if( n == 1)
	{
		char *tmp = new char[len];
		readCstring( tmp, len, f);	relPath = tmp;
		readCstring( tmp, len, f);	fileName = tmp;
		readCstring( tmp, len, f);	title = tmp;
		readCstring( tmp, len, f);	artist = tmp;
		readCstring( tmp, len, f);	album = tmp;
		readCstring( tmp, len, f);	year = tmp;
		readCstring( tmp, len, f);	genre = tmp;
		delete tmp;
	}
}


// store entry to a file database
int dbEntry::pickle(FILE *f)
{
	/*uint16_t len = (uint16_t)(relPath.length() + fileName.length() +
					title.length() + album.length() + artist.length() +
					year.length() + genre.length());
	len += 7;	//include zero-terminations*/

	// make sure the strings contain no zeros themselves:
	uint16_t lens[] = 
		{	strlen(relPath.c_str())+1,
			strlen(fileName.c_str())+1,
			strlen(title.c_str())+1,
			strlen(artist.c_str())+1,
			strlen(album.c_str())+1,
			strlen(year.c_str())+1,
			strlen(genre.c_str())+1,
		};

	uint16_t len = lens[0] + lens[1] + lens[2] + lens[3] + lens[4] + lens[5] + lens[6];

	fwrite( &len, sizeof(uint16_t), 1, f);
	fwrite( relPath.c_str(),  lens[0], 1, f);
	fwrite( fileName.c_str(), lens[1], 1, f);
	fwrite( title.c_str(),    lens[2], 1, f);
	fwrite( artist.c_str(),   lens[3], 1, f);
	fwrite( album.c_str(),    lens[4], 1, f);
	fwrite( year.c_str(),     lens[5], 1, f);
	fwrite( genre.c_str(),    lens[6], 1, f);

	return 2 + len;	// length + data
}




//--------------------------- External functions -------------------------------------------




/// list directory recursively, files only.
void listdirRecursive( const std::string& basePath,
					   const std::string& relPath,
					   std::list<dbEntry>& lstFound)
{
	//std::auto_ptr<fileInfo> fInfo;

	std::string sStartDir = path::join(basePath, relPath);
	DIR* pDir = opendir ( sStartDir.c_str ());
	if (!pDir)
		return;

	db_printf(8,"listdirRecursive(): %s\n",sStartDir.c_str () );

	dirent* pEntry;
	while ( (pEntry = readdir(pDir)) )
	{
		string fullEntry = path::join(sStartDir, pEntry->d_name );
#ifdef __CYGWIN__
		bool isDir = path::isdir( fullEntry );		// Cygwin doesn't have d_type
#else
		bool isDir = (pEntry->d_type & DT_DIR)!=0;
#endif
		if ( (isDir) && strcmp( pEntry->d_name, "..") && strcmp( pEntry->d_name, ".") )
		{
			std::string newRelPath = path::join(relPath, pEntry->d_name);
			listdirRecursive( basePath, newRelPath, lstFound);
		}
		if( !(isDir) )
		{
			fileInfo fInfo( fullEntry.c_str() );

			if( fInfo.isAudioFile) {
				dbEntry entry(relPath, string(pEntry->d_name), &fInfo );
				lstFound.push_back( entry );
				//db_printf(50,"%-30s: %s - %s, year %s\n", fInfo.url.c_str(), fInfo.tags["artist"].c_str(), fInfo.tags["title"].c_str(), fInfo.tags["year"].c_str() );
			}
			else
			{
				db_printf(5,"%-30s: invalid\n", pEntry->d_name);
			}
		}
	}
	closedir( pDir );
}


//--------------------------- Comparison functions for sorting and searching -------------


const char musicDB::compare::emptyStr = 0;

bool musicDB::compareTitle::operator() (uint32_t entryNrA, uint32_t entryNrB)
{
	// Load data:
	string sA, sB;
	const char *cA = invalidStr, *cB=invalidStr;
	if( entryNrA < offset->size() ) sA = (*this)[entryNrA].title,	cA=sA.c_str();	//need to keep sA, else cA becomes invalid
	if( entryNrB < offset->size() ) sB = (*this)[entryNrB].title,	cB=sB.c_str();
	//size_t len = min(strlen(cA), maxStrLen);
	return strncasecmp(cA , cB, maxStrLen )<0;		// Compare (only up to strLen(invalidStr) characters)
}


bool musicDB::compareAlbum::operator() (uint32_t entryNrA, uint32_t entryNrB)
{
	// Load data:
	string sA, sB;
	const char *cA = invalidStr, *cB=invalidStr;
	if( entryNrA < offset->size() ) sA = (*this)[entryNrA].album,	cA=sA.c_str();	//need to keep sA, else cA becomes invalid
	if( entryNrB < offset->size() ) sB = (*this)[entryNrB].album,	cB=sB.c_str();
	return strncasecmp(cA , cB, maxStrLen )<0;		// Compare (only up to strLen(invalidStr) characters)
}


bool musicDB::compareArtist::operator() (uint32_t entryNrA, uint32_t entryNrB)
{
	// Load data:
	string sA, sB;
	const char *cA = invalidStr, *cB=invalidStr;
	if( entryNrA < offset->size() ) sA = (*this)[entryNrA].artist,	cA=sA.c_str();	//need to keep sA, else cA becomes invalid
	if( entryNrB < offset->size() ) sB = (*this)[entryNrB].artist,	cB=sB.c_str();
	return strncasecmp(cA , cB, maxStrLen )<0;		// Compare (only up to strLen(invalidStr) characters)
}


bool musicDB::compareGenre::operator() (uint32_t entryNrA, uint32_t entryNrB)
{
	// Load data:
	string sA, sB;
	const char *cA = invalidStr, *cB=invalidStr;
	if( entryNrA < offset->size() ) sA = (*this)[entryNrA].genre,	cA=sA.c_str();	//need to keep sA, else cA becomes invalid
	if( entryNrB < offset->size() ) sB = (*this)[entryNrB].genre,	cB=sB.c_str();
	return strncasecmp(cA , cB, maxStrLen )<0;		// Compare (only up to strLen(invalidStr) characters)
}




int musicDB::init(const char *dbName, const char *idxName)
{
	// only scan if index is missing
	int loadedEntries =  load(dbName, idxName);
	if( loadedEntries <= -2)
	{
		db_printf(1,"Scanning '%s'\n", basePath.c_str() );
		scan( dbName );
	}

	if( loadedEntries <= -1)
	{
		db_printf(1,"Sorting results\n");
		index( idxName );
	}

	return nrEntries;
}



// Scan the file system for audio files, and read the tags
void musicDB::scan(const char *dbName)
{
	vector<uint32_t> dynOffset;		//offset into f_db, per entry

	std::list<dbEntry> entries;
	std::string relpath = "";	//need a reference, so can't pass a "" to listdirRecursive()

	db_printf(3,"List recursive: '%s','%s'\n", basePath.c_str(), relpath.c_str() );
	listdirRecursive( basePath, relpath, entries );

	// Generate data file
	f_db = fopen(dbName, "wb");			//file with all data

	//write header:
	fwrite( "sqdb", 4, 1, f_db);
	//store path, to verify the loading (including zero-terminator:
	fwrite( basePath.c_str(), basePath.size()+1, 1, f_db);


	for (list<dbEntry>::iterator it = entries.begin(); it != entries.end(); it++)
	{
		dynOffset.push_back( ftell(f_db) );
		it->pickle( f_db );
	}
	fclose(f_db);

	// Make the db ready for usage
	this->nrEntries = dynOffset.size();
	printf(" musicDB::scan(): found %llu entries\n", (LLU)nrEntries);

	this->offset.resize(nrEntries); // = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)
		this->offset[i] = dynOffset[i];

	f_db = fopen( dbName, "rb" );
}







// Load a musicDB from disk, including the index file
int musicDB::load(const char *dbName, const char *idxName)
{
	vector<uint32_t> dynOffset;		//offset into f_db, per entry

	//Close db, if it's open:
	if( f_db != NULL)
		fclose(f_db);

	f_db = fopen(dbName, "rb");
	if( f_db == NULL)
		return -2;

	//Check header:
	char hdr[4];
	fread( hdr, 4, 1, f_db);
	if( memcmp( hdr, "sqdb", 4 )!=0)
		return -2;

	//Verify that db was generate using the same path:
	char dbPath[200];
	readCstring( dbPath, sizeof(dbPath), f_db);
	if( strcmp( dbPath, basePath.c_str()) != 0)
		return -2;

	while( !feof(f_db) )
	{
		dynOffset.push_back( ftell(f_db) );	//keep track of file offsets
		dbEntry e(f_db);
	}
	dynOffset.pop_back();	//last item is at EOF, remove it.

	// Make the db ready for usage
	this->nrEntries = dynOffset.size();

	this->offset.resize(nrEntries); // = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)
		this->offset[i] = dynOffset[i];


	// Load the sorted indices:
	FILE *f_idx= fopen(idxName, "rb");	//file with sorted indices into data
	//	first store header: number of entries, and an index into fdb:
	if( f_idx == NULL)
		return -1;

	uint32_t nrItems32 = 0;
	int n = fread( &nrItems32, sizeof(nrItems32), 1, f_idx);
	if( nrItems32 != nrEntries)
	{
		printf("invalid index file, it has %llu instead of %llu entries\n", (LLU)nrItems32, (LLU)nrEntries);
		fclose(f_idx);
		return -1;
	}

	//for now: verify this, instead of just loading it:
	uint32_t off;
	for(size_t i = 0; i< nrEntries; i++)
	{
		n = fread( &off, sizeof(uint32_t), 1, f_idx);
		if( offset[i] != off )
		{
			fclose(f_idx);
			printf("offset[%llu] is %i instead of %i\n", (LLU)i, off, offset[i]);
			fclose(f_idx);
			return -1;
		}
	}

	// Read sorted tables
	for(int table=0; table < IDX_END-1; table++)
	{
		idx[table].resize(nrEntries);
		for(size_t i=0; i< nrEntries; i++)
			n = fread( &idx[table][i], 1, sizeof(uint32_t), f_idx);
	}

	// cleanup:
	fclose(f_idx);

	return nrEntries;
}



// Create sorted indices for the datasets, store the to disk
void musicDB::index(const char *idxName)
{
	//initialize a temporary index:
	vector<uint32_t> tidx;
	tidx.resize( nrEntries );
	for(size_t i=0; i< nrEntries; i++)	tidx[i] = i;

	// Sort by title:
	db_printf(5,"\n\tSorting by title:\n");
	sort( tidx.begin(), tidx.end(), compareTitle(f_db, &offset) );
	idx[IDX_TITLE].resize(nrEntries);	// = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)
	{
		idx[IDX_TITLE][i] = tidx[i];

		fseek(f_db, offset[ tidx[i] ], SEEK_SET);
		dbEntry e(f_db);
		if( e.title.length() < 1) {
			db_printf(55,"%2u: empty tags: '%s'\n", tidx[i], e.fileName.c_str() );
		} else {
			db_printf(55,"%2u: %s\n", tidx[i], e.title.c_str() );
		}
	}

	// Sort by artist
	sort( tidx.begin(), tidx.end(), compareArtist(f_db, &offset ) );
	idx[IDX_ARTIST].resize(nrEntries);// = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)	idx[IDX_ARTIST][i] = tidx[i];

	// Sort by Album
	sort( tidx.begin(), tidx.end(), compareAlbum(f_db, &offset ) );
	idx[IDX_ALBUM].resize(nrEntries); // = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)	idx[IDX_ALBUM][i] = tidx[i];

	// Sort by Genre
	sort( tidx.begin(), tidx.end(), compareGenre(f_db, &offset ) );
	idx[IDX_GENRE].resize(nrEntries); // = new uint32_t[ nrEntries ];
	for(size_t i=0; i< nrEntries; i++)	idx[IDX_GENRE][i] = tidx[i];


	//Write to disk:
	FILE *f_idx= fopen(idxName, "wb");	//file with sorted indices into data

	//	first store header: number of entries, and an index into fdb:
	uint32_t nrItems32 = nrEntries;
	fwrite( &nrItems32, sizeof(nrItems32), 1, f_idx);
	for(size_t i = 0; i< nrEntries; i++)
		fwrite( &offset[i], sizeof(uint32_t), 1, f_idx);
	//stored sorted tables:
	for(int table=0; table < IDX_END-1; table++)
		for(size_t i=0; i< nrEntries; i++)
			fwrite( &idx[table][i], 1, sizeof(uint32_t), f_idx);

	// cleanup:
	fclose(f_idx);
}


//--------------------------- Database searching -----------------------------------------


const char *dbFieldStr[] = {
	"Invalid",
	"Any",
	"All",
	"Title",
	"Album",
	"Artist",
	"Year",
	"Genre",
};


///Binary search, since std::lower_bound is inconvenient if Compare has virtual members:

//returns first matching item in <first,last>, starting with 'value'
//	code is from std::lower_bound, but this version accepts a dynamic compare object
template <class iter, class T, class Compare>
iter binSearchLowerIt(iter first, iter last, const T& value, Compare *comp, bool findLower = true)
{
	size_t count  = last-first;
	for(; 0 < count; )
	{
		size_t count2 = count / 2;
		iter mid    = first;
		mid += count2;
		if( (*comp)( *mid, value) ) {
			first  = ++mid;
			count -= count2 + 1;
		} else
			count = count2;
	}
	return first;
}

template <class iter, class T, class Compare>
iter binSearchUpperIt(iter first, iter last, const T& value, Compare *comp, bool findLower = true)
{
	size_t count  = last-first;
	for(; 0 < count; )
	{
		size_t count2 = count / 2;
		iter mid    = first;
		mid += count2;
		if( !(*comp)( value, *mid) ) {
			first  = ++mid;
			count -= count2 + 1;
		} else
			count = count2;
	}
	return first;
}





void dbQuery::find()
{
	uint32_t val = (uint32_t)(-1);	//in the compare functions, all invalid indices are evaluated as *match.
	/*switch(idxField)
		begin = std::lower_bound( idxIn.begin(), idxIn.end(), val, musicDB::compareArtist( db->f_db, &db->offset, match ) );
		end   = std::upper_bound( idxIn.begin(), idxIn.end(), val, musicDB::compareArtist( db->f_db, &db->offset, match ) );
	}*/

	musicDB::compare *cmp;
	switch(field)
	{
		case DB_TITLE:	cmp = new musicDB::compareTitle( db->f_db, &db->offset, match ); break;
		case DB_ALBUM:	cmp = new musicDB::compareAlbum( db->f_db, &db->offset, match ); break;
		case DB_ARTIST:	cmp = new musicDB::compareArtist(db->f_db, &db->offset, match ); break;
		case DB_GENRE:	cmp = new musicDB::compareGenre( db->f_db, &db->offset, match ); break;
		default:        cmp = NULL;
	}

	//TODO: check what happens if match is not found.
	if( match[0] == 0)
	{
		//include everything
		ilen = itEnd - itBegin;
	}
	else if(cmp != NULL)
	{
		std::vector<uint32_t>::iterator rBegin, rEnd;
		//itBegin = lower_bound( itBegin, itEnd, val, *cmp);	//doesn't work
		itBegin = binSearchLowerIt( itBegin, itEnd, val, cmp);
		itEnd   = binSearchUpperIt( itBegin, itEnd, val, cmp);
		/*for( rEnd = itBegin; rEnd != itEnd; rEnd++)
			if( (*cmp)(val, *rEnd) )	break;
		itEnd = rEnd;*/
		ilen   = itEnd - itBegin;	//TODO: verify this
		delete cmp;
	} else {
		ibegin = iend = ilen = 0;
	}
}



/// Start a new query
dbQuery::dbQuery(musicDB *db, dbField field, const char *match):
	db(db), match(match), field(field)
{
	musicDB::IDX_ID idxField = musicDB::IDX_END;
	switch(field)
	{
		case DB_TITLE:	idxField = musicDB::IDX_TITLE; break;
		case DB_ALBUM:	idxField = musicDB::IDX_ALBUM; break;
		case DB_ARTIST:	idxField = musicDB::IDX_ARTIST; break;
		default:        idxField = musicDB::IDX_END;
	}

	if( idxField != musicDB::IDX_END )
	{
		//set iterators to acces the base list:
		idx =  &db->idx[idxField];
		ownIdx = false;
		//itBegin = idx->begin(); //doens't work since idx is const
		itBegin = db->idx[idxField].begin();
		itEnd   = db->idx[idxField].end();

		//call the find function to update itBegin and itEnd:
		find();

		//update unique indices:
		uniqueIdx.clear();
		if( itBegin != itEnd)
		{
			uniqueIdx.push_back( uIdx(*itBegin, 1 ) );
			string lastToken = (*db)[ uniqueIdx.back().idx ].getField(field);

			for(std::vector<uint32_t>::iterator it=itBegin; it != itEnd; it++)
			{
				dbEntry r= (*db)[*it];
				if( r.getField(field) == lastToken )
				{
					uniqueIdx.back().count++;
				} else {
					lastToken = r.getField(field);
					uniqueIdx.push_back(  uIdx(*it, 1 ) );
				}
			}
		}

		db_printf(5,"found %lli items for key %s, match '%s'. %lli unique\n", (long long)(itEnd-itBegin), dbFieldStr[field], match, (long long)(uniqueIdx.size()) );
		if( ilen < 20 )
		{
			for(std::vector<uint32_t>::iterator it=itBegin; it != itEnd; it++)
			{
				dbEntry r= (*db)[*it];
				db_printf(5,"\t%s - %s, %s\n", r.artist.c_str(), r.title.c_str(), r.album.c_str() );
			}
		}
	} //if (idxField != ...
}



/// Query on results of another Query
dbQuery::dbQuery(musicDB *db, dbQuery &base, dbField field, const char *match):
		db(db), match(match),  field(field)
{
	this->field = field;

	if( field != base.getField() )
	{
		//different field than parent query, need to copy and re-sort:
		musicDB::vec32_t *idxIn = new musicDB::vec32_t[ base.size() ];
		std::copy( base.begin(), base.end(), idxIn->begin() );
		idx = idxIn;
		ownIdx = true;

		switch(field)
		{
		case DB_TITLE:
			sort( idxIn->begin(), idxIn->end(), musicDB::compareTitle( db->f_db, &db->offset, match) );
		case DB_ARTIST:
			sort<musicDB::vec32_t::iterator, musicDB::compareArtist>( idxIn->begin(), idxIn->end(), musicDB::compareArtist( db->f_db, &db->offset, match) );
		case DB_ALBUM:
			sort( idxIn->begin(), idxIn->end(), musicDB::compareAlbum( db->f_db, &db->offset, match) );
		case DB_GENRE:
			sort( idxIn->begin(), idxIn->end(), musicDB::compareGenre( db->f_db, &db->offset, match) );
        default:
            break;
		}
		itBegin = idxIn->begin();
		itEnd   = idxIn->end();
	} else {
		//same field, just use a reference:
		idx = base.getIdx();
		ownIdx = false;

		itBegin = base.begin();
		itEnd   = base.end();
	}

	//call the find function to update itBegin and itEnd:
	find();
}

