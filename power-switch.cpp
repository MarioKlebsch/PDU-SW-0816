/*
   power_switch: control Argus PDU SW-0816
   Copyright (C) 2025  Mario Klebsch, DG1AM
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/system/error_code.hpp>

#include "case_insensitive.h"
#include "http_status_error_category.h"
#include "rapidxml.hpp"

#ifdef _WIN32
#include "WindowsService.h"
#endif

static const char *license_info =
"Copyright (C) 2025 Mario Klebsch, DG1AM\n"
"License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n";


// return a sef of all channels
static auto all_channels()
{
    std::set<channel> ret;
    for (const auto &item:map_channel_name_to_index)
        ret.insert(item.second);
    return ret;
}


// Parse a channel list. Example: "153" is parsed to a list of ch1, ch3 and ch5
// Channel list is added to existing channels
// returns false on invalid input.
static bool parse_channel_list(std::string_view list, std::set<channel> &channels)
{
    for (const auto ch:list)
    {
        if (ch >='1' && ch <= '8')
            channels.insert(channel(ch-'1'));
        else
            return false;
    }
    return true;
}

static auto parse_channels(int argc, const char *argv[])
{
    std::set<channel> ret;
    while (argc--)
    {
        auto arg = *argv++;
        auto it = map_channel_name_to_index.find(arg);
        if (it != map_channel_name_to_index.end())
            ret.insert(it->second);
        else if (iequals(arg, "all"))
            ret = all_channels();
        else if (!parse_channel_list(arg, ret))
            std::cerr << "unknown channel " << arg << "\n";
    }
    return ret;
}

// invert std::map:
// Create a new map with data of input as keys
// and keys of input as data.
template<typename T1, typename T2, typename Cmp>
static auto invert_map(const std::map<T1, T2, Cmp>&map)
{
    std::map<T2,T1> ret;
    for (const auto &item:map)
        ret[item.second] = item.first;
    return ret;
}

std::ostream& operator<<(std::ostream &s, channel channel)
{
    static const auto map_channel_index_to_name=invert_map(map_channel_name_to_index);
    
    auto it = map_channel_index_to_name.find(channel);
    if (it != map_channel_index_to_name.end())
        s << it->second;
    return s;
}

std::ostream& operator<<(std::ostream &s, const std::set<channel>&channels)
{
    bool first=true;
    for (const auto ch:channels)
    {
        if (first)
            first=false;
        else
            s << ", ";
        s << ch;
    }
    return s;
}

std::string to_string(const channel channel)
{
    std::ostringstream os;
    os << channel;
    return os.str();
}

std::string to_string(const std::set<channel>&channels)
{
    std::ostringstream os;
    os << channels;
    return os.str();
}

std::string to_string(op_t op)
{
    switch(op)
    {
        case on:  return "on";
        case off: return "off";
        default:  return "<unknown>";
    }
}

std::string base64_encode(const std::string &s)
{
    using namespace boost::archive::iterators;
    using base64_iterator = base64_from_binary<transform_width<const char *, 6, 8 >>;
    
    std::ostringstream os;
    
    std::copy(base64_iterator(s.c_str()),
              base64_iterator(s.c_str()+s.length()),
              ostream_iterator<char>(os));
    // add terminator
    return os.str()+"=";
}

using namespace std::string_literals;
namespace http = boost::beast::http;


// syncronous http transaction
http::response<http::string_body>
http_transaction(http::request<http::string_body> &&request,
                 http::status                       expected_status,
                 boost::system::error_code         &ec)
{
    boost::asio::io_context io_context;

    const auto resolved=boost::asio::ip::tcp::resolver{io_context}.resolve(addr, port,  ec);
    if (ec)
        return {};

    auto it = resolved.begin();
    while (it != resolved.end() && ! it->endpoint().address().is_v4())
        it++;

    //    std::cout << "connecting " << it->endpoint().address().to_string() << "\n";
    boost::beast::tcp_stream s{io_context};
    s.connect(*it, ec);
    if (ec)
        return {};

    namespace http = boost::beast::http;

    request.set(http::field::host, addr);
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.set(http::field::authorization, "Basic "s + base64_encode(user + ":" + password) );
    http::write(s, request, ec);
    if (ec)
        return {};

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(s, buffer, response, ec);
    if (ec)
        return {};

    s.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    ec={};

    if (response.result() != expected_status)
    {
        ec = make_error_code(response.result());
        return {};
    }

    return response;
}

inline http::response<http::string_body>
http_transaction(http::request<http::string_body> &&request, boost::system::error_code &ec)
{
    return http_transaction(std::move(request), http::status::ok, ec);
}


// asyncronous http transaction
template<typename CB>
void async_http_transaction(boost::asio::io_context           &io_context,
                            http::request<http::string_body> &&request,
                            http::status                       expected_status,
                            const CB                          &cb)
{

    class http_op: public std::enable_shared_from_this<http_op>
    {
        using std::enable_shared_from_this<http_op>::shared_from_this;
        
        boost::asio::ip::tcp::resolver    resolver;
        boost::beast::tcp_stream          s;
        http::request<http::string_body>  request;
        CB                                cb;
        boost::beast::flat_buffer         buffer;
        http::response<http::string_body> response;
        http::status                      expected_status;

        http_op()=delete;
        http_op(const http_op&)=delete;
        http_op&operator=(const http_op&)=delete;

    public:
        http_op(boost::asio::io_context &io_context, http::request<http::string_body> &&request, http::status expected_status, const CB &cb):
            resolver{io_context},
            s{io_context},
            request{std::move(request)},
            expected_status{expected_status},
            cb{cb}
        {
            this->request.set(http::field::host, addr);
            this->request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            this->request.set(http::field::authorization, "Basic "s + base64_encode(user + ":" + password) );
        }

        void start()
        {
            resolver.async_resolve(addr, port, [This=shared_from_this()](auto ec, auto it) {
                if (ec)
                {
                    This->cb(ec, This->response);
                    return;
                }
                const decltype(it) end;
                while (it != end && ! it->endpoint().address().is_v4())
                    it++;

                This->connect(it);
            });
        }

    private:
        void connect(const boost::asio::ip::tcp::resolver::iterator& it)
        {
//            std::cout << "connecting " << it->endpoint().address().to_string() << "\n";
            s.async_connect(*it, [This=shared_from_this()](auto ec) {
                if (ec)
                {
                    This->cb(ec, This->response);
                    return;
                }
                This->send();
            });
        }

        void send()
        {
            http::async_write(s, request, [This=shared_from_this()](auto ec, auto bytes_written) {
                if (ec)
                {
                    This->cb(ec, This->response);
                    return;
                }
                This->receive();
            } );
            
        }

        void receive()
        {
            http::async_read(s, buffer, response, [This=shared_from_this()](auto ec, auto bytes_written) {
                if (ec)
                    return This->cb(ec, This->response);

                boost::system::error_code e;
                This->s.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);
                if (This->response.result() != This->expected_status)
                    This->cb(make_error_code(This->response.result()), This->response);

                This->cb(boost::system::error_code{}, This->response);
            });
        }
    };

    std::make_shared<http_op>(io_context, std::move(request), expected_status, cb)->start();
}

template<typename CB>
inline void async_http_transaction(boost::asio::io_context &io_context, http::request<http::string_body> &&request, const CB &cb)
{
    return async_http_transaction(io_context, std::move(request), http::status::ok, cb);
}

//power switch request
static inline http::request<http::string_body> swith_request(const std::set<channel> &channels, op_t op)
{
    std::ostringstream os;
    os << "/control_outlet.htm?";
    for (auto ch:channels)
        os << "outlet" << int(ch) << "=1&";
    os << "op=" << int(op);
    return {http::verb::get, os.str(), 11};
}

// status request
static inline http::request<http::string_body> status_request()
{
    return {http::verb::get, "/status.xml", 11};
};

struct channel_status
{
    channel          channel;
    std::string_view name;
    bool             state;
};

// status response
static std::list<const channel_status> parse_status_response(const http::response<http::string_body> &response)
{
    rapidxml::xml_document<> doc;
    doc.parse<0>(response.body());

    std::list<const channel_status> ret;
    
    const auto& root = doc.first_node().value();
    std::ostringstream os;

    for (int ch = 0; ch < 8; ch++)
    {
        auto n = root.first_node("outletStat"s + char('0' + ch));
        if (!n.has_value())
            continue;

        static const auto map_channel_index_to_name = invert_map(map_channel_name_to_index);

        auto it = map_channel_index_to_name.find(channel(ch));
        if (it == map_channel_index_to_name.end())
            continue;
        ret.emplace_back(channel(ch), it->second, iequals(n.value().value(), "on"));
    }
    return ret;
}

template<typename S>
static S strip_path_element(S &path)
{
    static constexpr char delimiter='/';
    std::size_t n;
    while ((n = path.find(delimiter))==0)
        path = path.substr(1);
    
    S ret;
    if (n == std::string::npos)
    {
        ret = path;
        path={};
    }
    else
    {
        ret = path.substr(0, n);
        path = path.substr(n+1);
        while ((n = path.find(delimiter))==0)
            path = path.substr(1);
    }
    return ret;
}

#ifdef PROXY_BIND_PORT
namespace http = boost::beast::http;
using namespace std::string_literals;
using tcp = boost::asio::ip::tcp;
class proxy_server
{

    class session : public std::enable_shared_from_this<session>
    {
        boost::asio::io_context& io_context;
        boost::beast::tcp_stream         s;
        boost::beast::flat_buffer        buffer;
        http::request<http::string_body> request;
        boost::asio::deadline_timer      timer;

    public:
        explicit session(boost::asio::io_context& io_context, tcp::socket&& s) :
            io_context{ io_context },
            s{ std::move(s) },
            timer{ io_context }
        {
        }
        session() = delete;
        session(const session&) = delete;
        session& operator=(const session&) = delete;

        void close()
        {
            boost::system::error_code e;
            s.socket().shutdown(tcp::socket::shutdown_send, e);
            s.socket().close(e);
        }

        void start()
        {
            request = {};
            boost::beast::http::async_read(s, buffer, request,
                [This = shared_from_this()](auto ec, auto bytes_transferred) {
                    if (ec)
                    {
                        if (ec == http::error::end_of_stream)
                            This->close();
                        else if (ec != boost::asio::error::operation_aborted)
                            std::cerr << "read() failed: " << ec.message() << "\n";
                        return;
                    }

                    This->process_request();
                });
        }

        void send_response(http::status status, std::string_view content_type, std::string_view msg)
        {
            http::response<http::string_body> response{ status, request.version() };
            response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            response.set(http::field::content_type, content_type);
            response.body() = std::string(msg);
            response.prepare_payload();

            boost::beast::async_write(s, http::message_generator{ std::move(response) },
                [This = shared_from_this()](const auto& ec, auto bytes_transferred) {
                    if (ec)
                    {
                        if (ec != boost::asio::error::operation_aborted)
                            std::cerr << "beast::async_write() failed: " << ec.message() << "\n";
                        return;
                    }
                    This->close();
                });
        }

        void bad_request(std::string_view why)
        {
            send_response(http::status::bad_request, "text/plain", why);
        }

        void not_found()
        {
            send_response(http::status::not_found, "text/plain", "not found");
        }

        void internal_server_error(std::string_view operation, const boost::system::error_code& ec = {})
        {
            std::ostringstream os;
            os << "<html><head><title>internal server error</title></head>"
                << "<body><h1>internal server error</h1><p>"
                << operation;
            if (ec)
                os << " failed: " << ec.message();
            os << "</p></body></html>";
            send_response(http::status::internal_server_error, "text/html", os.str());
        }

        void root_document()
        {
            async_http_transaction(io_context, status_request(), [This = shared_from_this()](auto ec, auto response) {
                if (ec)
                    return This->internal_server_error("http-transaction status", ec);

                try
                {
                    auto switch_states = parse_status_response(response);
                    std::ostringstream os;
                    os << R"---(
<html>
    <head>
        <title>power switch</title>
        <style>
.on {
  background-color: Chartreuse;
}
.off {
}
.state {
  text-align: center;
}
table, th, td {
  border: 1px solid;
  border-collapse: collapse;
}
#overlay.dim {
  display:inline;
}
#overlay {
  background-color: rgba(0,0,0,0.2);
  display:none;
  position:fixed;
  left:0;
  top: 0;
  width:100%;
  height:100%;
}
        </style>
        <script>

function set_switch(request)
{
  // din window when operation is in progress
  document.getElementById('overlay').classList.add('dim');

  const xhr = new XMLHttpRequest();
  xhr.open("GET", "/" + request, true);
  xhr.onload = (e) => {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
//        console.log(xhr.responseText);
      } else {
        console.error(xhr.statusText);
      }
      location.reload();
    }
  };
  xhr.onerror = (e) => {
    console.error(xhr.statusText);
    location.reload();
  };
  xhr.send(null);
}

        </script>
    </head>
    <body>
        <h1>power switch</h1>
        <h2>Scenes:</h2>
        <ul>
)---";
                    for (const auto &scene: scenes)
                        os << "<li><button onclick='set_switch(\"set/" << scene.first <<"\")'>" << scene.first << "</button></li>\n";
                    os << R"---(
        </ul>

        <h2>Channels:</h2>
        <table>
            <tr><th>channel</th><th>state</th><th colspan='2'>command</th></tr>
)---";
                    for(const auto &state:switch_states)
                    {
                        os << "<tr class='" << state.name << "'>";
                        os << "<td class='channel'>" << state.name << "</td>";
                        os << "<td class='state " << (state.state ? "on" : "off") << "'>" << (state.state ? "on" : "off") << "</td>";
                        
                        os << "<td class='off_button'><button onclick='set_switch(\"" << state.name <<"?off\")'>off</button></td>";
                        os << "<td class='on_button'><button onclick='set_switch(\"" << state.name <<"?on\")'>on</button></td>";
                        
                        os << "</tr>\n";
                    }
                    os << R"---(
        </table>
        <div id='overlay'/>
    </body>
</html>
)---";

                    return This->send_response(http::status::ok, "text/html", os.str());
                }
                catch (const std::exception& ex)
                {
                    return This->internal_server_error("xml parsing failed: "s + ex.what(), ec);
                }
                });
        }

        void show()
        {
            async_http_transaction(io_context, status_request(), [This = shared_from_this()](auto ec, auto response) {
                if (ec)
                    return This->internal_server_error("http-transaction status", ec);

                try
                {
                    auto switch_states = parse_status_response(response);
                    std::ostringstream os;
                    for(const auto &state:switch_states)
                        os << state.name << ": " << (state.state ? "on"s : "off"s) << "\n";

                    return This->send_response(http::status::ok, "text/plain", os.str());
                }
                catch (const std::exception& ex)
                {
                    return This->internal_server_error("xml parsing failed: "s + ex.what(), ec);
                }
                });
        }

        void power_cycle(const std::set<channel>& channels, std::chrono::milliseconds delay)
        {
            async_http_transaction(io_context, swith_request(channels, off),
                [This = shared_from_this(), channels, delay](auto ec, auto response) {
                    if (ec)
                        return This->internal_server_error("http-transaction off", ec);

                    This->timer.expires_from_now(boost::posix_time::milliseconds(delay.count()));
                    This->timer.async_wait([This, channels](auto ec) {
                        if (ec)
                            return This->internal_server_error("wait", ec);

                        async_http_transaction(This->io_context, swith_request(channels, on),
                            [This, channels](auto ec, auto response) {
                                if (ec)
                                    return This->internal_server_error("http-transaction on", ec);

                                This->send_response(http::status::ok, "text/plain", to_string(channels) + ": power cycled");
                            });
                        });
                });
        }

        void set_channels(const std::set<channel>& channels, op_t op)
        {
            async_http_transaction(io_context, swith_request(channels, op),
                [This = shared_from_this(), channels, op](auto ec, auto response) {
                    if (ec)
                        return This->internal_server_error("http-transaction", ec);
                    return This->send_response(http::status::ok, "text/plain", to_string(channels) + ": " + to_string(op));
                });
        }

        void set_channels(const std::set<channel>& channels, std::string_view query)
        {
            if (iequals(query, "on"))
                set_channels(channels, on);
            else if (iequals(query, "off"))
                set_channels(channels, off);
            else if (iequals(query, "cycle"))
                power_cycle(channels, std::chrono::seconds(5));
            else
                return bad_request("request error: illegal request");
        }

        void set_scene(std::string_view name)
        {
            auto it = scenes.find(name);
            if (it == scenes.end())
                return not_found();
            const auto& scene = it->second;

            auto turn_on = [This = shared_from_this(), &scene]() {
                if (scene.on.empty())
                    return This->send_response(http::status::ok, "text/plain", "Ok");
                async_http_transaction(This->io_context, swith_request(scene.on, on), [This](auto ec, auto& response) {
                    if (ec)
                        return This->internal_server_error("http-transaction on", ec);
                    This->send_response(http::status::ok, "text/plain", "Ok");
                    });
                };

            if (scene.off.empty())
                return turn_on();

            async_http_transaction(io_context, swith_request(scene.off, off),
                [This = shared_from_this(), turn_on = std::move(turn_on)](auto ec, auto& response) {
                    if (ec)
                        return This->internal_server_error("http-transaction off", ec);
                    turn_on();
                });
        }

        void process_request()
        {
            if (request.method() != http::verb::get)
            {
                //todo
                std::cerr << "bad method\n";
                return;
            }

            const std::string_view target{ request.target().data(), request.target().size() };
            auto n = request.target().find('?');
            auto query = n == std::string::npos ? "" : request.target().substr(n + 1);
            auto path = target.substr(0, n);

            if (path.empty())
                return bad_request("request error: path is empty");

            if (path[0] != '/')
                return bad_request("request error: path is not absolute");
            path = path.substr(1); // strip off leading '/';

            if (path == "")
                return root_document();
            else if (iequals(path, "show"))
                return show();
            else if (iequals(path, "all"))
                return set_channels(all_channels(), query);

            auto it = map_channel_name_to_index.find(path);
            if (it != map_channel_name_to_index.end())
                return set_channels({ it->second }, query);

            auto root = strip_path_element(path);
            if (iequals(root, "set"))
                return set_scene(path);

            return not_found();
        }

    };

    boost::asio::io_context &io_context;
    tcp::acceptor acceptor{ io_context };

    void accept()
    {
        acceptor.async_accept([this](auto ec, auto&& socket) {
            if (ec)
            {
                if (ec != boost::asio::error::operation_aborted)
                    std::cerr << "accept() failed: " << ec.message() << "\n";
                return;
            }
            std::make_shared<session>(io_context, std::move(socket))->start();
            accept();
        });
    }

public:
    proxy_server(boost::asio::io_context &io_context):io_context{ io_context }{}

    int start()
    {
        tcp::endpoint ep{
#ifdef PROXY_BIND_ADDR
            boost::asio::ip::make_address(PROXY_BIND_ADDR),
#else
            boost::asio::ip::tcp::v4(),
#endif
            PROXY_BIND_PORT };
        boost::system::error_code ec;

        acceptor.open(ep.protocol(), ec);
        if (ec)
        {
            std::cerr << "open() failed: " << ec.message() << "\n";
            return -1;
        }

        if (ep.address().is_v6())
        {
            acceptor.set_option(boost::asio::ip::v6_only{ false }, ec);
            if (ec)
                std::cerr << "set_option(v6_only, true) failed: " << ec.message() << "\n";
        }

        acceptor.set_option(tcp::acceptor::reuse_address{ true }, ec);
        if (ec)
            std::cerr << "set_option(reuse_address, true) failed: " << ec.message() << "\n";

        acceptor.bind(ep, ec);
        if (ec)
        {
            std::cerr << "bind(" << ep.address().to_string() << ", " << ep.port() << ") failed: "
                << ec.message() << "\n";
            return -1;
        }

        acceptor.listen(tcp::acceptor::max_connections, ec);
        if (ec)
        {
            std::cerr << "listen() failed: " << ec.message() << "\n";
            return -1;
        }

        accept();
        return 0;
    }

    void stop()
    {
        boost::system::error_code ec;
        acceptor.close(ec);
    }

};

#endif /* PROXY_PORT */

int set_switch(const std::set<channel> &channels, op_t op)
{
    boost::system::error_code ec;
    auto response = http_transaction(swith_request(channels, op), ec);
    if (ec)
    {
        std::cerr << "GET /control_outlet.htm failed: " << ec.message() << "\n";
        return -1;
    }

    return 0;
}

int set_scene(int argc, const char *argv[])
{
    for (int i=0; i<argc; i++)
    {
        auto scene = argv[i];
        auto it = scenes.find(scene);
        if (it == scenes.end())
        {
            std::cerr << "unknown scene: " << scene << "\n";
            return -1;
        }
        int ret;
        if (!it->second.off.empty() && (ret=set_switch(it->second.off, off)))
                return ret;
        if (!it->second.on.empty() && (ret=set_switch(it->second.on, on)))
            return ret;
    }
    return 0;
}

int show(const std::set<channel> &channels)
{
    boost::system::error_code ec;
    auto response = http_transaction(status_request(), ec);
    if (ec)
    {
        std::cerr << "GET /status.xml failed: " << ec.message() << "\n";
        return -1;
    }
    try
    {
        auto switch_states = parse_status_response(response);
        for(const auto &state:switch_states)
            std::cout << state.name << ": " << (state.state ? "on"s : "off"s) << "\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "XML parsing failed: " << ex.what() << "\n";
        return -1;
    }
}

int show_channels()
{
    std::cout << "Available channels:\n";
    for (const auto &item:map_channel_name_to_index)
        std::cout << "- " << item.first << "\n";
    std::cout << "- all\n";
    return 0;
}

int show_scenes()
{
    std::cout << "Available scenes:\n";
    for (const auto &item:scenes)
    {
        std::cout << "- " << item.first;
        if (!item.second.off.empty())
        {
            std::cout << " off:";
            for (auto ch:item.second.off)
                std::cout << " " << ch;
        }
        if (!item.second.on.empty())
        {
            std::cout << " on:";
            for (auto ch:item.second.on)
                std::cout << " " << ch;
        }
        std::cout << "\n";
    }
    return 0;
}


#ifdef PROXY_BIND_PORT
#ifdef _WIN32
class PowerSwitchService : public windows::service
{
    boost::asio::io_context io_context;
    proxy_server            proxy{ io_context };

    std::string name()         const override { return "PowerSwitchProxy"; };
    std::string display_name() const override { return "PDU SW-0816 power switch proxy service"; }
    std::string arguments()    const override { return "service"; };

    int init() override
    {
        return proxy.start();
    }

    int mainloop() override
    {
        io_context.run();
        return 0;
    }

    void stop() override
    {
        proxy.stop();
    }

public:
    PowerSwitchService() = default;
};

#endif /* _WIN32 */
#endif /* PROXY_PORT */

int usage(const char* name)
{
    auto p = strrchr(name, '/');
    if (p)
        name = p + 1;
    p = strrchr(name, '\\');
    if (p)
        name = p + 1;

    std::cerr << "usage:\n";
    std::cerr << "    " << name << " on    <channel>...  : turn on channel(s)\n";
    std::cerr << "    " << name << " off   <channel>...  : turn off channel(s)\n";
    std::cerr << "    " << name << " cycle <channel>...  : power cycle channel(s), 5s off\n";
    std::cerr << "    " << name << " set   <scene>...    : turn off/on accorting to scene(s)\n";
    std::cerr << "    " << name << " show [<channel>...] : show current switch state of channel(s)\n";
    std::cerr << "    " << name << " info                : show software info\n";
#ifdef PROXY_BIND_PORT
#ifdef PROXY_BIND_ADDR
    std::cerr << "    " << name << " proxy               : proxy server on " << PROXY_BIND_ADDR << " port " << PROXY_BIND_PORT << "\n";
#else
    std::cerr << "    " << name << " proxy               : proxy server on port " << PROXY_BIND_PORT << "\n";
#endif
#ifdef _WIN32
    PowerSwitchService{}.handle_command("", std::string{ name } + " service");
#endif /* _WIN32 */
#endif /* PROXY_BIND_PORT */
    std::cerr << "\n" << license_info;
    return -1;
}

int main(int argc, const char * argv[])
{
    if (argc < 2)
        return usage(argv[0]);
    auto cmd = argv[1];

#ifdef _WIN32
    if (iequals(cmd, "service"))
    {
        if (argc < 3)
            return usage(argv[0]);

        return PowerSwitchService{}.handle_command(argv[2], std::string{ argv[0] } + " " + cmd );
    }
#endif /* _WIN32 */
    if (iequals(cmd, "on"))
    {
        if (argc == 2)
            return show_channels();
        return set_switch(parse_channels(argc - 2, argv+2), on);
    }
    else if (iequals(cmd, "off"))
    {
        if (argc == 2)
            return show_channels();
        return set_switch(parse_channels(argc - 2, argv+2), off);
    }
    if (iequals(cmd, "cycle"))
    {
        if (argc == 2)
            return show_channels();
        auto channels = parse_channels(argc - 2, argv+2);
        auto ret = set_switch(channels, off);
        if (ret)
            return ret;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return set_switch(channels, on);
    }
    else if (iequals(cmd, "set"))
    {
        if (argc == 2)
            return show_scenes();
        return set_scene(argc-2, argv+2);
    }
    else if (iequals(cmd, "show"))
    {
        if (argc==2)
            return show(all_channels());
        return show(parse_channels(argc - 2, argv+2));
    }
#ifdef PROXY_BIND_PORT
    else if (iequals(cmd, "proxy"))
    {
        if (argc!=2)
            return usage(argv[0]);

        boost::asio::io_context io_context;
        proxy_server proxy{ io_context };
        auto ret = proxy.start();
        if (ret)
            return ret;
        io_context.run();
        return 0;
    }
#endif /* PROXY_PORT */
    else if (iequals(cmd, "info"))
    {
        std::cout << "control Argus PDU SW-0816\n";
        std::cout << "address: " << addr << "\n";
        std::cout << "user: " << user << "\n";
#ifdef PROXY_BIND_PORT
#ifdef PROXY_BIND_ADDR
        std::cout << "proxy: " << PROXY_BIND_ADDR << " port " << PROXY_BIND_PORT << "\n";
#else
        std::cout << "proxy: port " << PROXY_BIND_PORT << "\n";
#endif
#endif
        std::cout << "\n";
        std::cout << license_info;
        return 0;
    }
    else
        return usage(argv[0]);

    return 0;
}
