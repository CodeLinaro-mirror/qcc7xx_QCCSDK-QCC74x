#include "FreeRTOS_POSIX.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <lwip/errno.h>
#include <netdb.h>
#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/net_sockets.h"

#include "utils_getopt.h"

#include "mqtt.h"
#include "shell.h"

#include "qcc74x_rtc.h"

static uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
static uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */

static shell_sig_func_ptr abort_exec;
static int test_sockfd;
static const char* addr;


typedef struct {
    int ssl_inited;
    mbedtls_net_context net;
    mbedtls_x509_crt ca_cert;
    mbedtls_x509_crt owncert;
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    mbedtls_pk_context pkey;
} ssl_param_t;

typedef struct {
    char *ca_cert;
    int ca_cert_len;
    char *own_cert;
    int own_cert_len;
    char *private_cert;
    int private_cert_len;

    char **alpn;
    int alpn_num;

    char *psk;
    int psk_len;
    char *pskhint;
    int pskhint_len;

    char *sni;
} ssl_conn_param_t;

static int ssl_random(void *prng, unsigned char *output, size_t output_len)
{
    (void)prng;
    qcc74x_trng_readlen(output, output_len);
    return 0;
}

char ca_cert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDAzCCAeugAwIBAgIUSIynf9c+C4TGvBNrpOGW0EsWKRUwDQYJKoZIhvcNAQEL\n"
    "BQAwETEPMA0GA1UEAwwGTXlDQV8xMB4XDTI0MDUyMzAxNTIzOFoXDTI1MDUyMzAx\n"
    "NTIzOFowETEPMA0GA1UEAwwGTXlDQV8xMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A\n"
    "MIIBCgKCAQEAj7Xp94L7VLyo6m4AdvKIa1pOjZQUzPnfgkAJhdHmupIMhdx8/L/i\n"
    "Yn55tZhPOzk7S+CYuYoH1Bx/tvmP+HJ9J8qeNOPm+QbiYNUSJcs2vXkKMFgiSqh4\n"
    "zE6B80+clSSR0de80s7td0PTL9k/emQSu+mV1q2JrXyhcP6/fumt1TzrmTb2SxiB\n"
    "rMQh76v8NfV0a0Y78QDd+S0JIhJ27S610DCSnl+B1pgI8sMuwtSSNOyNYFbym2PO\n"
    "LYUYaN9Lup7i2pxPi/p/tIwBq3mbs01JawBMFArTkidxIK+xkf4NVUT1weurw+s8\n"
    "3yC+pomUtvKpCNxdgSslnCxNhoUYxxCrawIDAQABo1MwUTAdBgNVHQ4EFgQUz1kI\n"
    "khTZxHjVYlOk16H7v57pJCowHwYDVR0jBBgwFoAUz1kIkhTZxHjVYlOk16H7v57p\n"
    "JCowDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEASyul5PoQ8V0M\n"
    "+lKOCxeL8qbYTTLJzcIGcN7FQzTWHYqfmdlc+9IZ7mHk1RR+fxNll3EpHaqA/U9J\n"
    "bRUiBIzsMRWaCl0g3NCcZOILQ/hfgh6TgFyo5bOUKNOjBKGrV4/mnOOyPvPc5n/H\n"
    "BW3LjqmfE2CFeUruXutOPIkY8YDiPRFwCSaNapO4Bc6+kgYQz7dPEWOF0b2IcJ7O\n"
    "vD2JPeKSoUxPs71TUNzGBQx+mbXxULrhFkSs9jRdPUJQpJqaq+SVLLXOU+RsQab0\n"
    "dqud+4N3VgjA1tI/Oi23sF4xMotaWH4VrWIxuu0O8I3GYx+ZhFyhRa33ma7JxZtu\n"
    "VlA0gHsH5Q==\n"
    "-----END CERTIFICATE-----\n";

char own_cert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC9DCCAdygAwIBAgIUZp6RfB5tG+5MCi/Wtjk2eAxtk3QwDQYJKoZIhvcNAQEL\n"
    "BQAwETEPMA0GA1UEAwwGTXlDQV8xMB4XDTI0MDUyMzAxNTIzOFoXDTI1MDUyMzAx\n"
    "NTIzOFowEzERMA8GA1UEAwwIY2xpZW50XzEwggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
    "DwAwggEKAoIBAQCo2b69q96nG1eO7/PZCYRg78mk1Z9KbWLtQftZDJJIuIv/Xa/P\n"
    "suSUajK8bJJ5CUlP05cowcccBqZ41mlP4nC0iGqesnJd5rDo/QIo09Oms67KkW9y\n"
    "wB8vGgo5nQjeT/q05iX4di2Bo3Npj5brKocbkH4rTWidKh1LoDETW709IJICWTkU\n"
    "4Sj5IQnAuY8a9MREBkjo03KctDWG2qY9JFOdb5E6K+UmbXL7t68ItOdAlK80HUvn\n"
    "+AyhXjabfeP9JD3/UABm37Lmm3hyfbGMCJgCkVt/TXTZun+30fHzfE5Ok9+iKdki\n"
    "DZMw9DfSp+7jh4lyb01S5VN+T/5pOpEUjfi3AgMBAAGjQjBAMB0GA1UdDgQWBBSE\n"
    "t5Zai9tEI2DNfbocINJowkpNAjAfBgNVHSMEGDAWgBTPWQiSFNnEeNViU6TXofu/\n"
    "nukkKjANBgkqhkiG9w0BAQsFAAOCAQEAcIzEp40BLHawKNzEQ18iO4MWcSVmCvvG\n"
    "CqPrmuHSQ5TUkKUwlu4Kf1SqRlhy2W5dt04Ws4z7olHu6pIzKKGSvHUVRPM85+CH\n"
    "ZzfT2zA7mD4UgX8LO9g2HhhBzNKZQFzZgt+FPy1XxMRD+zunnDDXrmTNW2whBZsP\n"
    "AKIro78SpJbwUpQW/Go6a2YYMYY6QGIXcoNFyhkL2Wk6SIMqmvrHXQwF/LqQZupQ\n"
    "CYVdac/j7bQPKweIclsq7b9O1dixurCCSjJ6V7CBTZbUM53jBs2BZA60lRmwVHFl\n"
    "oGtU9KCUhH7IJXQYAm1sqxnfICaVJdxb0a5ahCj5LMsZQXFvabOWNg==\n"
    "-----END CERTIFICATE-----\n";

char private_cert[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCo2b69q96nG1eO\n"
    "7/PZCYRg78mk1Z9KbWLtQftZDJJIuIv/Xa/PsuSUajK8bJJ5CUlP05cowcccBqZ4\n"
    "1mlP4nC0iGqesnJd5rDo/QIo09Oms67KkW9ywB8vGgo5nQjeT/q05iX4di2Bo3Np\n"
    "j5brKocbkH4rTWidKh1LoDETW709IJICWTkU4Sj5IQnAuY8a9MREBkjo03KctDWG\n"
    "2qY9JFOdb5E6K+UmbXL7t68ItOdAlK80HUvn+AyhXjabfeP9JD3/UABm37Lmm3hy\n"
    "fbGMCJgCkVt/TXTZun+30fHzfE5Ok9+iKdkiDZMw9DfSp+7jh4lyb01S5VN+T/5p\n"
    "OpEUjfi3AgMBAAECggEAQcs/IOdUxibIUoE7somb1K37E8jN3hGLp8m7KDLW2ZFX\n"
    "s/UMqdEacp6DEJr55EHzGDDpyM7hSBFrUtCxjpg8tIwnh5kfKWnX66GS8te/tEh1\n"
    "xqcvFza3sAHklBiVuRLPLSg1CYD7MHXBZhO2igIzTVFbaIIsTnl3mt1b4iAwOB3S\n"
    "3rE5yRDvYXo9SM8aoL7iDaQoWd4z8hLiQmxMBDwI9sjp0BFUTLBjWu/qcikcWwOx\n"
    "Z9MtYwwl6Dldvf3tJ9cTY4/6wQKMMsLqS3Jov2k8xV0DvGU0Xkq6N96L++iaE3Iz\n"
    "bz+AbWz1J3UAHJ5T844TJtn2KxNWdqIgUryVkbqlKQKBgQDoYsBUYZnth735AKnl\n"
    "kLaP0jBKbVMaMa3ihA1qNUcCKkKfxP1xenrbS94UWRJzLq/txehb4YsptJ1AJ624\n"
    "HeMUm85A9Ka4vFxfBmMOYyxMx9OMFQbxxijv2oRP+3irupBAAEVuAbpaI4YWL4Wn\n"
    "mfeFzLFggt4bTGiUO7jvQOT4CwKBgQC6AjLtep8iUSQz/xo5ULnw/lHMf8lOin5t\n"
    "LqoA3qwx/ootOZp5WG9rVaKimELxdQwO4DtH9ZGJfbKeJDh6AbePjcHShfK2UzI0\n"
    "/xvZLzI+Wgz2ybX9HJ6y/rUy83eXMI5/G5zJEXRjgLZdDBkKP5FUllzn9dYQKq4n\n"
    "froofr0xhQKBgGUEsIi92NUJwgphQHm7u7CvdTCVb5+eYvFp6A74VALb7hRX5LiU\n"
    "vp1f0I1V3xBVBkM9WfG/DOi3S/hch0bXLySBSlqwP11k/F6Ofzb844AJCawGY/tr\n"
    "4dXKTuQZ8+3q7tPzijr041ZVxO8V/2rJInF2dtD2cqi5QWjWwWC3/BDbAoGBALhw\n"
    "Zb9Ez3Zfrt5AHeinOzjTdJiqPRo8CZCJrNif826/TpP5gkrAffqYN2OyRu7VeNdn\n"
    "UGDSBzWmBe75S2bvPOyDS6VMXJadeDNEooCAY/mfoSs0Z9hLALHMDjAvam3oH+O5\n"
    "pYVaYWYowqsQy+N1lM2jKwx2n4P4TieraTuuMDthAoGBALNnxVZyGsUj4pqTIbnh\n"
    "9CbfDkf2os2KwuLQTtkmMTRQe88mDHaVlY7dFBeuAhF9gf3Z/TapiesBlgPxehiu\n"
    "1aNylDK6ScLXxzq/ZEIX7Ou4gP3hBAHrtHbRYS8klYkV7tNCW0fmiz1M3C/8HP0V\n"
    "FHFBlzbNHqryPsFm7VVC4TyY\n"
    "-----END PRIVATE KEY-----\n";

static int tcp_client_connect(char *ip, char *port_str)
{
    int fd;
    int res;

    struct sockaddr_in addr;
    ip4_addr_t remote_ip;
    int port = atoi(port_str);

    ip4addr_aton(ip, &remote_ip);

    printf("tcp client connect %s:%d\r\n", ip4addr_ntoa(&remote_ip), port);

    {
        if ( (fd =  socket(AF_INET, SOCK_STREAM, 0))  < 0) {
            printf("socket create failed\r\n");
            return -2;
        }

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_len = sizeof(addr);
        addr.sin_port = htons(port);
        //addr.sin_addr.s_addr = ((struct in_addr *) hostinfo->h_addr)->s_addr;
        addr.sin_addr.s_addr = ip4_addr_get_u32(&remote_ip);
    }

    printf("tcp_client_connect fd:%d\r\n", fd);

    int on= 1;
    res = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    if (res != 0) {
        printf("setsockopt failed, res:%d\r\n", res);
    }

    res = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (res < 0) {
        printf("connect failed, res:%d\r\n", res);
        close(fd);
    }

    return fd;
}

static int ssl_client_connect(char *ip, char *port, ssl_param_t *ssl_param)
{
    int fd, ret;
    ssl_conn_param_t param = {0};
    //int mbedtls_platform_set_printf(int (*printf_func)(const char *, ...));
    //mbedtls_platform_set_printf(printf);

    memset(ssl_param, 0, sizeof(*ssl_param));
    memset(&param, 0, sizeof(param));

    param.ca_cert = ca_cert;
    param.ca_cert_len = sizeof(ca_cert);

    param.own_cert = own_cert;
    param.own_cert_len = sizeof(own_cert);

    param.private_cert = private_cert;
    param.private_cert_len = sizeof(private_cert);

    fd = tcp_client_connect(ip, port);
    if (fd < 0) {
        printf("tcp_client_connect fd:%d\r\n", fd);
        return -1;
    }

    param.sni = "localhost_1";

    ssl_param->ssl_inited = 1;
    /*
     * Initialize the connection
     */
    ssl_param->net.fd = fd;

    mbedtls_ssl_config_init(&ssl_param->conf);
    mbedtls_ssl_init(&ssl_param->ssl);
    mbedtls_x509_crt_init(&ssl_param->ca_cert);
    mbedtls_x509_crt_init(&ssl_param->owncert);
    mbedtls_pk_init(&ssl_param->pkey);

    ret = mbedtls_x509_crt_parse(&ssl_param->ca_cert, (unsigned char *)param.ca_cert, (size_t)param.ca_cert_len);
    if (ret < 0) {
        printf("[MBEDTLS] ssl connect: root parse failed- 0x%x\r\n", -ret);
        goto err;
    }
    // ret = mbedtls_x509_crt_parse(&ssl_param->owncert, (unsigned char *)param.own_cert, (size_t)param.own_cert_len);
    // if (ret < 0) {
    //  printf("[MBEDTLS] ssl connect: x509 parse failed- 0x%x\r\n", -ret);
    //  goto err;
    // }
    // ret = mbedtls_pk_parse_key(&ssl_param->pkey, (unsigned char *)param.private_cert, param.private_cert_len, NULL, 0);
    // if (ret != 0) {
    //  printf("[MBEDTLS] ssl connect: x509 parse failed- 0x%x\r\n", -ret);
    //     goto err;
    // }
    ret = mbedtls_ssl_config_defaults(&ssl_param->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf("[MBEDTLS] ssl connect: x509 config failed- 0x%x\r\n", -ret);
        goto err;
    }
    //mbedtls_ssl_conf_authmode(&ssl_param->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_authmode(&ssl_param->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&ssl_param->conf, &ssl_param->ca_cert, NULL);
    // mbedtls_ssl_conf_own_cert(&ssl_param->conf, &ssl_param.owncert, &ssl_param.pkey);
    mbedtls_ssl_conf_rng(&ssl_param->conf, ssl_random, NULL);
    //mbedtls_ssl_conf_dbg(&ssl_param->conf, my_debug, NULL);
    //mbedtls_ssl_conf_alpn_protocols(&ssl_param->conf, (const char **)alpn_str);
    //mbedtls_ssl_conf_psk(&ssl_param->conf, (const unsigned char *)"psk123456", 10, (const unsigned char *)"identity", 9);
    if((ret = mbedtls_ssl_setup(&ssl_param->ssl, &ssl_param->conf)) != 0) {
        printf("mbedtls ssl_setup fail \r\n", -ret);
        goto err;
    }

    if (param.sni) {
        mbedtls_ssl_set_hostname(&ssl_param->ssl, param.sni);
    }
    mbedtls_ssl_set_bio(&ssl_param->ssl, &ssl_param->net, mbedtls_net_send, mbedtls_net_recv, NULL);

    /*
     * handshake
     */
    printf("[MBEDTLS] Performing the SSL/TLS handshake ... \r\n");
    while ((ret = mbedtls_ssl_handshake(&ssl_param->ssl)) != 0) {
        if ((ret != MBEDTLS_ERR_SSL_WANT_READ) && (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
            printf("[MBEDTLS] ssl connect: mbedtls_ssl_handshake returned -0x%x\r\n", -ret);
            goto err;
        }
    }

    /*
     * verify the server certificate
     */
    printf("[MBEDTLS] ...... Verifying peer X.509 certificate ... \r\n");
    ret = mbedtls_ssl_get_verify_result(&ssl_param->ssl);
    if (ret != 0) {
        printf("[MBEDTLS] ssl connect: verify result not confirmed - %d\r\n", -ret);
        goto err;
    }

    printf("[MBEDTLS] ssl connect ok\r\n");

	/* make non-blocking */
    if (fd != -1) {
        int iMode = 1;
        ioctlsocket(fd, FIONBIO, &iMode);
    }


	return fd;

err:
	mbedtls_ssl_close_notify(&ssl_param->ssl);
    mbedtls_x509_crt_free(&ssl_param->ca_cert);
    mbedtls_ssl_free(&ssl_param->ssl);
    mbedtls_ssl_config_free(&ssl_param->conf);

    close(ssl_param->net.fd);
	return -1;
}

/**
 * @brief The function will be called whenever a PUBLISH message is received.
 */
static void publish_callback_1(void** unused, struct mqtt_response_publish *published);

ssl_param_t g_ssl_param = {0};
static void test_close(int sig)
{
    if (test_sockfd)
    {
        close(test_sockfd);
    }

    printf("mqtt_pub stop publish to %s\r\n", addr);

    abort_exec(sig);
}

static int example_mqtt(int argc, const char *argv[])
{
    const char* port;
    const char* topic;

    int ret = 0;
    // int argc = 0;

    abort_exec = shell_signal(1, test_close);

    struct qcc74x_device_s *rtc;

    rtc = qcc74x_device_get_by_name("rtc");
    qcc74x_rtc_set_time(rtc, 0);
    /* our build utc time 1686873600 2023-06-16 00:00:00 */
    time_t time_stamp = 1686873600;

    /* get address (argv[1] if present) */
    if (argc > 1) {
        addr = argv[1];
    }

    /* get port number (argv[2] if present) */
    if (argc > 2) {
        port = argv[2];
    }

    /* get the topic name to publish */
	topic = "qcc74x_undef";

    /* open the non-blocking TCP socket (connecting to the broker) */
	test_sockfd = ssl_client_connect(addr, port, &g_ssl_param);
	struct custom_socket_handle handle;
	handle.type = MQTTC_PAL_CONNTION_TYPE_TLS;
	handle.ctx.fd = test_sockfd;
	handle.ctx.ssl_ctx = &g_ssl_param.ssl;

    if (test_sockfd < 0) {
        printf("Failed to open socket: %d\r\n", test_sockfd);
        test_close(SHELL_SIGINT);
    }

    /* setup a client */
    struct mqtt_client client;

    mqtt_init(&client, &handle, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), publish_callback_1);
    /* Create an anonymous session */
    const char* client_id = NULL;
    /* Ensure we have a clean session */
    uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION;
    /* Send connection request to the broker. */
    ret = mqtt_connect(&client, client_id, NULL, NULL, 0, NULL, NULL, connect_flags, 400);

    if (ret != MQTT_OK)
    {
        printf("fail \r\n");
    }
    /* check that we don't have any errors */
    if (client.error != MQTT_OK) {
        printf("error: %s\r\n", mqtt_error_str(client.error));
        test_close(SHELL_SIGINT);
    }

    /* start publishing the time */
    printf("%s is ready to begin publishing the time.\r\n", argv[0]);

    while(1) {
        /* get the current time */
        time_t time = (QCC74x_RTC_TIME2SEC(qcc74x_rtc_get_time(rtc)) + time_stamp);
        struct tm *gmt = gmtime(&time);
        char time_buf[26];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", gmt);
        // printf("%s\r\n", time_buf);

        /* print a message */
        char application_message[256] = {"{\"hello mqtt by qcc743/8 !\"}\r\n"};
        snprintf(&application_message[strlen(application_message)], sizeof(application_message) - strlen(application_message), "Now time is %s", time_buf);
        printf("%s published : \"%s\"\r\n", argv[0], application_message);

        /* publish the time */
        mqtt_publish(&client, topic, application_message, strlen(application_message) + 1, MQTT_PUBLISH_QOS_0);
        mqtt_sync(&client);

        /* check for errors */
        if (client.error != MQTT_OK) {
            printf("error: %s\r\n", mqtt_error_str(client.error));
            test_close(SHELL_SIGINT);
        }
        vTaskDelay(3000);
    }

    /* disconnect */
    /* exit */
    test_close(SHELL_SIGINT);

}

static void publish_callback_1(void** unused, struct mqtt_response_publish *published)
{
    /* not used in this example */
}

#ifdef CONFIG_SHELL
#include <shell.h>

extern uint32_t wifi_state;
static int check_wifi_state(void)
{
    if (wifi_state == 1)
    {
        return 0;
    } else {
        return 1;
    }
}

static int cmd_mqtts_publisher(int argc, const char **argv)
{
    uint32_t ret = 0;

    ret = check_wifi_state();
    if (ret != 0) {
        printf("your wifi not connected!\r\n");
        return 0;
    }

    // xTaskCreate(example_mqtt,(char*)"test_mqtt", 8192, argv, 10, NULL);
    example_mqtt(argc, argv);

    return 0;
}

SHELL_CMD_EXPORT_ALIAS(cmd_mqtts_publisher, mqtts_pub, mqtts publisher);
#endif
