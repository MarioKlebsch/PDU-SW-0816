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

#ifndef HTTP_STATUS_ERROR_CATEGORY_H_
#define HTTP_STATUS_ERROR_CATEGORY_H_

#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

namespace boost
{
namespace system
{
template<>
struct is_error_code_enum<boost::beast::http::status>
{ static const bool value = true; };
}
}

namespace boost
{
namespace beast
{
namespace http
{

static inline boost::system::error_category& error_category()
{
    static class : public boost::system::error_category
    {
        const char * name() const BOOST_NOEXCEPT override { return "boost::beast::http::error_category";}
        
        std::string message( int ev ) const override
        {
            namespace http=boost::beast::http;
            switch(http::status(ev))
            {
                case http::status::unknown:                             return "unknown";

                case http::status::continue_: return "HTTP atatus code 100 Continue: "
                    "The server has received the request headers and the client should proceed to send the request body.";

                case http::status::switching_protocols: 
                    return "HTTP status code 101 Switching Protocols: "
                    "The requester has asked the server to switch protocols and the server has agreed to do so.";
                    
                case http::status::processing: 
                    return "HTTP status code 102 Processing: "
                    "A WebDAV request may contain many sub-requests involving file operations, requiring a long time to complete the request. This code indicates that the server has received and is processing the request, but no response is available yet. This prevents the client from timing out and assuming the request was lost. The status code is deprecated.";

                case http::status::ok: 
                    return "HTTP status code 200 OK:"
                    "Standard response for successful HTTP requests.";

                case http::status::created: 
                    return "HTTP status code 201 Created: "
                    "The request has been fulfilled, resulting in the creation of a new resource.";

                case http::status::accepted: 
                    return "HTTP status code 202 Accepted: "
                    "The request has been accepted for processing, but the processing has not been completed.";

                case http::status::non_authoritative_information:
                    return "HTTP status code 203 Non-Authoritative Information: "
                    "The server is a transforming proxy (e.g. a Web accelerator) that received a 200 OK from its origin, but is returning a modified version of the origin's response.";

                case http::status::no_content:
                    return "HTTP status code 204 No Content: "
                    "The server successfully processed the request, and is not returning any content.";

                case http::status::reset_content:
                    return "HTTP status code 205 Reset Content: "
                    "The server successfully processed the request, asks that the requester reset its document view, and is not returning any content.";
                case http::status::partial_content:
                    return "HTTP status code 206 Partial Content: "
                    "The server is delivering only part of the resource (byte serving) due to a range header sent by the client.";

                case http::status::multi_status:
                    return "HTTP status code 207 Multi-Status: "
                    "The message body that follows is by default an XML message and can contain a number of separate response codes, depending on how many sub-requests were made.";

                case http::status::already_reported:
                    return "HTTP status code 208 Already Reported: "
                    "The members of a DAV binding have already been enumerated in a preceding part of the (multistatus) response, and are not being included again.";

                case http::status::im_used:
                    return "HTTP status code 226 IM Used: "
                    "The server has fulfilled a request for the resource, and the response is a representation of the result of one or more instance-manipulations applied to the current instance.";

                case http::status::multiple_choices:
                    return "HTTP status code 300 Multiple Choices: "
                    "Indicates multiple options for the resource from which the client may choose (via agent-driven content negotiation).";

                case http::status::moved_permanently:
                    return "HTTP status code 301 Moved Permanently: "
                    "This and all future requests should be directed to the given URI.";

                case http::status::found:
                    return "HTTP status code 302 Found:"
                    "Tells the client to look at (browse to) another URL.";

                case http::status::see_other:
                    return "HTTP status code 303 See Other: "
                    "The response to the request can be found under another URI using the GET method.";

                case http::status::not_modified:
                    return "HTTP status code 304 Not Modified: "
                    "Indicates that the resource has not been modified since the version specified by the request headers If-Modified-Since or If-None-Match.";
                    
                case http::status::use_proxy:
                    return "HTTP status code 305 Use Proxy: "
                    "The requested resource is available only through a proxy, the address for which is provided in the response";

                case http::status::temporary_redirect:
                    return "HTTP status code 307 Temporary Redirect: "
                    "In this case, the request should be repeated with another URI; however, future requests should still use the original URI.";

                case http::status::permanent_redirect:
                    return "HTTP status code 308 Permanent Redirect: "
                    "This and all future requests should be directed to the given URI.";

                case http::status::bad_request:
                    return "HTTP status code 400 Bad Request: "
                    "The server cannot or will not process the request due to an apparent client error (e.g., malformed request syntax, size too large, invalid request message framing, or deceptive request routing).";

                case http::status::unauthorized:
                    return "HTTP status code 401 Unauthorized: "
                    "Similar to 403 Forbidden, but specifically for use when authentication is required and has failed or has not yet been provided.";

                case http::status::payment_required:
                    return "HTTP status code 402 Payment Required: "
                    "Reserved for future use. The original intention was that this code might be used as part of some form of digital cash or micropayment scheme.";

                case http::status::forbidden:
                    return "HTTP status code 403 Forbidden: "
                    "The request contained valid data and was understood by the server, but the server is refusing action. ";

                case http::status::not_found:
                    return "HTTP status code 404 Not Found: "
                    "The requested resource could not be found but may be available in the future.";

                case http::status::method_not_allowed:
                    return "HTTP status code 405 Method Not Allowed: "
                    "A request method is not supported for the requested resource.";

                case http::status::not_acceptable:
                    return "HTTP status code 406 Not Acceptable: "
                    "The requested resource is capable of generating only content not acceptable according to the Accept headers sent in the request.";

                case http::status::proxy_authentication_required:
                    return "HTTP status code 407 Proxy Authentication Required: "
                    "The client must first authenticate itself with the proxy.";

                case http::status::request_timeout:
                    return "HTTP status code 408 Request Timeout: "
                    "The server timed out waiting for the request.";

                case http::status::conflict:
                    return "HTTP status code 409 Conflict: "
                    "Indicates that the request could not be processed because of conflict in the current state of the resource, such as an edit conflict between multiple simultaneous updates.";

                case http::status::gone:
                    return "HTTP status code 410 Gone: "
                    "Indicates that the resource requested was previously in use but is no longer available and will not be available again.";

                case http::status::length_required:
                    return "HTTP status code 411 Length Required: "
                    "The request did not specify the length of its content, which is required by the requested resource.";

                case http::status::precondition_failed:                 return "HTTP status code xxx precondition_failed";
                case http::status::payload_too_large:                   return "HTTP status code xxx payload_too_large";
                case http::status::uri_too_long:                        return "HTTP status code xxx uri_too_long";
                case http::status::unsupported_media_type:              return "HTTP status code xxx unsupported_media_type";
                case http::status::range_not_satisfiable:               return "HTTP status code xxx range_not_satisfiable";
                case http::status::expectation_failed:                  return "HTTP status code xxx expectation_failed";
                case http::status::misdirected_request:                 return "HTTP status code xxx misdirected_request";
                case http::status::unprocessable_entity:                return "HTTP status code xxx unprocessable_entity";
                case http::status::locked:                              return "HTTP status code xxx locked";
                case http::status::failed_dependency:                   return "HTTP status code xxx failed_dependency";
                case http::status::upgrade_required:                    return "HTTP status code xxx upgrade_required";
                case http::status::precondition_required:               return "HTTP status code xxx precondition_required";
                case http::status::too_many_requests:                   return "HTTP status code xxx too_many_requests";
                case http::status::request_header_fields_too_large:     return "HTTP status code xxx request_header_fields_too_large";
                case http::status::connection_closed_without_response:  return "HTTP status code xxx connection_closed_without_response";
                case http::status::unavailable_for_legal_reasons:       return "HTTP status code xxx unavailable_for_legal_reasons";
                case http::status::client_closed_request:               return "HTTP status code xxx client_closed_request";

                case http::status::internal_server_error:               return "HTTP status code xxx internal_server_error";
                case http::status::not_implemented:                     return "HTTP status code xxx not_implemented";
                case http::status::bad_gateway:                         return "HTTP status code xxx bad_gateway";
                case http::status::service_unavailable:                 return "HTTP status code xxx service_unavailable";
                case http::status::gateway_timeout:                     return "HTTP status code xxx gateway_timeout";
                case http::status::http_version_not_supported:          return "HTTP status code xxx http_version_not_supported";
                case http::status::variant_also_negotiates:             return "HTTP status code xxx variant_also_negotiates";
                case http::status::insufficient_storage:                return "HTTP status code xxx insufficient_storage";
                case http::status::loop_detected:                       return "HTTP status code xxx loop_detected";
                case http::status::not_extended:                        return "HTTP status code xxx not_extended";
                case http::status::network_authentication_required:     return "HTTP status code xxx network_authentication_required";
                case http::status::network_connect_timeout_error:       return "HTTP status code xxx network_connect_timeout_error";
                default:                                                return "???";
            }
        }
    } instance;
    return instance;
}

boost::system::error_code make_error_code(status e)
{
    return {int(e), error_category()};
}

}
}
}

#endif /* HTTP_STATUS_ERROR_CATEGORY_H_ */

