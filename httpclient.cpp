/* copy of python's http.client implementation
*
* http://docs.python.org/3.1/library/http.client.html
*/


#include <string.h>
#include <stdio.h>

#include "httpclient.hpp"	//includes winsock.h, so do that before any windows.h stuff
#include "util.hpp"




//see http://docs.python.org/dev/py3k/library/urllib.parse.html
namespace urllib {
	namespace parse {
		map<string,string> urlparse(string url)
		{
			map<string,string> ret;
			//split url into scheme,host:port,path,params,query,fragment
			return ret;
		}


		/** Split a url string into separate components.
			Attribute 	Index 	Value 	Value if not present
			scheme 	0 	URL scheme specifier 	empty string
			netloc 	1 	Network location part 	empty string
			path 	2 	Hierarchical path 	empty string
			query 	3 	Query component 	empty string
			fragment 	4 	Fragment identifier 	empty string
			username 	  	User name 	None
			password 	  	Password 	None
			hostname 	  	Host name (lower case) 	None
			port 	  	Port number as integer, if present 	None
		*/
		map<string,string> urlsplit(string url)
		{
			map<string,string> ret;
			//split url into parts, without separating params
			size_t idx0 = url.find(':');
			size_t idx1 = url.find("//");

			string netloc = url.substr( idx1 + 2 );
			size_t idx2 = netloc.find('/');

			if( idx1 < url.size() )
				ret["scheme"] = url.substr(0,idx0);
			if( idx2 < netloc.size() )
			{
				ret["netloc"] = netloc.substr(0, idx2);
				ret["path"]   = netloc.substr(idx2);
			}

			return ret;
		}


	} //namespace parse
} //namespace urllib



namespace http
	{


	/// Parse url parameters in the form http://127.0.0.1/stream?player=str?something=else
	std::map<std::string,std::string> parseUrlParams(std::string paramStr, std::string *fname)
	{
		std::map<std::string,std::string> ret;
		std::vector<std::string> pairs = pstring::split(paramStr, '?');

		//first pair is the filename, so skip that:
		if(fname != NULL)
			*fname = pairs[0];
		for(size_t i=1; i< pairs.size(); i++)
		{
			std::vector<std::string> keyVal = pstring::split( pairs[i] , '=' );
			if( keyVal.size() == 2 )
				ret[ keyVal[0] ] = keyVal[1];
		}

		return ret;	//return a copy....
	}


	parseRequestHeader::parseRequestHeader(const char *data)
	{
		const char *m,*p,*v,*e; //method,path,version of first line
		m = data;

		// split first line into methd,path,version
		p = strchr(m, ' ');
		if(p == NULL) return;
		method.assign(m, p-m);
		p++;

		v = strchr(p,' ');
		if(v == NULL) return;
		path.assign(p, v-p);
		v++;

		e = strpbrk(v, "\r\n");
		if(e == NULL) return;
		version.assign(v, e-v);
		e++;

		// Parse the header parameters:
		const char *str = strpbrk(e,"\n\r"); // Skip the first line
		const char *end = data + strlen(data);
		while( (str < end) && (str!=NULL) )
		{
			str++;		// Move past the new-line char (strpbrk already skips on char
			const char *sep = strchr(str,':');
			const char *next= (str==NULL) ? NULL : strpbrk(str,"\r\n");
			if( (sep!=NULL) && (next!=NULL) && (sep<next) )
			{
				string pname, pval;
				while(*sep==' ') sep++;	// strip spaces

				pname.assign( str, sep-str );
				pval.assign( sep+1, next-sep-1 );
				headerParam[pname] = pval;
			}
			str = next;
		}
	} // parseRequestHeader::parseRequestHeader()


	// Extract a parameter from the url
	string parseRequestHeader::getUrlParam(string name)
	{
		string out;
		const char *str = strchr(path.c_str(), '?' );
		const char *end = path.data() + path.size();
		while( (str != end) && (str!=NULL) )
		{
			const char *sep = strchr(str  ,'=');
			const char *next= strpbrk(str+1,"?");			//next value
			if (next==NULL) next = end;

			if( (sep!=NULL) && (sep<next) )
			{
				if (strncmp( name.c_str(), str+1, name.size()) == 0)
				{
					out.assign( sep+1, next-sep-1 );
					break;
				}
			}
			str = next;
		} //while( str != NULL )
		return out;
	} //parseRequestHeader::getUrlParam()



	// Extract a cookie from the header:
	string parseRequestHeader::getCookie(string name)
	{
		string out;
		string cookie = headerParam["Cookie"];
		const char *str = cookie.c_str();
		const char *end = cookie.data() + cookie.size();
		while( str != end )
		{
			while( *str == ' ') str++; //strip spaces
			const char *sep = strchr(str  ,'=');
			const char *next= (str==NULL) ? NULL : strchr(str+1,';');	//next value
			if(next == NULL) next = end;

			if( (sep!=NULL) && (sep<next) )
			{
				if (strncmp( name.c_str(), str, name.size()) == 0)
				{
					out.assign( sep+1, next-sep-1 );
					break;
				}
			}
			str = next;
		} //while( str != NULL )
		return out;
	} //parseRequestHeader::getCookie()




	namespace client
	{

		HTTPResponse::HTTPResponse(Socket* sock):
				sock(sock),
				_alreadyRead(0),
				status(0)
		{
			//Read the header
			char buf[1024];
			int hist2 = 0;
			int hist4 = 0;
			int  n;
			char headerEnd=0;

			do
			{
				n = sock->Recv(buf, sizeof(buf));
				if( n < 0)
					break;
				for(int i=0; i<n; i++)
				{
					hist4 = ((hist4<<8) | buf[i]) & 0xFFFFFFFF;
					hist2 = hist4 & 0xFFFF;
					if ((hist2==0x0D0D) || (hist2==0x0A0A) || (hist4==0x0d0a0d0a) || (hist4==0x0a0d0a0d) )
					{
						_header.append( buf, i-3 );
						_body.append(  buf+i+1, n-i-1);
						headerEnd=1;
						break;
					}
				}//for i
				if( headerEnd )
					break;
				//if( !headerEnd)
				_header.append(buf, n);
			}
			while( n > 0 );

			//Parse the first line of the header:
			const char *s0 = _header.c_str();
			const char *s1 = strchr( s0 , ' ');		//first space
			const char *n1 = strchr( s0 , '\r');	//newline
			const char *s2 = (s1==NULL) ? NULL : ((*s1 == 0) ? NULL : strchr(s1+1, ' '));	//second space
			if( (s1!=NULL) && (s1 < s2) && (s2 < n1) )
			{
				version.assign(s0,s1-s0);
				status = atoi(s1+1);
				reason.assign(s2+1, n1-s2-1);
			}
		}



		string& HTTPResponse::read(int amt)
		{
			char buf[1024];

			//Flush data that has already been read:
			_body.erase(0,_alreadyRead);

			//If amt is given, read a limited amount of data:
			int toRead = amt - _body.length();
			while( toRead>0 )
			{
				int n = sock->Recv(buf, min<long>(sizeof(buf),toRead) );
				if( n < 0 )
					return _body;
				_body.append( buf, n);
				toRead = amt - _body.length();
			}


			if( amt > 0)	//only return a part of the data
			{
				_alreadyRead = amt;
			}
			else			//return all data
			{
				int n=0;
				do
				{
					_body.append( buf, n );
					n = sock->Recv(buf, sizeof(buf));
				} while( n > 0 );
				_alreadyRead = _body.length();
			}

			return _body;
		}



		size_t HTTPResponse::readto(FILE *f)
		{
			size_t nread=0;
			char buf[1024];
			int n=0;
			//store part of the body that has already been fetched:
			int  size = _body.size();
			if( size > 0)
			{
				int nb = fwrite( _body.c_str(), size, 1, f);
				if ( nb != 1)
					return nread;
				nread += _body.size();
			}

			do
			{
				n = sock->Recv(buf, sizeof(buf));
				fwrite(buf, n, 1, f);
				nread += n;
			} while( n > 0 );
			return nread;
		}



		string HTTPResponse::getheader(string name, string def)
		{
			const char *str = _header.c_str();
			const char *end = _header.c_str() + _header.size();
			string ret = def;

			str = strchr(str,'\n'); // Skip the first line
			while( (str < end) && (str!=NULL) )
			{
				str++;	// Move past the new-line char
				const char *sep = strchr(str,':');
				const char *next= strchr(str,'\n');
				if( (sep!=NULL) && (next!=NULL) && (sep<next) )
				{
					sep++; // Skip separator sign
					if (strncmp( name.c_str(), str, name.size()) == 0)
					{	//if header name is identical, copy the value to the output:
						while( *sep==' ' ) { sep++; }	//skip spaces
						ret.assign(sep, next-sep-1);
						break;
					}
				}
				str = next;
			}
			return ret;
		}


		void HTTPConnection::request(string method, string url, string headers, string body)
		{
			int port=80;
			char str[80];

			sock.Connect(host.c_str(), port);
			string data = method;
			data.append(" ");
			data.append(url);
			data.append(" HTTP/1.1\r\n");
			//Fill in some headers (and hope their not given)
			data.append( string("Host: ") + hostName + string("\r\n") );
			if( body.size() > 0 )
			{
				sprintf(str,"Content-Length: %llu\r\n", (LLU)body.length() );
				data.append(str);
			}

			if(headers.length() > 0)
			{
				data.append(headers);
				data.append("\r\n");
			}
			data.append(body);
			//printf("HTTPConnection::request(): data to send:\n%s\n\n", data.c_str() );
			//int n =
			sock.Send( data.c_str(), data.length() );
		} // HTTPConnection::request()


	} // namespace client
} // namespace http

