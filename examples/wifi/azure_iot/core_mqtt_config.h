#ifndef CORE_MQTT_CONFIG_DEFAULTS_
#define CORE_MQTT_CONFIG_DEFAULTS_

int printf(const char *format, ...);
#define LogError( message )                  printf message
#define LogInfo( message )                   printf message
#define LogWarn( message )                   printf message
#define LogDebug( message )                  printf message 
#define SdkLog( message )                    printf message

/**
 * @brief The maximum number of MQTT PUBLISH messages that may be pending
 * acknowledgement at any time.
 *
 * QoS 1 and 2 MQTT PUBLISHes require acknowledgment from the server before
 * they can be completed. While they are awaiting the acknowledgment, the
 * client must maintain information about their state. The value of this
 * macro sets the limit on how many simultaneous PUBLISH states an MQTT
 * context maintains.
 */
#define MQTT_STATE_ARRAY_MAX_COUNT    10U

#endif /* ifndef CORE_MQTT_CONFIG_DEFAULTS_ */
