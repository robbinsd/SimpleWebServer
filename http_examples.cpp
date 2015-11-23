#include "server_http.hpp"
#include "client_http.hpp"
#include "Database.h"
//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;

namespace
{
	Database* s_db = NULL;
};

enum class HttpCode
{
	OK=200,
	BAD_REQUEST=400,
	NOT_FOUND=404
};
ostream& operator<<(ostream& stream, HttpCode code)
{
	string codeName;
	switch (code)
	{
	case HttpCode::OK:
		codeName = "OK";
		break;
	case HttpCode::BAD_REQUEST:
		codeName = "BAD_REQUEST";
		break;
	case HttpCode::NOT_FOUND:
		codeName = "NOT_FOUND";
		break;
	}
	return stream << static_cast<int>(code) << " " << codeName;
}

template <typename T>
void ResponseWithCode(HttpServer::Response& response, HttpCode code, const T& thing)
{
	stringstream thingStream;
	thingStream << thing;
	response << "HTTP/1.1 " << code << "\r\nContent-Length: " << thingStream.tellp() << "\r\n\r\n" << thingStream.rdbuf();
}

class ToStringVisitor : public boost::static_visitor<string>
{
public:
	string operator()(int i) const
	{
		stringstream stream;
		stream << i;
		return stream.str();
	}

	string operator()(bool b) const
	{
		stringstream stream;
		stream << b;
		return stream.str();
	}

	string operator()(const string & str) const
	{
		return str;
	}
};

template <typename R, typename T1, typename T2>
class SwitchCaseConstVisitor : public boost::static_visitor<R>
{
public:
	typedef std::function<R(const T1&)> Type1Handler;
	typedef std::function<R(const T2&)> Type2Handler;
	SwitchCaseConstVisitor(Type1Handler handler1, Type2Handler handler2)
		: m_handler1(handler1)
		, m_handler2(handler2)
	{
	}

	R operator()(T1 type1Value) const
	{
		return m_handler1(type1Value);
	}

	R operator()(T2 type2Value) const
	{
		return m_handler2(type2Value);
	}

protected:
	Type1Handler m_handler1;
	Type2Handler m_handler2;
};

template <typename R, typename T1, typename T2>
R SwitchCaseConst(const boost::variant<T1, T2>& choice, std::function<R(const T1&)> handler1, std::function<R(const T2&)> handler2)
{
	return choice.apply_visitor(SwitchCaseConstVisitor<R,T1,T2>(handler1, handler2));
}

template <typename R, typename T1, typename T2>
class SwitchCaseVisitor : public boost::static_visitor<R>
{
public:
	typedef std::function<R(T1&)> Type1Handler;
	typedef std::function<R(T2&)> Type2Handler;
	SwitchCaseVisitor(Type1Handler handler1, Type2Handler handler2)
		: m_handler1(handler1)
		, m_handler2(handler2)
	{
	}

	R operator()(T1 type1Value) const
	{
		return m_handler1(type1Value);
	}

	R operator()(T2 type2Value) const
	{
		return m_handler2(type2Value);
	}

protected:
	Type1Handler m_handler1;
	Type2Handler m_handler2;
};

template <typename R, typename T1, typename T2>
R SwitchCase(const boost::variant<T1, T2>& choice, std::function<R(T1&)> handler1, std::function<R(T2&)> handler2)
{
	return choice.apply_visitor(SwitchCaseVisitor<R, T1, T2>(handler1, handler2));
}

//POST-example for the path /string, responds the posted string
void StringPost(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	//Retrieve string from istream (request->content)
	stringstream ss;
	ss << request->content.rdbuf();
	string content = ss.str();

	response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}

//POST-example for the path /json, responds firstName+" "+lastName from the posted json
//Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
//Example posted json:
//{
//  "firstName": "John",
//  "lastName": "Smith",
//  "age": 25
//}
void JsonPost(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	try {
		ptree pt;
		read_json(request->content, pt);

		string name = pt.get<string>("firstName") + " " + pt.get<string>("lastName");

		response << "HTTP/1.1 200 OK\r\nContent-Length: " << name.length() << "\r\n\r\n" << name;
	}
	catch (exception& e) {
		response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
	}
}

//GET-example for the path /info
//Responds with request-information
void InfoGet(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	stringstream content_stream;
	content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
	content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
	for (auto& header : request->header) {
		content_stream << header.first << ": " << header.second << "<br>";
	}

	//find length of content_stream (length received using content_stream.tellp())
	content_stream.seekp(0, ios::end);

	response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
}

//GET-example for the path /match/[number], responds with the matched string in path (number)
//For instance a request GET /match/123 will receive: 123
void MatchGet(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	string number = request->path_match[1];
	response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
}

void DbGet(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	string number = request->path_match[1];
	boost::variant<Database::Error, Database::Name> result = s_db->GetName(number);
	/*boost::variant<string, int> result2 = "poop"s;
	cout << result2 << endl;
	boost::variant<int, string> result3 = "poop"s;
	cout << result3 << endl;
	boost::variant<bool, string> result25("poop"s);
	cout << result25 << endl;
	boost::variant<bool, const char*> result26("ding dong");
	cout << result26 << endl;
	auto thing = string("poop2");
	auto thing2 = "poopd";
	auto thing3 = "poopie"s;
	cout << thing << endl;
	boost::variant<int, bool, string> result5 = "poop"s;
	boost::variant<int, bool, string> result6("poop"s);
	boost::variant<int, string, bool> result1 = "thing"s;
	boost::variant<int, string, bool> result7("thing"s);
	cout << result1 << endl;
	result1 = false;
	cout << result1 << endl;
	cout << false << endl;
	cout << true << endl;
	cout << result5 << endl;
	string r5 = result5.apply_visitor(ToStringVisitor());
	result5 = true;
	cout << result5 << endl;
	r5 = result5.apply_visitor(ToStringVisitor());
	result5 = false;
	cout << result5 << endl;
	r5 = result5.apply_visitor(ToStringVisitor());
	result5 = 239;
	cout << result5 << endl;
	r5 = result5.apply_visitor(ToStringVisitor());
	boost::variant<string, string, string> result4 = "poop"s;*/
	SwitchCaseConst<void, Database::Error, Database::Name>(
		result,
		[&response](const Database::Error& error) {
		ResponseWithCode(response, HttpCode::BAD_REQUEST, error.m_message);
	},
		[&response](const Database::Name& name) {
		ResponseWithCode(response, HttpCode::OK, name);
	}
	);
	/*if (result.which() == 0)
	{
	response << result.;
	}
	else if (responseRows.size() == 1)
	{
	ResponseWithCode(response, HttpCode::OK, responseRows.front());
	}
	else
	{
	ResponseWithCode(response, HttpCode::BAD_REQUEST, "No name found for that id.");
	}*/
}

void DbPut(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	string id = request->path_match[1];
	stringstream nameStream;
	nameStream << request->content.rdbuf();
	string errorMsg = s_db->ReplaceName(id, nameStream.str()).m_message;
	if (errorMsg.empty())
	{
		ResponseWithCode(response, HttpCode::OK, "Success");
	}
	else
	{
		ResponseWithCode(response, HttpCode::BAD_REQUEST, errorMsg);
	}
}

//Default GET-example. If no other matches, this anonymous function will be called. 
//Will respond with content in the web/-directory, and its subdirectories.
//Default file: index.html
//Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
void WebGet(HttpServer::Response& response, shared_ptr<HttpServer::Request> request) {
	boost::filesystem::path web_root_path("web");
	if (!boost::filesystem::exists(web_root_path))
		cerr << "Could not find web root." << endl;
	else {
		auto path = web_root_path;
		path += request->path;
		if (boost::filesystem::exists(path)) {
			if (boost::filesystem::canonical(web_root_path) <= boost::filesystem::canonical(path)) {
				if (boost::filesystem::is_directory(path))
					path += "/index.html";
				if (boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
					ifstream ifs;
					ifs.open(path.string(), ifstream::in | ios::binary);

					if (ifs) {
						ifs.seekg(0, ios::end);
						size_t length = ifs.tellg();

						ifs.seekg(0, ios::beg);

						response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";

						//read and send 128 KB at a time
						size_t buffer_size = 131072;
						vector<char> buffer;
						buffer.resize(buffer_size);
						size_t read_length;
						try {
							while ((read_length = ifs.read(&buffer[0], buffer_size).gcount())>0) {
								response.write(&buffer[0], read_length);
								response.flush();
							}
						}
						catch (const exception &e) {
							cerr << "Connection interrupted, closing file" << endl;
						}

						ifs.close();
						return;
					}
				}
			}
		}
	}
	string content = "Could not open path " + request->path;
	response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
}

int main() {
	// TODO: make better version of variant, which can do unsafe access for more convenient function flow.
	// or just make wrapper around variant, which defines SwitchCase methods. Basically want to take advantage of argument type deduction.
	Database::Failable<Database> fDb = Database::Create();
	int mainRet = SwitchCase<int, Database::Error, Database>(
		fDb,
		[](Database::Error& error)
		{
			cerr << error.m_message << endl;
			return 1; // returned by main. 1 indicates an error.
		},
		[](Database& db)
		{
			//HTTP-server at port 8080 using 4 threads
			s_db = &db;
			HttpServer server(8080, 4);

			//Add resources using path-regex and method-string, and a function
			server.resource["^/string$"]["POST"] = StringPost;
			server.resource["^/json$"]["POST"] = JsonPost;
			server.resource["^/info$"]["GET"] = InfoGet;
			server.resource["^/match/([0-9]+)$"]["GET"] = MatchGet;
			server.resource["^/db/([0-9]+)$"]["GET"] = DbGet;
			server.resource["^/db/([0-9]+)$"]["PUT"] = DbPut;
			server.default_resource["GET"] = WebGet;

			thread server_thread([&server]() {
				//Start server
				server.start();
			});

			//Wait for server to start so that the client can connect
			this_thread::sleep_for(chrono::seconds(1));

			server_thread.join();
			return 0;
		}
	);
    
	return mainRet;
}


//Client examples
/*HttpClient client("localhost:8080");
auto r1 = client.request("GET", "/match/123");
cout << r1->content.rdbuf() << endl;

string json = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
stringstream ss(json);
auto r2 = client.request("POST", "/string", ss);
cout << r2->content.rdbuf() << endl;

ss.str(json);
auto r3 = client.request("POST", "/json", ss);
cout << r3->content.rdbuf() << endl;*/