/** 
 * @file 
 * Read / write a configuration file
 * the interface is similar to python's configParser
 *	http://docs.python.org/library/configparser.html
 *
 * the internal format should be compatible with:
 *	http://tools.ietf.org/html/rfc822.html
 */


#ifndef _CONFIG_PARSER_HPP_
#define _CONFIG_PARSER_HPP_

#include <vector>
#include <map>
#include <string> 


using namespace std;


/// Provide conversions from c types to standard strings
/// TODO: raise exceptions when something goes wrong
class configValue {
private:
	string data;
public:
	configValue(void)	{ }

	// Convert from c-type to string using constructors:
	configValue(char val);
	configValue(const char *val): data(val) {}
	configValue(string val): data(val) {}
	configValue(int val);
	configValue(size_t val);
	
	// Convert from string to c-type using conversion operator:

	operator bool();
	operator string();
	operator const char*();
	operator int();

};

 
class configParser {
public:
	//a single section consists of [key,value] pairs:
	typedef std::map<string,configValue> section_t;
	//a whole config file consist out of multiple sections:
	typedef map<string, section_t>	config_t;
private:
	string filename;		//current filename
	config_t config;		//configuration data

	//read in comments in the config file,
	// to store them also if the options have change
	map<string, string>	commentSection;
	map<string, map<string,string> > commentOption;

	
public:
	configParser(void)
	{
	}

	//Constructor which accepts default values
	configParser(const char *filename, config_t defaults = config_t() )
	{
		config = defaults;	//initialize with default values
		read(filename);
	}

	/// Read configuration from disk
	void read(string filename);

	/// Write config to disk:
	void write(string filename="");


	///get function, the conversion operators of configValue
	/// should take care of type-conversion
	configValue get(string section, string option, configValue defaultValue=configValue() );

	/// get a value, also set the default value if it's not set yet.
	configValue getset(string section, string option, configValue value )
	{
		if( !hasOption(section,option) )
			set(section,option,value);
		return config[section][option];
	}


	void set(string section, string option, string val)
	{
		config[section][option] = val;
	}

	void set(string section, string option, configValue val)
	{
		config[section][option] = val;
	}

	
	bool hasOption(string section, string option);


	bool hasSection(string section)
	{
		return config.find(section) != config.end();
	}


	/// Return list of options in a section. (not implemented yet)
	vector<string> options(string section);


	/// Return all items in a section
	const section_t& items(string section)
	{
		return config[section];
	}


	bool removeOption(string section, string option);

	bool removeSection(string section);
};


#endif	//#ifdef _CONFIG_PARSER_HPP_
