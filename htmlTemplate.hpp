/* Base classes for dynamic html generation.
 *
 * Parsing is a simple search-and-replace using template files
 *
 */

#include <string>
#include <vector>
#include <fstream>

using namespace std;


//----------------- Keyword replacement functions ----------------------

/// Base class for all template-matchers
class keywordMatcher
{
public:
	/// Return a list of keywords processed by this class
	virtual vector<string> keyWords(void)=0;

	/// Generate content for a single keyword:
	virtual string replace(string keyword)=0;
};



/// Base class for a template matcher that generates
/// a list (directory-listing, playlist)
class keywordMatcherMulti: public keywordMatcher
{
private:
protected:
	const char* itemTemplate; // Template for a single item
public:
	keywordMatcherMulti(const char* itemTemplate):
		itemTemplate(itemTemplate)
	{
	}

	/// Generate content for the whole list:
	virtual string replace(string keyword)=0;
};


/// Combine the different template matchers into a single group:
/// for each 'templates[x]' the templates[x]._first is pre-pended to the keyword,
//  to allow hierarchy.
class keywordMatcherGroup : keywordMatcher
{
private:
	map<string,keywordMatcher> *subMatchers;
public:
	keywordMatcherGroup( map<string,keywordMatcher> *subMatchers):
		subMatchers(subMatchers)
	{
	}

	//TODO: pre-pend template names to the keyword:
	vector<string> keyWords(void)
	{
	    return vector<string>();
	}
};





//----------------- Connection handlers for dynamic content -------------------


/// Base class to generate dynamic content:
class dHtmlHandler
{
public:
	//Send data to socket, given the urlPath.
    // urlPath is relative, base has been strippd.
	virtual string generateData(const char *templateData)=0;
};



/// Base class to generate content using template-matching
class dHtmlTemplate: public dHtmlHandler
{
protected:
	keywordMatcher* matchFcn;	//function to parse keywords into results

public:
	dHtmlTemplate(keywordMatcher* matchFcn):
		matchFcn(matchFcn)
	{
	}

	string generateData(const char* templateData)
	{
		return generateData(templateData, *matchFcn);
	}

	//Allow the function to be called without class instantiation:
	static string generateData(const char* templateData, keywordMatcher& matchFcn)
	{
		string parsedData,kw;
        //load template
        //search for keywords,
		const char *s1 = templateData;	//start of current block
		const char *p  = s1;					//current position.
        const char *end= s1==NULL? NULL: s1+strlen(s1); // + templateData.size();

		while( p < end )
		{
			p = strchr(p, '#' ); // Search for start of keyword
			if( p == NULL )
			{
				//no keywords found, copy remaing data:
				parsedData.append( s1 );
				break;
			}
			parsedData.append( s1, p-s1 ); // copy all [s1..p] into output

			s1 = p;
			p = strchr(p+1, '#' ); // Search for end of keyword
			if( p == NULL )
				break;
			if( p == s1+1)		//a double ## is the escape
			{
				parsedData.push_back( *p );
			}
			else				//no escape, really insert the keyword
			{
				kw.assign( s1+1, p-s1-1);
				pstring::tolower( kw );

				// replace keyword by value:
				parsedData.append( matchFcn.replace( kw ) );
			}

			p++;	//skip end of delimiter
			s1 = p;
		}
		return parsedData;
	}
};




//------------------- File template functions --------------------------


/// Generate html for a single file:
class keywordMatcherFile: public keywordMatcher
{
	//string *realName;
	const string *vfsName;
	vector<string> kw;
public:
	keywordMatcherFile(const string& vfsName):
		vfsName(&vfsName)
	  {
		kw.push_back("relurl");
		kw.push_back("name_escaped");
		kw.push_back("name");
		kw.push_back("mime");
	  }

	vector<string> keyWords(void)
	{
		return kw;
	}

	string replace(string keyword)
	{
		string out;
		if( keyword == "relurl" )
			out = *vfsName;
		else if( keyword == "name_escaped" )
			out = path::escape( *vfsName );
		else if( keyword == "name" )
		{
			vector<string> items = pstring::split(*vfsName,'/');
			out = items.back(); //[1];
			//out = path::split(*vfsName)[1];
		}
		else if( keyword == "mimetype" )
		{
			const char *ext = strrchr(vfsName->c_str(), '.');
			out = getMime(ext);
		}
		return out;
	}

};



/// Generate listing of a directory:
class keywordMatcherDirlist: public keywordMatcherMulti
{
	const char *vfsPath, *realPath;
	vector<string> kw;
public:
	keywordMatcherDirlist(const char* itemTemplate,
						  const char *vfsPath,
						  const char *realPath):
		keywordMatcherMulti(itemTemplate),
		vfsPath(vfsPath),
		realPath(realPath)
	{
		kw.push_back("base");
		kw.push_back("filelist");
	}

	vector<string> keyWords(void)
	{
		return kw;
	}

	string replace(string keyword)
	{
		string out;
		if( keyword == "base" )
			out = path::unescape(vfsPath);
		else if(keyword == "filelist" )
		{
			std::vector<std::string> files = path::listdir( realPath , true );	//generate sorted file-list
			for(size_t i=0; i<files.size(); i++)
			{
				//string fullName = path::join( realPath, files[i]);
				string vfsName  = vfsPath;
				if( vfsName[vfsName.size()-1] != '/')
					vfsName.push_back('/');	//always use forward slashes
				vfsName.append( files[i] );

				keywordMatcherFile tmp( vfsName );	//key-word replace function
				dHtmlTemplate      gen( &tmp );			//calls key-word replace
				out.append( gen.generateData(itemTemplate) );
			}
		} //if keyword
		return out;
	}

}; //class htmlTemplateDirlist





/// Server content of a virtual-file system.
/// html-files are parsed through dHtmlTemplate()
/// directories are parsed through another dHtmlTemplate()
/// other files are served directly
class dHtmlVFS: public dHtmlTemplate
{
	string basePath;	//path to html-data
	string vfsPath;
	//string *dirTemplate, *dirItemTemplate;
	const char *dirTemplate, *dirItemTemplate;
public:
	dHtmlVFS(keywordMatcher* matchFcn, //keyword matcher to apply on the requested file
			 string vfsPath,	//relative path in the HTML request
			 string basePath,	//absolute path on the disk
			 const char *dirTemplate,		//template file for directoy listing
			 const char *dirItemTemplate	//template file for item in directoy listing
			 ):
		dHtmlTemplate(matchFcn),
		basePath(basePath),
		vfsPath(vfsPath),
		dirTemplate(dirTemplate),
		dirItemTemplate(dirItemTemplate)
	{
	}


	nbuffer::buffer* generateData(string fname)
	{
		nbuffer::buffer *outBuf = NULL;
		if( path::isfile(fname) )
		{
			// get mime type, now only based on extension:
			const char *ext = strrchr(fname.c_str(), '.');
			string mime = getMime(ext);
			string mimeStart = "text";
			if( pstring::startswith(mime, mimeStart) )
			{	// Parse the data, then send it
				// Load into memory:
				ifstream filestr;
				filestr.open( fname.c_str(), ios_base::in );
				std::stringstream ss;
				ss << filestr.rdbuf();

				// Search keywords, replace them using:
				dHtmlTemplate parser( matchFcn );
				string parsedData = parser.generateData( ss.str().c_str() );
				outBuf = new nbuffer::bufferString( parsedData );
			}
			else
			{	// Send data without touching it:
				outBuf = new nbuffer::bufferFile( fname.c_str() );
			}
		}
		else	//not a file, generate directory listing
		{
			keywordMatcherDirlist dirMatchFcn( dirItemTemplate, vfsPath.c_str(), fname.c_str() );
			dHtmlTemplate parser( &dirMatchFcn );
			string parsedData =  parser.generateData( dirTemplate );
			outBuf = new nbuffer::bufferString( parsedData );
		}
		return outBuf;
	} //generateData
}; // class dHtmlVFS

