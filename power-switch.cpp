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
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/system/error_code.hpp>

#include "case_insensitive.h"
#include "http_status_error_category.h"
#include "rapidxml.hpp"

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
static bool parse_channel_list(const char *list, std::set<channel> &channels)
{
    char ch;
    while ((ch=*list++))
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

using namespace std::string_literals;

static std::string http_get(const std::string &path, boost::system::error_code &ec)
{
    boost::asio::io_context io_context;

    const auto resolved=boost::asio::ip::tcp::resolver{io_context}.resolve(addr, "80", ec);
    if (ec)
        return {};

    boost::beast::tcp_stream s{io_context};
    
    s.connect(resolved, ec);
    if (ec)
        return {};
    
    namespace http = boost::beast::http;

    http::request<http::string_body> req{http::verb::get, path, 11};
    req.set(http::field::host, addr);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    req.set(http::field::authorization, "Basic "s + base64_encode(user + ":" + password) );
    http::write(s, req, ec);
    if (ec)
        return {};

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(s, buffer, response, ec);
    if (ec)
        return {};
    if ((ec=response.result()) != http::status::ok)
        return {};

    s.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    ec={};
    
    return response.body();
}

static std::string http_get(const std::string &path, const std::string &query, boost::system::error_code &ec)
{
    return http_get(path + "?" + query, ec);
}

int set_switch(op_t op, const std::set<channel> &channels)
{
    std::ostringstream os;
    for (auto ch:channels)
        os << "outlet" << int(ch) << "=1&";
    os << "op=" << int(op);

    boost::system::error_code ec;
    auto response = http_get("/control_outlet.htm", os.str(), ec);
    if (ec)
    {
        std::cerr << "GET /control_outlet.htm failed: " << ec.message() << "\n";
        return -1;
    }
    return 0;
}

int set_szene(int argc, const char *argv[])
{
    for (int i=0; i<argc; i++)
    {
        auto szene = argv[i];
        auto it = szenes.find(szene);
        if (it == szenes.end())
        {
            std::cerr << "unknown szene: " << szene << "\n";
            return -1;
        }
        int ret;
        if (!it->second.off.empty() && (ret=set_switch(off, it->second.off)))
                return ret;
        if (!it->second.on.empty() && (ret=set_switch(on, it->second.on)))
            return ret;
    }
    return 0;
}

int show(const std::set<channel> &channels)
{
    boost::system::error_code ec;
    auto response = http_get("/status.xml", ec);
    if (ec)
    {
        std::cerr << "GET /status.xml failed: " << ec.message() << "\n";
        return -1;
    }

    rapidxml::xml_document<> doc;
    try
    {
        doc.parse<0>(response);
    }
    catch (const std::exception&ex)
    {
        std::cerr << "XML parsing failed: " << ex.what() << "\n";
        return -1;
    }
    
    const auto &root = doc.first_node().value();
    for (auto ch:channels)
    {
        auto n = root.first_node("outletStat"s + char('0'+int(ch)));
        if (!n.has_value())
            continue;

        static const auto map_channel_index_to_name=invert_map(map_channel_name_to_index);

        auto it = map_channel_index_to_name.find(ch);
        if (it == map_channel_index_to_name.end())
            continue;
        std::cout << it->second << ": " << n.value().value() << "\n";
    }
    return 0;
}

int show_channels()
{
    std::cout << "Available channels:\n";
    for (const auto &item:map_channel_name_to_index)
        std::cout << "- " << item.first << "\n";
    std::cout << "- all\n";
    return 0;
}

int show_szenes()
{
    std::cout << "Available szenes:\n";
    for (const auto &item:szenes)
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

int usage(const char *name)
{
    auto p=strrchr(name, '/');
    if (p)
        name=p+1;
    p=strrchr(name, '\\');
    if (p)
        name=p+1;

    std::cerr << "usage:\n";
    std::cerr << "    " << name << " on    <channel>...  : turn on channel(s)\n";
    std::cerr << "    " << name << " off   <channel>...  : turn off channel(s)\n";
    std::cerr << "    " << name << " cycle <channel>...  : power cycle channel(s), 5s off\n";
    std::cerr << "    " << name << " set   <szene>...    : turn off/on accorting to szene(s)\n";
    std::cerr << "    " << name << " show [<channel>...] : show current switch state of channel(s)\n";
    std::cerr << "    " << name << " info                : show software info\n";

    std::cerr << "\n" << license_info;
    return -1;
}

int main(int argc, const char * argv[])
{
    if (argc < 2)
        return usage(argv[0]);
    auto cmd=argv[1];

    if (iequals(cmd, "on"))
    {
        if (argc == 2)
            return show_channels();
        return set_switch(on, parse_channels(argc - 2, argv+2));
    }
    else if (iequals(cmd, "off"))
    {
        if (argc == 2)
            return show_channels();
        return set_switch(off, parse_channels(argc - 2, argv+2));
    }
    if (iequals(cmd, "cycle"))
    {
        if (argc == 2)
            return show_channels();
        auto channels = parse_channels(argc - 2, argv+2);
        auto ret = set_switch(off, channels);
        if (ret)
            return ret;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return set_switch(on, channels);
    }
    else if (iequals(cmd, "set"))
    {
        if (argc == 2)
            return show_szenes();
        return set_szene(argc-2, argv+2);
    }
    else if (iequals(cmd, "show"))
    {
        if (argc==2)
            return show(all_channels());
        return show(parse_channels(argc - 2, argv+2));
    }
    else if (iequals(cmd, "info"))
    {
        std::cout << "control Argus PDU SW-0816\n";
        std::cout << "address: " << addr << "\n";
        std::cout << "user: " << user << "\n";
        std::cout << "\n";
        std::cout << license_info;
        return 0;
    }
    else
        return usage(argv[0]);

    return 0;
}
