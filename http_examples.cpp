#include "server_http.hpp"
#include "client_http.hpp"
#include "Database.h"
#include "Choice.h"
#include "Async.h"
#include "SharedPtrMonad.h"
#include "Curry.h"
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

// TODO: delete this.
template <typename T>
void ResponseWithCode(HttpServer::Response& response, HttpCode code, const T& thing)
{
    stringstream thingStream;
    thingStream << thing;
    response << "HTTP/1.1 " << code << "\r\nContent-Length: " << thingStream.tellp() << "\r\n\r\n" << thingStream.rdbuf();
}

template <typename T>
std::string FormatResponseWithCode(HttpCode code, const T& thing)
{
    stringstream thingStream;
    thingStream << thing;
    stringstream stream;
    stream << "HTTP/1.1 " << code << "\r\nContent-Length: " << thingStream.tellp() << "\r\n\r\n" << thingStream.rdbuf();
    return stream.str();
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

typedef std::string Response;
Async<Response> DbGet(shared_ptr<HttpServer::Request> request) {
    string number = request->path_match[1];
    return FunctorTransformm<Failable<Database::Name>, Response>(
        s_db->GetName(number), 
        [](Failable<Database::Name> result)
        {
            if (result.IsFailure())
            {
                return FormatResponseWithCode(HttpCode::BAD_REQUEST, result.GetFailure().m_message);
            }
            return FormatResponseWithCode(HttpCode::OK, result.GetSuccess());
        }
    );
}

Async<Response> DbPut(shared_ptr<HttpServer::Request> request) {
    string id = request->path_match[1];
    stringstream nameStream;
    nameStream << request->content.rdbuf();
    return FunctorTransformm<Error, Response>(
        s_db->ReplaceName(id, nameStream.str()), 
        [](Error error)
        {
            if (error.m_message.empty())
            {
                return FormatResponseWithCode(HttpCode::OK, "Success");
            }
            return FormatResponseWithCode(HttpCode::BAD_REQUEST, error.m_message);
        }
    );    
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
void DoThing(const string& str)
{
    cout << str << endl;
}
void DoThingStr(string& str)
{
    str = str + " life";
}
void DoThingInt(int& i)
{
    i++;
}
void printThreeStrs(std::string a, std::string b, std::string c)
{
    std::cout << a << b << c << endl;
}
int main() {
    /*auto sharedPoop = std::make_shared<string>("poop");
    std::shared_ptr<string> sharedEmpty;
    std::function<size_t(string)> LengthFunc = [](string str) { return str.length(); };
    std::shared_ptr<size_t> sharedPoopLen = FunctorTransform(LengthFunc, sharedPoop);
    std::shared_ptr<size_t> sharedEmptyLen = FunctorTransform(LengthFunc, sharedEmpty);

    std::function<int(int,int,int)> add = [](int a, int b, int c) {
        return a + b + c;
    };
    auto curriedAdd = curry(add);
    cout << curriedAdd(1)(5)(6) << endl;
    curry(printThreeStrs)("Hello ")("functional ")("world!");

    auto aa = MakeAsync<int>(5);
    auto ab = MakeAsync<int>(29);
    auto ac = MakeAsync<int>(73);
    Async<int> aSum = FunctorTransform3(add, aa, ab, ac);
    aSum([](int aSum)
    {
        cout << "Sum is: " << aSum << endl;
    });

    std::shared_ptr<int> a = std::make_shared<int>(1);
    std::shared_ptr<int> b = std::make_shared<int>(5);
    std::shared_ptr<int> c = std::make_shared<int>(6);
    std::shared_ptr<int> sum = FunctorApply(FunctorApply(FunctorTransform(curriedAdd, a), b), c);
    std::shared_ptr<int> sum2 = FunctorTransform3(add, a, b, c);

    std::function<std::shared_ptr<int>(int)> squareRoot = [](int x) {
        if (x >= 0)
        {
            return std::make_shared<int>(sqrt(x));
        }
        return std::shared_ptr<int>();
    };
    std::function<std::shared_ptr<int>(int)> fiveDiv = [](int x) {
        if (x != 0)
        {
            return std::make_shared<int>(-25 / x);
        }
        return std::shared_ptr<int>();
    };
    std::shared_ptr<int> result = FunctorBind(std::make_shared<int>(25), squareRoot);
    std::shared_ptr<int> result2 = squareRoot(25);
    std::shared_ptr<int> result3 = FunctorBind(squareRoot(25), fiveDiv); //-5
    std::shared_ptr<int> result4 = FunctorBind(squareRoot(-2), fiveDiv); //empty
    std::shared_ptr<int> result5 = FunctorBind(squareRoot(-10), fiveDiv); //empty
    std::shared_ptr<int> result6 = FunctorBind(squareRoot(0), fiveDiv); //empty
    std::shared_ptr<int> result7 = FunctorBind(squareRoot(1), fiveDiv); //-25
    std::shared_ptr<int> result8 = FunctorBind(fiveDiv(2), squareRoot); //empty
    std::shared_ptr<int> result9 = FunctorBind(fiveDiv(-2), squareRoot); // 3
    std::shared_ptr<int> result10 = fiveDiv(-2) >= squareRoot; // 3
    cout << "done, son." << endl;*/
    //std::shared_ptr<int> sum2 = FunctorBind(c, FunctorBind(b, FunctorBind(a, curriedAdd)));
    /*std::function<void(const string&)> constHandler = DoThing;
    std::function<void(string&)> handler = DoThing;
    string str("hi");
    const string& strCR = str;
    string& strR = str;
    constHandler(strCR);
    constHandler(strR);
    //handler(strCR);
    handler(strR);

    Choice<int, string> choice("hello"s);
    Choice<int, string> choice2(302);
    int len = choice2.SwitchCaseConst<int>([](const int& i) {
        return i;
    }, [](const string& str) {
        return str.length();
    });

    choice.SwitchCase<void>(DoThingInt, DoThing);*/

    /*Failable<int> fi(1);
    fi.SwitchCase<void>([](Error& err)
    {
        err.m_code++;
    },
        [](int& i)
    {
        i--;
    });
    cout << fi.GetVariant() << endl;*/
    Failable<Database> fDb = Database::Create();
    int mainRet = fDb.SwitchCase<int>(
        [](Error& error)
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
            //server.resource["^/string$"]["POST"] = StringPost;
            //server.resource["^/json$"]["POST"] = JsonPost;
            //server.resource["^/info$"]["GET"] = InfoGet;
            //server.resource["^/match/([0-9]+)$"]["GET"] = MatchGet;
            server.resource["^/db/([0-9]+)$"]["GET"] = DbGet;
            server.resource["^/db/([0-9]+)$"]["PUT"] = DbPut;
            //server.default_resource["GET"] = WebGet;

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
    //return 0;
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