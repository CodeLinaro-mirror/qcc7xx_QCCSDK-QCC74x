################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

LWIP_SRCS := \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/acd.c \
../../../Middlewares/Third_Party/LwIP/src/core/altcp.c \
../../../Middlewares/Third_Party/LwIP/src/core/altcp_alloc.c \
../../../Middlewares/Third_Party/LwIP/src/core/altcp_tcp.c \
../../../Middlewares/Third_Party/LwIP/src/api/api_lib.c \
../../../Middlewares/Third_Party/LwIP/src/api/api_msg.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/auth.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/autoip.c \
../../../Middlewares/Third_Party/LwIP/src/netif/bridgeif.c \
../../../Middlewares/Third_Party/LwIP/src/netif/bridgeif_fdb.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/ccp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/chap-md5.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/chap-new.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/chap_ms.c \
../../../Middlewares/Third_Party/LwIP/src/core/def.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/demand.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/dhcp.c \
../../../Middlewares/Third_Party/LwIP/src/apps/lwip_dhcpd/dhcp_server_raw.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/dhcp6.c \
../../../Middlewares/Third_Party/LwIP/src/core/dns.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/eap.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/ecp.c \
../../../Middlewares/Third_Party/LwIP/src/api/err.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/etharp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ethernet.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/ethip6.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/eui64.c \
../../../Middlewares/Third_Party/LwIP/src/apps/http/fs.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/fsm.c \
../../../Middlewares/Third_Party/LwIP/src/apps/http/httpd.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/icmp.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/icmp6.c \
../../../Middlewares/Third_Party/LwIP/src/api/if_api.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/igmp.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/inet6.c \
../../../Middlewares/Third_Party/LwIP/src/core/inet_chksum.c \
../../../Middlewares/Third_Party/LwIP/src/core/init.c \
../../../Middlewares/Third_Party/LwIP/src/core/ip.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/ip4.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_addr.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4/ip4_frag.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/ip6.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_addr.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/ip6_frag.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/ipcp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/ipv6cp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/lcp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/lowpan6.c \
../../../Middlewares/Third_Party/LwIP/src/netif/lowpan6_ble.c \
../../../Middlewares/Third_Party/LwIP/src/netif/lowpan6_common.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/magic.c \
../../../Middlewares/Third_Party/LwIP/src/core/mem.c \
../../../Middlewares/Third_Party/LwIP/src/core/memp.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/mld6.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/mppe.c \
../../../Middlewares/Third_Party/LwIP/src/apps/mqtt/mqtt.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/multilink.c \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6/nd6.c \
../../../Middlewares/Third_Party/LwIP/src/api/netbuf.c \
../../../Middlewares/Third_Party/LwIP/src/api/netdb.c \
../../../Middlewares/Third_Party/LwIP/src/core/netif.c \
../../../Middlewares/Third_Party/LwIP/src/api/netifapi.c \
../../../Middlewares/Third_Party/LwIP/src/core/pbuf.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/ppp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/pppapi.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/pppcrypt.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/pppoe.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/pppol2tp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/pppos.c \
../../../Middlewares/Third_Party/LwIP/src/core/raw.c \
../../../Middlewares/Third_Party/LwIP/src/netif/slipif.c \
../../../Middlewares/Third_Party/LwIP/src/api/sockets.c \
../../../Middlewares/Third_Party/LwIP/src/core/stats.c \
../../../Middlewares/Third_Party/LwIP/src/core/sys.c \
../../../Middlewares/Third_Party/LwIP/system/OS/sys_arch.c \
../../../Middlewares/Third_Party/LwIP/src/core/tcp.c \
../../../Middlewares/Third_Party/LwIP/src/core/tcp_in.c \
../../../Middlewares/Third_Party/LwIP/src/core/tcp_out.c \
../../../Middlewares/Third_Party/LwIP/src/api/tcpip.c \
../../../Middlewares/Third_Party/LwIP/src/core/timeouts.c \
../../../Middlewares/Third_Party/LwIP/src/core/udp.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/upap.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/utils.c \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp/vj.c \
../../../Middlewares/Third_Party/LwIP/src/netif/zepif.c

C_SRCS += $(LWIP_SRCS)

OBJS += $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.o)))

C_DEPS += $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.d)))

vpath %.c \
../../../Middlewares/Third_Party/LwIP/src/api \
../../../Middlewares/Third_Party/LwIP/src/apps/http \
../../../Middlewares/Third_Party/LwIP/src/apps/lwip_dhcpd \
../../../Middlewares/Third_Party/LwIP/src/apps/mqtt \
../../../Middlewares/Third_Party/LwIP/src/core \
../../../Middlewares/Third_Party/LwIP/src/core/ipv4 \
../../../Middlewares/Third_Party/LwIP/src/core/ipv6 \
../../../Middlewares/Third_Party/LwIP/src/netif \
../../../Middlewares/Third_Party/LwIP/src/netif/ppp \
../../../Middlewares/Third_Party/LwIP/system/OS


# Each subdirectory must supply rules for building sources it contributes
Middlewares/LwIP/%.o Middlewares/LwIP/%.su Middlewares/LwIP/%.cyclo: %.c Middlewares/LwIP/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32H7S3xx -c -I../../../Appli/Inc -I../../../iperf -I../../../RCP -I../../../RCP/modem/include -I../../../RCP/transport/sdio/include -I../../../RCP/stream/wifi/include -I../../../RCP/stream/net/include -I../../../RCP/stream/bt/include -I../../../RCP/stream/tty/include -I../../../rcp_protocol/include -I../../../rcp_protocol/include/port -I../../../rcp_protocol/include/wifi -I../../../Middlewares/Third_Party/LwIP/src/include -I../../../Middlewares/Third_Party/LwIP/system -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc -I../../../Drivers/STM32H7RSxx_HAL_Driver/Inc/Legacy -I../../../Middlewares/Third_Party/FreeRTOS/Source/include -I../../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F -I../../../Drivers/CMSIS/RTOS2/Include -I../../../Middlewares/Third_Party/LwIP/src/include/netif/ppp -I../../../Middlewares/Third_Party/LwIP/src/apps/http -I../../../Middlewares/Third_Party/LwIP/src/apps/lwip_dhcpd -I../../../Drivers/CMSIS/Device/ST/STM32H7RSxx/Include -I../../../Middlewares/Third_Party/LwIP/src/include/lwip -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/apps -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/priv -I../../../Middlewares/Third_Party/LwIP/src/include/lwip/prot -I../../../Middlewares/Third_Party/LwIP/src/include/netif -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/arpa -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/net -I../../../Middlewares/Third_Party/LwIP/src/include/compat/posix/sys -I../../../Middlewares/Third_Party/LwIP/src/include/compat/stdc -I../../../Middlewares/Third_Party/LwIP/system/arch -I../../../Drivers/CMSIS/Include -I../../../BleApp/App -I../../../BleApp/Target -I../../../Middlewares/ST/STM32_WPAN/ble/include -I../../../Middlewares/ST/STM32_WPAN/ble/include/auto -I../../../Middlewares/ST/STM32_WPAN/ble/Interfaces -Ofast -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Middlewares-2f-LwIP

clean-Middlewares-2f-LwIP:
	-$(RM) $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.cyclo))) $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.d))) $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.o))) $(addprefix ./Middlewares/LwIP/,$(notdir $(LWIP_SRCS:.c=.su)))

.PHONY: clean-Middlewares-2f-LwIP
