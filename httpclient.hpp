/* copy of python's http.client implementation
*
* http://docs.python.org/3.1/library/http.client.html
*/

#include <vector>
#include <string>
#include <map>
using namespace std;

#include "socket.hpp"


//see http://docs.python.org/dev/py3k/library/urllib.parse.html
namespace urllib {
	namespace parse {
		//map<string,string> urlparse(string url);

		map<string,string> urlsplit(string url);

	} //namespace parse
} //namespace urllib



namespace http
	{


	/// Parse url parameters in the form http://127.0.0.1/stream?player=str?something=else
	std::map<std::string,std::string> parseUrlParams(std::string paramStr, std::string *fname=NULL);

	/// Parse the header of an http-request
	class parseRequestHeader
	{
	private:
		 parseRequestHeader();
		 const char *hparam;
		 const char *headerStart;
	public:
		//First line of an html query:
		string method,path,version;

		map<string,string> headerParam;	//header parameters, by name

		parseRequestHeader(const char *data);

		/// Extract a parameter from the url
		string getUrlParam(string name);

		/// Extract a cookie from the header:
		string getCookie(string name);
	};



	namespace client {
		const int HTTP_PORT=80;
		const int HTTPS_PORT=443;

		/// Http status codes:
		enum status {CONTINUE=100,OK=200,NOT_FOUND=404};

		/// Todo: add dictionary 'responses' to map status-enum to a string
		/*struct tst{
			enum status stat;
			const char  *desc;
		};*/


		class HTTPResponse
		{
		private:
			Socket *sock;	//socket to the connection

			string _header;	//header as a string.
			string _body;	//data returned from server
			int	   _alreadyRead;	//bytes in _body that have been read() already

			//parsed header:
			string version;				//http version in header
			//map<string,string> hdrMap;	//parsed header elements
			void parseHeader(void);	//parse '_header' into status and hdrMap
		public:
			/// parsed header results:
			int	   status;	// return code from server
			string reason;	// description of the status code
			//string reason(void);	//string of _status;

			HTTPResponse(Socket* sock);


			/// Returns the body, up to 'amt' bytes
			string& read(int amt=-1);

			/// Return the body to f
			size_t readto(FILE *f);

			string& getheaders(void)
			{
				return _header;
			}

			string getheader(string name, string def="");
		};



		class HTTPConnection
		{
		private:
			string hostName;
			string host,path;
			int port;
			Socket sock;	//todo: make this a shared pointer with HTTPResponse()

			//disable copying until Socket can properly be copied.
			HTTPConnection(const HTTPConnection& conn);

		public:
			HTTPConnection(string host, int port=80)
			{
				//split url into hostname and path
				//extract port name from url
				this->port = port;
				hostName = host;
				this->host = sock.getHostByName( host.c_str() );
			}

			~HTTPConnection()
			{
				sock.Close();
			}


			void request(string method = "GET",string url="/", string headers="", string body="");


			HTTPResponse getresponse(void)
			{
				return HTTPResponse( &sock );
			}

			void close(void)
			{
				sock.Close();
			}

		};

	} //namespace client
} // namespace http
