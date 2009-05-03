
#include <stdlib.h>

#include <fstream>
#include <sstream>
#include "util.hpp"


#include "configParser.hpp"


configValue::configValue(char val)
	{
		data = val;
	}

configValue::configValue(int val)
{
	std::stringstream tmp;
	tmp << val;
	data = tmp.str();
}


configValue::configValue(size_t val)
{
	std::stringstream tmp;
	tmp << val;
	data = tmp.str();
}


//convert from string to another type:
configValue::operator bool()
{
	bool ret = false;
	if( data == "1" || data == "true" || data == "yes" )
		ret = true;
	return ret;
}

configValue::operator string()		{return data; }
configValue::operator const char*()	{return data.c_str(); }
configValue::operator int()			{return atoi( data.c_str() ); }





// Read configuration from disk
void configParser::read(string filename)
{
	this->filename = filename;
	string line;
	ifstream myfile( filename.c_str() );
	string cSection;		//current section
	string cComment="";		//current comment

	if (myfile.is_open())
	{
		while (! myfile.eof() )
		{
			getline(myfile, line);
			if( line.size() < 1 )
				continue;
			else if( (line[0] == ';') || (line[0] == '#') )
				cComment += line;
			else if( (line[0] == '[') )
			{
				size_t end = line.find(']');
				cSection = line.substr(1, end-1);
				//store comments, so the can be written back:
				commentSection[cSection] = cComment;
				cComment = "";
			} else {
				vector<string> keyVal = pstring::split(line, '=' );
				string cOption = pstring::strip(keyVal[0], ' ');
				config[cSection][cOption] = pstring::strip(keyVal[1], ' ');
				//store comments, so the can be written back:
				commentOption[cSection][cOption] = cComment;
				cComment = "";
			}
		}
		myfile.close();
	}
}



// Write config to disk
void configParser::write(string fname )
{
	if( fname.size() < 1)
		fname = filename;
	string line;
	ofstream myfile( fname.c_str() );

	for(config_t::iterator itSec = config.begin(); itSec != config.end(); itSec++)
	{
		string secName = itSec->first;
		section_t *sec  = &itSec->second;
		myfile << "[" << secName << "]" << endl;
		for(section_t::iterator itItem = sec->begin(); itItem != sec->end(); itItem ++)
		{
			const string &itemName = itItem->first;
			configValue &item      = itItem->second;
			string itemStr = item;
			myfile << itemName << " = " << itemStr << endl;
		}
	}
	myfile.close();
}



//get function, the conversion operators of configValue
// should take care of type-conversion
configValue configParser::get(string section, string option, configValue defaultValue )
{
	if( hasOption(section,option) )
		return config[section][option];
	else
		return defaultValue;
}



bool configParser::hasOption(string section, string option)
{
	if( config.find(section) == config.end() )
		return false;
	section_t sec = config[section];
	bool hasOpt = (sec.find(option) != sec.end() );
	return hasOpt;
}
