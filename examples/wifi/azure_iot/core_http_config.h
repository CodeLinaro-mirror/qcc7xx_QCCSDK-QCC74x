#ifndef CORE_HTTP_CONFIG_DEFAULTS_
#define CORE_HTTP_CONFIG_DEFAULTS_

/**
 * @brief Maximum size, in bytes, of headers allowed from the server.
 *
 * If the total size in bytes of the headers received from the server exceeds
 * this configuration, then the status code
 * #HTTPSecurityAlertResponseHeadersSizeLimitExceeded is returned from
 * #HTTPClient_Send.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. <br>
 * <b>Default value:</b> `2048`
 */
#ifndef HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES
    #define HTTP_MAX_RESPONSE_HEADERS_SIZE_BYTES    2048U
#endif

/**
 * @brief The HTTP header "User-Agent" value.
 *
 * The following header line is automatically written to
 * #HTTPRequestHeaders_t.pBuffer:
 * "User-Agent: my-platform-name\r\n"
 *
 * <b>Possible values:</b> Any string. <br>
 * <b>Default value:</b> `my-platform-name`
 */
#ifndef HTTP_USER_AGENT_VALUE
    #define HTTP_USER_AGENT_VALUE    "az"
#endif

/**
 * @brief The maximum duration between non-empty network reads while receiving
 * an HTTP response via the #HTTPClient_Send API function.
 *
 * The transport receive function may be called multiple times until the end of
 * the response is detected by the parser. This timeout represents the maximum
 * duration that is allowed without any data reception from the network for the
 * incoming response.
 *
 * If the timeout expires, the #HTTPClient_Send function will return
 * #HTTPNetworkError.
 *
 * If #HTTPResponse_t.getTime is set to NULL, then this HTTP_RECV_RETRY_TIMEOUT_MS
 * is unused. When this timeout is unused, #HTTPClient_Send will not retry the
 * transport receive calls that return zero bytes read.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. A small timeout value
 * is recommended. <br>
 * <b>Default value:</b> `10`
 */
#ifndef HTTP_RECV_RETRY_TIMEOUT_MS
    #define HTTP_RECV_RETRY_TIMEOUT_MS    ( 10U )
#endif

/**
 * @brief The maximum duration between non-empty network transmissions while
 * sending an HTTP request via the #HTTPClient_Send API function.
 *
 * When sending an HTTP request, the transport send function may be called multiple
 * times until all of the required number of bytes are sent.
 * This timeout represents the maximum duration that is allowed for no data
 * transmission over the network through the transport send function.
 *
 * If the timeout expires, the #HTTPClient_Send function returns #HTTPNetworkError.
 *
 * If #HTTPResponse_t.getTime is set to NULL, then this HTTP_RECV_RETRY_TIMEOUT_MS
 * is unused. When this timeout is unused, #HTTPClient_Send will not retry the
 * transport send calls that return zero bytes sent.
 *
 * <b>Possible values:</b> Any positive 32 bit integer. A small timeout value
 * is recommended. <br>
 * <b>Default value:</b> `10`
 */
#ifndef HTTP_SEND_RETRY_TIMEOUT_MS
    #define HTTP_SEND_RETRY_TIMEOUT_MS    ( 10U )
#endif

#ifndef LIBRARY_LOG_NAME
    #define LIBRARY_LOG_NAME    "HTTP"
#endif

int printf(const char *format, ...);
#define LogError( message )                  printf message
#define LogInfo( message )                   printf message
#define LogWarn( message )                   printf message
#define LogDebug( message )                  printf message 
#define SdkLog( message )                    printf message

#endif /* ifndef CORE_HTTP_CONFIG_DEFAULTS_ */
