
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "https_fota.h"

const char ca_cert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDcTCCAlmgAwIBAgIUcWUm8jVNxpvTBgXPBEJdhDvP1AYwDQYJKoZIhvcNAQEL\n"
    "BQAwQDEdMBsGA1UEAwwUQm91ZmZhbG8gT1RBIFRlc3QgQ0ExETAPBgNVBAoMCEJv\n"
    "dWZmYWxvMQwwCgYDVQQLDANPVEEwHhcNMjYwNDExMTA0NDI1WhcNMzYwNDA4MTA0\n"
    "NDI1WjBAMR0wGwYDVQQDDBRCb3VmZmFsbyBPVEEgVGVzdCBDQTERMA8GA1UECgwI\n"
    "Qm91ZmZhbG8xDDAKBgNVBAsMA09UQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC\n"
    "AQoCggEBALYEX4vreaIMDwWEvR3eB+c9EiA6/bdBymQZDFMIoc1DphTskZNi55Fv\n"
    "MC6cwMGSKnTcNQ6OgXYnEeWhNHrzu8E3e8bSfM0/ElWH2ooqxGNy4g5ZaJewlIhq\n"
    "fpsRwByfhneEsnxR9LoD7QaDCN+FGziv3kvDC1gFiVbCHxOxUAhhyyk7gjA6wZr1\n"
    "xrUc4yg86JAKR6dfhU8qzZP+URUN7/Ftdg8K+l+AZOYI2l5xOEcVewGD8G8y1vu1\n"
    "INERgmk0gxahbnyznl6Kh0pzqTqcirOufbpCF7IFXTBciBuNsVLms7mCoPrsMAxj\n"
    "tEfZmaC2y1tbycLqc1SA/6oTRPhBFmkCAwEAAaNjMGEwDwYDVR0TAQH/BAUwAwEB\n"
    "/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYEFCHQY0eaMvT69MCafwE0TFFehNvg\n"
    "MB8GA1UdIwQYMBaAFCHQY0eaMvT69MCafwE0TFFehNvgMA0GCSqGSIb3DQEBCwUA\n"
    "A4IBAQCl9GfXX5B5nu4tbtDevztTldsrgJA+WuWUhRkj4JwJRVaNkjAMgVJnK5Gu\n"
    "PyU4G621K1n7SkTWE3cGr/sl5MomXMXY+l+4d8Y6Vh0HbRXf8WmBmDL59NTvTV+6\n"
    "DiqpsA6HBZpyAZmFSxGjEmnr28LkBQfQiBiLrmDPjSNZnpPJMzU2SnNkVon4d5TW\n"
    "appergWniOiblmVBdg4ioMuHzH4ezFE1X7fME2FNwMq3FoG36tDmjpOx+B9fV8GR\n"
    "Ubt4cSN1pHd+WY7lcS6ZUqM09cxi4QxVRHZQuWrV+LvhYanh8Dd61YNsUrdt/XfO\n"
    "ugs11OvPXb26kSRAOON3AxXR/zTr\n"
    "-----END CERTIFICATE-----\n";

static void __ota_status_cb(void *arg, https_fota_status_t event)
{
	uint8_t *ota_progress = (uint8_t *)arg;

    switch (event) {
        case HTTPS_FOTA_START:
        printf("HTTPS_FOTA_START\r\n");
        break;

        case HTTPS_FOTA_SERVER_CONNECTE_FAIL:
        *ota_progress = 0;
        printf("HTTPS_FOTA_SERVER_CONNECTE_FAIL\r\n");
        break;

        case HTTPS_FOTA_PROCESS_TRANSFER:
        printf("HTTPS_FOTA_PROCESS_TRANSFER\r\n");
        break;

        case HTTPS_FOTA_TRANSFER_FINISH:
        printf("HTTPS_FOTA_TRANSFER_FINISH\r\n");
        break;

        case HTTPS_FOTA_IMAGE_VERIFY:
        printf("HTTPS_FOTA_IMAGE_VERIFY\r\n");
        break;

        case HTTPS_FOTA_IMAGE_VERIFY_FAIL:
        *ota_progress = 0;
        printf("HTTPS_FOTA_IMAGE_VERIFY_FAIL\r\n");
        break;

        case HTTPS_FOTA_SUCCESS:
        printf("HTTPS_FOTA_SUCCESS\r\n");
        *ota_progress = 0;
        qcc74x_sys_reset_por();
        break;

        case HTTPS_FOTA_ABORT:
        *ota_progress = 0;
        printf("HTTPS_FOTA_ABORT\r\n");
        break;

        default:
        break;
    }
}

int cmd_wifi_ota_test(int argc, char **argv)
{
	int ret;
    char *url;
    static uint8_t ota_progress = 0;

    struct https_fota_config config = {0};

    /* default cmd like this: qcc74x />wifi_ota_test http://192.168.31.112:5000/build/build_out/wifi_ota_qcc743.bin.ota */
	if (argc <= 1) {
		return -1;
	}
	if (ota_progress) {
		printf("OTA upgrade in progress\r\n");
		return -1;
	}

    ota_progress = 1;
    url = argv[1];

    config.callback = __ota_status_cb;
    config.user_arg = &ota_progress;
    config.ca_pem = ca_cert;
    config.ca_len = sizeof(ca_cert);
	ret = https_fota(url, &config);

	if (ret < 0) {
		printf("https_fota fail\r\n");
	}

    return 0;
}

int cmd_wifi_ota_rollback(int argc, char **argv)
{
	https_ota_rollback();
	return 0;
}

int cmd_dns_local_hostadd(int argc, char **argv)
{
    ip_addr_t addr;

    if ((argc != 3) || !ipaddr_aton(argv[2], &addr)) {
        return -1;
    }

    dns_local_removehost(argv[1], NULL);
    if (dns_local_addhost(argv[1], &addr) != ERR_OK) {
        return -1;
    }

    printf("DNS_LOCAL_HOSTADD_OK: %s %s\r\n", argv[1], argv[2]);
    return 0;
}

#ifdef CONFIG_SHELL
#include <shell.h>

SHELL_CMD_EXPORT_ALIAS(cmd_wifi_ota_test, wifi_ota_test, wifi ota test);
SHELL_CMD_EXPORT_ALIAS(cmd_wifi_ota_rollback, wifi_ota_rollback, wifi ota rollback test);
SHELL_CMD_EXPORT_ALIAS(cmd_dns_local_hostadd, dns_local_hostadd, add DNS local host entry);
#endif
