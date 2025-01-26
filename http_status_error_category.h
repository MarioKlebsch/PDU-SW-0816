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

                case http::status::precondition_failed:
                    return "HTTP status code 412 Precondition Failed: "
                    "The server does not meet one of the preconditions that the requester put on the request header fields.";

                case http::status::payload_too_large:
                    return "HTTP status code 413 Payload Too Large: "
                    "The request is larger than the server is willing or able to process.";

                case http::status::uri_too_long:
                    return "HTTP status code 414 URI Too Long: "
                    "The URI provided was too long for the server to process.";

                case http::status::unsupported_media_type:
                    return "HTTP status code 415 Unsupported Media Type: "
                    "The request entity has a media type which the server or resource does not support.";

                case http::status::range_not_satisfiable:
                    return "HTTP status code 416 Range Not Satisfiable: "
                    "The client has asked for a portion of the file (byte serving), but the server cannot supply that portion. ";

                case http::status::expectation_failed:
                    return "HTTP status code 417 Expectation Failed: "
                    "The server cannot meet the requirements of the Expect request-header field.";

                case http::status::misdirected_request:
                    return "HTTP status code 421 Misdirected Request: "
                    "The request was directed at a server that is not able to produce a response (for example because of connection reuse).";

                case http::status::unprocessable_entity:
                    return "HTTP status code 422 Unprocessable Content: "
                    "The request was well-formed (i.e., syntactically correct) but could not be processed.";

                case http::status::locked:
                    return "HTTP status code 423 Locked: "
                    "The resource that is being accessed is locked.";

                case http::status::failed_dependency:
                    return "HTTP status code 424 Failed Dependency: "
                    "The request failed because it depended on another request and that request failed (e.g., a PROPPATCH).";

                case http::status::upgrade_required:
                    return "HTTP status code 426 Upgrade Required: "
                    "The client should switch to a different protocol such as TLS/1.3, given in the Upgrade header field.";

                case http::status::precondition_required:
                    return "HTTP status code 428 Precondition Required: "
                    "The origin server requires the request to be conditional.";

                case http::status::too_many_requests:
                    return "HTTP status code 429 Too Many Requests: "
                    "The user has sent too many requests in a given amount of time. ";

                case http::status::request_header_fields_too_large:
                    return "HTTP status code 431 Request Header Fields Too Large: "
                    "The server is unwilling to process the request because either an individual header field, or all the header fields collectively, are too large.";

                case http::status::connection_closed_without_response:
                    return "HTTP status code 444 connection_closed_without_response";

                case http::status::unavailable_for_legal_reasons:
                    return "HTTP status code 451 Unavailable For Legal Reasons: "
                    "A server operator has received a legal demand to deny access to a resource or to a set of resources that includes the requested resource.";

                case http::status::client_closed_request:
                    return "HTTP status code 499 client_closed_request";

                case http::status::internal_server_error:
                    return "HTTP status code 500 Internal Server Error: "
                    "A generic error message, given when an unexpected condition was encountered and no more specific message is suitable.";

                case http::status::not_implemented:
                    return "HTTP status code 501 Not Implemented: "
                    "The server either does not recognize the request method, or it lacks the ability to fulfil the request. ";

                case http::status::bad_gateway:
                    return "HTTP status code 502 Bad Gateway: "
                    "The server was acting as a gateway or proxy and received an invalid response from the upstream server.";

                case http::status::service_unavailable:
                    return "HTTP status code 503 Service Unavailable: "
                    "The server cannot handle the request (because it is overloaded or down for maintenance). ";

                case http::status::gateway_timeout:
                    return "HTTP status code 504 Gateway Timeout: "
                    "The server was acting as a gateway or proxy and did not receive a timely response from the upstream server.";

                case http::status::http_version_not_supported:
                    return "HTTP status code 505 HTTP Version Not Supported: "
                    "The server does not support the HTTP version used in the request.";

                case http::status::variant_also_negotiates:
                    return "HTTP status code 506 Variant Also Negotiates: "
                    "Transparent content negotiation for the request results in a circular reference.";

                case http::status::insufficient_storage:
                    return "HTTP status code 507 Insufficient Storage: "
                    "The server is unable to store the representation needed to complete the request.";

                case http::status::loop_detected:
                    return "HTTP status code 508 Loop Detected: "
                    "The server detected an infinite loop while processing the request (sent instead of 208 Already Reported).";

                case http::status::not_extended:
                    return "HTTP status code 510 Not Extended: "
                    "Further extensions to the request are required for the server to fulfil it.";

                case http::status::network_authentication_required:
                    return "HTTP status code 511 Network Authentication Required: "
                    "The client needs to authenticate to gain network access.";

                case http::status::network_connect_timeout_error:
                    return "HTTP status code 599 Network Connect Timeout Error: "
                    "An error used by some HTTP proxies to signal a network connect timeout behind the proxy to a client in front of the proxy.";

                default:
                    return "???";
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

