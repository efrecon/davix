#ifndef DAVIX_HTTPREQUEST_H
#define DAVIX_HTTPREQUEST_H

#include <vector>
#include <unistd.h>
#include <davix_types.h>
#include <davixuri.hpp>
#include <status/davixstatusrequest.hpp>
#include <params/davixrequestparams.hpp>
#include <request/httpcachetoken.hpp>

#ifndef __DAVIX_INSIDE__
#error "Only davix.h or davix.hpp should be included."
#endif

/**
  @file httprequest.hpp
  @author Devresse Adrien

  @brief Http low level request interface
 */




namespace Davix {

/// Callback for body providers
/// Before each time the body is provided, the callback will be called
/// once with buflen == 0.  The body may have to be provided >1 time
/// per request (for authentication retries etc.).
/// For a call with buflen > 0, the callback must return:
///    <0           : error, abort request; session error string must be set.
///     0           : ignore 'buffer' contents, end of body.
///     0 < x <= buflen : buffer contains x bytes of body data.  */
typedef dav_ssize_t (*HttpBodyProvider)(void *userdata,
                                    char *buffer, dav_size_t buflen);

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class NEONRequest;
class NEONSessionFactory;

namespace RequestFlag{
    ///
    /// \brief Request flag
    ///
    ///
    enum RequestFlag{
        SupportContinue100 = 0x01, /**< Enable support for 100 Continue code (default: OFF) */
        IdempotentRequest  = 0x02  /**< Specifie the request as Idempotent ( default : ON) */
    };

}


/// @class HttpRequest
/// @brief Http low level request interface
/// HTTPRequest is the main davix class for low level HTTP queries
/// HTTPRequest objects are provided by Davix::Context
///
class DAVIX_EXPORT HttpRequest
{
public:
    ///
    /// \brief HttpRequest constructor with a defined URL
    /// \param context davix context
    /// \param url URL of the resource
    /// \param err Davix error report system
    ///
    HttpRequest(Context & context, const Uri & url, DavixError** err);

    ///
    /// \brief HttpRequest constructor with a defined URL from a string
    /// \param context davix context
    /// \param url URL of the resource
    /// \param err Davix error report system
    ///
    HttpRequest(Context & context, const std::string & url, DavixError** err);

    ///
    /// \brief HttpRequest internal usage
    /// \param req
    ///
    HttpRequest(NEONRequest* req);
    virtual ~HttpRequest();

    ///  add a optional HTTP header request
    ///  replace an existing one if already exist
    ///  if the content of value of the header field is empty : remove an existing one
    ///  @param field  header field name
    ///  @param value header field value
    void addHeaderField(const std::string & field, const std::string & value);

    ///
    /// \brief set the request method ( "GET", "PUT", ... )
    /// \param method request method
    ///
    void setRequestMethod(const std::string & method);


    ///
    /// \brief set the request parameter
    /// \param parameters Davix Request parameters
    ///
    ///  define the request parameters, can be used to define parameters
    ///  like authentication scheme, timeout or user agent.
    void setParameters(const RequestParams &parameters );

    ///   @brief execute this request completely
    ///
    ///   the answer is accessible with \ref Davix::HttpRequest::getAnswerContent
    ///   @param err davix error report
    ///   @return 0 on success
    ///
    int executeRequest(DavixError** err);

    ///
    ///  set the content of the request from a string
    ///  an empty string set no request content
    ///  @warning this string is not duplicated internally for performance reasons
    ///
    void setRequestBody(const std::string & body);

    ///
    /// set the content of the request from a buffer
    ///  NULL pointer means a empty content
    ///
    void setRequestBody(const void * buffer, dav_size_t len_buff);

    ///
    /// set the content of the request from a file descriptor
    /// start at offset and read a maximum of len bytes
    ///
    void setRequestBody(int fd, dav_off_t offset, dav_size_t len);

    ///
    /// set a callback to provide the body of the requests
    ///
    void setRequestBody(HttpBodyProvider provider, dav_size_t len, void* udata);

    ///
    /// @brief start a multi-part HTTP Request
    ///
    ///  the multi-part HTTP Request of davix
    ///  should be used for request with a large answer
    ///
    ///
    /// @param err : DavixError error report system
    /// @return return 0 if success, or a negative value if an error occures
    ///
    int beginRequest(DavixError** err);

    ///
    /// read a block of a maximum size bytes in the answer
    /// can return < max_size bytes depending of the data available
    ///
    /// @param buffer : buffer to fill
    /// @param max_size : maximum number of byte to read
    /// @param err : DavixError error report system
    /// @return number of bytes readed
    ///
    dav_ssize_t readBlock(char* buffer, dav_size_t max_size, DavixError** err);


    ///
    /// read a block of a maximum size bytes in the answer into buffer
    /// can return < max_size bytes depending of the data available
    ///
    /// @param buffer : vector to fill
    /// @param max_size : maximum number of byte to read
    /// @param err : DavixError error report system
    /// @return number of bytes readed
    ///
    dav_ssize_t readBlock(std::vector<char> & buffer, dav_size_t max_size, DavixError** err);

    ///
    /// read a segment of size bytes, return always max_size excepted if the end of the content is reached
    ///
    /// @param buffer : vector to fill
    /// @param max_size : maximum number of byte to read
    /// @param err : DavixError error report system
    /// @return number of bytes readed
    ///
    dav_ssize_t readSegment(char* buffer, dav_size_t max_size, DavixError** err);



    ///
    /// write the full answer content to the given file descriptor
    /// @param fd : buffer to fill
    /// @param err : DavixError error report system
    /// @return number of bytes read
    ///
    dav_ssize_t readToFd(int fd, DavixError** err);

    ///
    /// write the first 'read_size' first bytes to the given file descriptor
    /// @param fd : buffer to fill
    /// @param read_size : number of bytes to read
    /// @param err : DavixError error report system
    /// @return number of bytes read
    ///
    dav_ssize_t readToFd(int fd, dav_size_t read_size, DavixError** err);

    ///
    /// read a line of text of a maximum size bytes in the answer
    /// @param buffer : buffer to fill
    /// @param max_size : maximum number of bytes to read
    /// @param err : DavixError error report system
    /// @return number of bytes readed, if return == max_size -> the line too big
    ///
    dav_ssize_t readLine(char* buffer, dav_size_t max_size, DavixError** err);

    ///
    /// finish a request stated with beginRequest
    int endRequest(DavixError** err);

    /// get a reference to the internal anwser content buffer
    const char* getAnswerContent();

    /// get content length
    /// @return content size, return -1 if chunked
    dav_ssize_t getAnswerSize() const;

    ///
    ///  clear the current result
    ///
    void clearAnswerContent();

    ///
    /// @return current request code error
    /// undefined if executeRequest or beginRequest has not be called before
    int getRequestCode();

    ///
    /// get the value associated to a header key in the request answer
    /// @param header_name : key of the header field
    /// @param value : reference of the string to set
    /// @return true if this header exist or false if it does not
    bool getAnswerHeader(const std::string & header_name, std::string & value) const;

    ///
    /// @brief Extract a cache token from this Request
    ///
    /// a cache token allows to optimize future request
    /// cache tokens are dynamically allocated and need to be free
    /// @return cache token or null pointer if not available
    HttpCacheToken* extractCacheToken() const;

    ///
    /// \brief use the cache token of a previous request, enable request o
    ///  optimizations ( session re-use, redirection caching, server operation support )
    /// \param token
    ///
    void useCacheToken(const HttpCacheToken* token);

    /// set a HttpRequest flag
    void setFlag(const RequestFlag::RequestFlag flag, bool value);

    /// get a HttpRequest flag value
    bool getFlag(const RequestFlag::RequestFlag flag);

private:
    HttpRequest(const HttpRequest &req);
    HttpRequest & operator=(const HttpRequest &);

    NEONRequest* d_ptr;

    friend class NEONRequest;
    friend class NEONSessionFactory;

};


/// @class GetRequest
/// @brief Http low level request configured for GET operation
class GetRequest : public HttpRequest{
public:
    ///
    /// \brief Construct a HttpRequest for GET a operation
    /// \param context
    /// \param uri
    /// \param err
    ///
    GetRequest(Context & context, const Uri & uri, DavixError** err);
};

/// @class PutRequest
/// @brief Http low level request configured for PUT operation
class PutRequest : public HttpRequest{
public:
    ///
    /// \brief Construct a HttpRequest for PUT a operation
    /// \param context
    /// \param uri
    /// \param err
    PutRequest(Context & context, const Uri & uri, DavixError** err);
};

/// @class HeadRequest
/// @brief Http low level request configured for HEAD operation
class HeadRequest : public HttpRequest{
public:
    ///
    /// \brief Construct a HttpRequest for a HEAD operation
    /// \param context
    /// \param uri
    /// \param err
    ///
    HeadRequest(Context & context, const Uri & uri, DavixError** err);
};


/// @class DeleteRequest
/// @brief Http low level request configured for DELETE operation
class DeleteRequest : public HttpRequest{
public:
    ///
    /// \brief  Construct a HttpRequest forfor a DELETE operation
    /// \param context
    /// \param uri
    /// \param err
    ///
    DeleteRequest(Context & context, const Uri & uri, DavixError** err);
};


/// @class PropfindRequest
/// @brief Webdav low level request configured for PROPFIND operation
class PropfindRequest : public HttpRequest{
public:
    ///
    /// \brief  Construct a HttpRequest for a PROPFIND operation
    /// \param context
    /// \param uri
    /// \param err
    ///
    PropfindRequest(Context & context, const Uri & uri, DavixError** err);
};


/// \cond PRIVATE_SYMBOLS
DAVIX_EXPORT bool httpcodeIsValid(int code);

DAVIX_EXPORT void httpcodeToDavixCode(int code, const std::string & scope, const std::string & end_message, DavixError** err);
/// \endcond PRIVATE_SYMBOLS
}

#endif // DAVIX_HTTPREQUEST_H