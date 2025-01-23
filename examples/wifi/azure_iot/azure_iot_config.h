#ifndef AZURE_IOT_CONFIG_DEFAULTS_ 
#define AZURE_IOT_CONFIG_DEFAULTS_

int printf(const char *format, ...);
#define LogError( message )                  printf message
#define LogInfo( message )                   printf message
#define LogWarn( message )                   printf message
#define LogDebug( message )                  printf message 
#define SdkLog( message )                    printf message

#endif /* ifndef AZURE_IOT_CONFIG_DEFAULTS_ */
