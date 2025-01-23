/* configure the CPU type */
OUTPUT_ARCH( "riscv" )

/* configure the entry point */
ENTRY(__start)

StackSize    = 0x1000; /*  4KB */
HeapMinSize  = 0x1000; /*  4KB */
psram_heap_min_size = 0x1000;
__EM_SIZE = DEFINED(btble_controller_init) ? 32K : 0K;
__LPFW_SIZE = DEFINED(btble_controller_init) ? 28K : 60K;

__RFTLV_SIZE_OFFSET = 1K;
__RFTLV_SIZE_HOLE = 2K;
__RFTLV_HEAD1_H = (0x46524C42); /* */
__RFTLV_HEAD1_L = (0x41524150); /* PAPA */

VerOffset = 0xc00;

MEMORY
{
    fw_header_memory  (rx)  : ORIGIN = 0xA0000000 - 0x1000, LENGTH = 4K
    xip_memory  (rx)  : ORIGIN = 0xA0000000, LENGTH = 4M
    ram_psram  (wxa)   : ORIGIN = 0xA8000000, LENGTH = 4M
    ram_nocache (!rx) : ORIGIN = 0x22FC0400, LENGTH = 320K - 1K     /* reserved 1k for security */
    ram_memory  (!rx) : ORIGIN = 0x62FC0400, LENGTH = 320K - 1K     /* reserved 1k for security */
    ram_wifi  (wxa)   : ORIGIN = 0x23010000, LENGTH = 160K - __EM_SIZE - __LPFW_SIZE
    ram_lp  (wxa)     : ORIGIN = 0x63029000, LENGTH = __LPFW_SIZE
    lp_dumy_flag (rw) : ORIGIN = 0xC0000000, LENGTH = 0x40
    hbn_memory  (rx)  : ORIGIN = 0x20010000, LENGTH = 0x200

}

SECTIONS
{

    BOOT2_PT_ADDR = 0x63027c00;

    .fw_header :
    {
        KEEP(*(.fw_header))
    } > fw_header_memory

    .init :
    {
        KEEP (*(SORT_NONE(.init)))
        KEEP (*(SORT_NONE(.vector)))

    } > xip_memory

    .rftlv.tool :
    {
        . = ORIGIN(xip_memory) + __RFTLV_SIZE_OFFSET;
        PROVIDE( _ld_symbol_rftlv_address = . );
        LONG(__RFTLV_HEAD1_H);
        LONG(__RFTLV_HEAD1_L);
        . = ORIGIN(xip_memory) + __RFTLV_SIZE_OFFSET + __RFTLV_SIZE_HOLE;
    } > xip_memory

    .ver : AT(ORIGIN(xip_memory) + VerOffset)
    {
        LONG(0x42464c42);
        LONG(0x46524556);
        KEEP (*(.qcc74xverinf))
        LONG(0x42464c42);
        LONG(0x46524556);
        KEEP (*(.verinfo))
    } > xip_memory


    . = ALIGN(32);
    __nocache_ram_load_addr = .;

    .nocache_ram_region  : AT (__nocache_ram_load_addr)
    {
        . = ALIGN(32);
        __nocache_ram_data_start__ = .;

        *(.nocache_ram)

        . = ALIGN(32);
        __nocache_ram_data_end__ = .;
    } > ram_nocache

    .nocache_noinit_ram_region (NOLOAD) :
    {
        . = ALIGN(32);
        __nocache_noinit_ram_data_start__ = .;

        *(.nocache_noinit_ram)
        *(.noncacheable)

        . = ALIGN(32);
        __nocache_noinit_ram_data_end__ = .;
    } > ram_nocache

    /* Skip the nocache region */ 
    .ram_skip_nocache_region (NOLOAD) :
    {
        . += (__nocache_noinit_ram_data_end__ - ORIGIN(ram_nocache));
    } > ram_memory

    __itcm_load_addr = __nocache_ram_load_addr + SIZEOF(.nocache_ram_region);

    .itcm_region : AT (__itcm_load_addr)
    {
        . = ALIGN(32);
        __tcm_code_start__ = .;
        *libc.a:lib_a-memcpy.c.o*(.text)       
        *libc.a:lib_a-memcpy.c.o*(.rodata)     
        *libc.a:lib_a-memcpy-asm.c.o*(.text)   
        *libc.a:lib_a-memcpy-asm.c.o*(.rodata) 
        *libc.a:lib_vikmemcpy.c.o*(.text*)     
        *libc.a:lib_vikmemcpy.c.o*(.rodata*)   
        *libc.a:lib_a-memset.c.o*(.text)       
        *libc.a:lib_a-memset.c.o*(.rodata)     
        *libc.a:lib_memset.c.o*(.text*)        
        *libc.a:lib_memset.c.o*(.rodata*)      
        *libgcc.a:_clzsi2.o*(.text)            
        *libgcc.a:_clzsi2.o*(.rodata)          
        *libgcc.a:_clz.o*(.text)               
        *libgcc.a:_clz.o*(.rodata)             

        INCLUDE run_in_function_high_performance.ld.in 
        
        *(.text.GLB_Set_SPI_CLK
          .text.GLB_Swap_MCU_SPI_0_MOSI_With_MISO
          .text.qcc74x_dma_channel_stop
          .text.GLB_AHB_MCU_Software_Reset
          .text.qcc74x_spi_init
          .text.qcc74x_spi_link_txdma
          .text.qcc74x_dma_channel_init
          .text.qcc74x_dma_channel_irq_attach
          .text.qcc74x_dma_channel_tcint_mask
          .text.qcc74x_gpio_init
          .text.qcc74x_irq_disable
          .text.qcc74x_gpio_int_init
          .text.qcc74x_gpio_int_mask
          .text.qcc74x_gpio_int_clear
          .text.qcc74x_gpio_get_intstatus
          .text.qcc74x_irq_clear_pending
          .text.qcc74x_irq_enable
          .text.csi_vic_disable_irq
          .text.csi_vic_enable_irq
          .text.qcc74x_spi_link_rxdma
          .text.csi_vic_clear_pending_irq
        )
        *at_main.c.o*(.text.at_main_task)
        *at_main.c.o*(.rodata.at_main_task)
        *at_main.c.o*(.text.at_response_string)
        *at_main.c.o*(.rodata.at_response_string)
        *at_main.c.o*(.text.at_response_result)
        *at_main.c.o*(.rodata.at_response_result)
        *at_main.c.o*(.text.at_workq_send)
        *at_main.c.o*(.rodata.at_workq_send)
        *at_main.c.o*(.text.at_workq_dowork)
        *at_main.c.o*(.rodata.at_workq_dowork)
        *at_net_cmd.c.o*(.text.at_setup_cmd_cipsend)
        *at_net_cmd.c.o*(.rodata.at_setup_cmd_cipsend)
        *at_net_cmd.c.o*(.text.at_setup_cmd_cipsendl)
        *at_net_cmd.c.o*(.rodata.at_setup_cmd_cipsendl)
        *at_net_cmd.c.o*(.text.at_setup_cmd_ciprecvdata)
        *at_net_cmd.c.o*(.rodata.at_setup_cmd_ciprecvdata)
        *at_core.c.o*(.text*)
        *at_core.c.o*(.rodata*)
        *at_net_main.c.o*(.text.at_net_client_is_connected)
        *at_net_main.c.o*(.rodata.at_net_client_is_connected)
        *at_net_main.c.o*(.text.at_net_client_send_async)
        *at_net_main.c.o*(.rodata.at_net_client_send_async)
        *at_net_main.c.o*(.text.at_net_recvbuf_size_get)
        *at_net_main.c.o*(.rodata.at_net_recvbuf_size_get)
        *at_net_main.c.o*(.text.at_net_client_get_recvsize)
        *at_net_main.c.o*(.rodata.at_net_client_get_recvsize)
        *at_net_main.c.o*(.text.at_net_recvbuf_read)
        *at_net_main.c.o*(.rodata.at_net_recvbuf_read)
        *qcc74x_gpio.c.o*(.text.qcc74x_gpio_set)
        *qcc74x_gpio.c.o*(.text.qcc74x_gpio_read)
        *qcc74x_gpio.c.o*(.text.qcc74x_gpio_reset)
        *qcc743_gpio.c.o*(.text.qcc74x_gpio_pad_check)
        *qcc743_gpio.c.o*(.text.gpio_all_isr)
        *qcc743_gpio.c.o*(.rodata.gpio_all_isr)
        *qcc743_gpio.c.o*(.rodata.qcc74x_gpio_read)
        *lib_strcasecmp.c.o*(.text*)
        *lib_strcasecmp.c.o*(.rodata*)
        *lib_atoi.c.o*(.text*)
        *lib_atoi.c.o*(.rodata*)
        *lib_a-strlen.o*(.text*)
        *lib_a-strlen.o*(.rodata*)
        *lib_a-strncat.o*(.text*)
        *lib_a-strncat.o*(.rodata*)
        *lib_itoa.c.o*(.text*)
        *lib_itoa.c.o*(.rodata*)
        *lib_a-strncmp.o*(.text*)
        *lib_a-strncmp.o*(.rodata*)
        *lib_a-strlcpy.o*(.text*)
        *lib_a-strlcpy.o*(.rodata*)
        *vsnprintf.c.o*(.text*)
        *vsnprintf.c.o*(.rodata*)
        *snprintf.c.o*(.text*)
        *snprintf.c.o*(.rodata*)
        *mem.c.o*(.text.malloc)
        *mem.c.o*(.text.free)
        *qcc74x_tlsf.c.o*(.text*)
        *qcc74x_tlsf.c.o*(.rodata*)
        *tlsf.c.o*(.text*)
        *tlsf.c.o*(.rodata*)
       
        *qcc74x_dma.c.o*(.text.dma0_isr)
        *qcc74x_dma.c.o*(.text.dma1_isr)
        *qcc74x_dma.c.o*(.text.dma2_isr)
        *qcc74x_dma.c.o*(.text.qcc74x_dma_channel_lli_reload)
        *qcc74x_dma.c.o*(.rodata.qcc74x_dma_channel_lli_reload)
        *qcc74x_dma.c.o*(.text.qcc74x_dma_channel_start)
        *qcc74x_dma.c.o*(.rodata.qcc74x_dma_channel_start)
        *qcc74x_dma.c.o*(.text.qcc74x_dma_lli_config)
        *qcc74x_dma.c.o*(.rodata.qcc74x_dma_lli_config)
        *qcc74x_gpio.c.o*(.text.gpio_all_isr)
        *qcc74x_gpio.c.o*(.rodata.gpio_all_isr)
        *vector.S.o*(.text*)
        *vector.S.o*(.rodata*)
        *interrupt.c.o*(.text*)
        *interrupt.c.o*(.rodata*)
        *libfreertos.a:*.o*(SORT_BY_NAME(.rodata*))   
        *libfreertos.a:portASM.S.o*(.rodata*)   
        *libfreertos.a:portASM.S.o*(.text*)   
        
        *(.tcm_code.*)
        *(.tcm_const.*)
        *(.sclock_rlt_code.*)
        *(.sclock_rlt_const.*)

        *qcc743_glb*.o*(.rodata*)
        *qcc743_pds*.o*(.rodata*)
        *qcc743_common*.o*(.rodata*)
        *qcc743_sf_cfg*.o*(.rodata*)
        *qcc743_sf_ctrl*.o*(.rodata*)
        *qcc743_sflash*.o*(.rodata*)
        *qcc743_xip_sflash*.o*(.rodata*)
        *qcc743_ef_ctrl*.o*(.rodata*)
        *qcc743_romapi_patch*.o*(.rodata*)

        . = ALIGN(32);
        __tcm_code_end__ = .;
    } > ram_memory

    __dtcm_load_addr = __itcm_load_addr + SIZEOF(.itcm_region);

    .dtcm_region : AT (__dtcm_load_addr)
    {
        . = ALIGN(32);
        __tcm_data_start__ = .;

        *(.tcm_data)
        /* *finger_print.o(.data*) */

        . = ALIGN(32);
        __tcm_data_end__ = .;
    } > ram_memory

    __ram_load_addr = __dtcm_load_addr + SIZEOF(.dtcm_region);

    /* Data section */
    RAM_DATA : AT (__ram_load_addr)
    {
        . = ALIGN(32);
        __ram_data_start__ = .;

        PROVIDE( __global_pointer$ = . + 0x800 );

        *libqcc74x_macsw*.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *libqcc74x_fhost*.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *libqcc74x_os_adapter.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *liblwip.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *libwifi6_lwip_adapter.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *libqcc743_phyrf.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))
        *libm.a:*.o(SORT_BY_NAME(.data.* ) SORT_BY_NAME(.sdata.*))
        *libc.a:*.o(SORT_BY_NAME(.data.* ) SORT_BY_NAME(.sdata.*))
        *libgcc.a:*.o(SORT_BY_NAME(.data.* ) SORT_BY_NAME(.sdata.*))
        *libatmodule.a:*.o*(SORT_BY_NAME(.data*) SORT_BY_NAME(.sdata*))

        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        *(.sdata2)
        *(.sdata2.*)

        . = ALIGN(32);
        *(._k_queue.static.*)
        *(._k_sem.static.*)
        *(._k_mutex.static.*)
        _bt_gatt_service_static_list_start = .;
        KEEP(*(SORT_BY_NAME("._bt_gatt_service_static.static.*")))
        _bt_gatt_service_static_list_end = .;
        _bt_l2cap_fixed_chan_list_start = .;
        KEEP(*(SORT_BY_NAME("._bt_l2cap_fixed_chan.static.*")))
        _bt_l2cap_fixed_chan_list_end = .;
        _bt_l2cap_br_fixed_chan_list_start = .;
        KEEP(*(SORT_BY_NAME("._bt_l2cap_br_fixed_chan.static.*")))
        _bt_l2cap_br_fixed_chan_list_end = .;

        . = ALIGN(32);
        __qcc74xlog_tags_start__ = .;
        *(.qcc74xlog_tags_array)
        . = ALIGN(32);
        __qcc74xlog_tags_end__ = .;
        __ram_data_end__ = .;
    } > ram_memory

    __hbn_load_addr = __ram_load_addr + SIZEOF (RAM_DATA);

    .hbn_ram_region : AT (__hbn_load_addr)
    {
        . = ALIGN(32);
        __hbn_ram_start__ = .;

        KEEP(*(.hbn_code.*))
        KEEP(*(.hbn_data.*))

        . = ALIGN(32);
        __hbn_ram_end__ = .;
    } > hbn_memory

    __psram_load_addr = __hbn_load_addr + SIZEOF(.hbn_ram_region);

    .psram_data_region : AT (__psram_load_addr)
    {
        . = ALIGN(32);
        __psram_data_start__ = .;

        KEEP(*(.psram_data*))

        . = ALIGN(32);
        __psram_data_end__ = .;
    } > ram_psram

    .stack (NOLOAD):
    {
        __StackLimit = .;
        . = ALIGN(16);
        . = . + StackSize;
        __StackTop = .;
        __freertos_irq_stack_top = .;
    } > ram_memory

    .high_stack (NOLOAD):                          
    {                                              
      . = ALIGN(16);                               
      . = . + StackSize;                           
      __freertos_high_irq_stack_top = .;           
    } > ram_memory                                 

    PROVIDE( __freertos_irq_stack_top = __StackTop);
    PROVIDE( _sp_main = __StackTop);
    PROVIDE( _sp_base = __StackLimit);
 
    .wifibss (NOLOAD) :
    {
      PROVIDE( __wifi_bss_start = ADDR(.wifibss) );
      PROVIDE( __wifi_bss_end = ADDR(.wifibss) + SIZEOF(.wifibss) );
      _sshram = . ;
      *(SHAREDRAMIPC)
      *(SHAREDRAM)
      *mem.c.o*(.bss.ram_heap)
      _eshram = . ;
      *ipc_shared.o*(COMMON)
      *sdu_shared.o*(COMMON)
      *hal_desc.o*(COMMON)
      *txl_buffer_shared.o*(COMMON)
      *txl_frame_shared.o*(COMMON)
      *scan_shared.o*(COMMON)
      *scanu_shared.o*(COMMON)
      *mfp_bip.o*(COMMON)
      *me_mic.o*(COMMON)
      *(.wifi_ram*)
      . = ALIGN(16);
    } > ram_wifi

    PROVIDE( _heap_wifi_start = . );
    PROVIDE( _heap_wifi_size = ORIGIN(ram_wifi) + LENGTH(ram_wifi) - _heap_wifi_start );

    .bss (NOLOAD) :
    {
        . = ALIGN(32);
        __bss_start__ = .;

        *libqcc74x_macsw*.a:*.o*(SORT_BY_NAME(.bss*))
        *libqcc74x_fhost*.a:*.o*(SORT_BY_NAME(.bss*))
        *libqcc74x_os_adapter.a:*.o*(SORT_BY_NAME(.bss*))
        *liblwip.a:*.o*(SORT_BY_NAME(.bss*))
        *libwifi6_lwip_adapter.a:*.o*(SORT_BY_NAME(.bss*))
        *libqcc743_phyrf.a:*.o*(SORT_BY_NAME(.bss*))
        *libm.a:*.o(SORT_BY_NAME(.bss*))
        *libc.a:*.o(SORT_BY_NAME(.bss*))
        *libgcc.a:*.o(SORT_BY_NAME(.bss*))
        *libatmodule.a:*.o*(SORT_BY_NAME(.bss*))

        *(.bss*)
        *(.sbss*)
        *(COMMON)

        . = ALIGN(32);
        __bss_end__ = .;
    } > ram_memory

    .noinit_data (NOLOAD) :
    {
        . = ALIGN(32);
        __noinit_data_start__ = .;

        *(.noinit_data*)

        . = ALIGN(32);
        __noinit_data_end__ = .;
    } > ram_memory

    .heap (NOLOAD):
    {
        . = ALIGN(32);
        __HeapBase = .;

        KEEP(*(.heap*))

        . = ALIGN(32);
        __HeapLimit = .;
    } > ram_memory

    .lp_mon (NOLOAD)   :
    {
        PROVIDE( __lp_mon_start = ADDR(.lp_mon) );
        PROVIDE( __lp_mon_end = ADDR(.lp_mon) + SIZEOF(.lp_mon) );
        *(.lp_mon_ctx)
    } > ram_memory

    __HeapLimit = ORIGIN(ram_memory) + LENGTH(ram_memory);
    ASSERT(__HeapLimit - __HeapBase >= HeapMinSize, "heap region overflow")

    .psram_noinit_data (NOLOAD):
    {
        . = ALIGN(32);
        __psram_noinit_data_start__ = .;

        KEEP(*(.psram_noinit*))
        KEEP(*(.img_buf*))

        . = ALIGN(32);
        __psram_noinit_data_end__ = .;
    } > ram_psram

    .psram_heap (NOLOAD):
    {
        . = ALIGN(32);
        __psram_heap_base = .;
        KEEP(*(.psram_heap*))
        . = ALIGN(32);
        __psram_heap_end = .;
    } > ram_psram

    __psram_limit = ORIGIN(ram_psram) + LENGTH(ram_psram);
    ASSERT(__psram_limit - __psram_heap_base >= psram_heap_min_size, "psram heap region overflow")

    __ram_lp_load_addr = (__psram_load_addr + SIZEOF (.psram_data_region));

    .ramlp : AT (__ram_lp_load_addr)
    {
        . = ALIGN(32);
        __ram_lp_code_start__ = .;
        *(.ram_lp*)
        *utils_list.c.o*(.text*)
        *utils_list.c.o*(.rodata*)
        *libspisync.a:*.o*(.text*)
        *libspisync.a:*.o*(.rodata*)

        *libnxspi.a:nxspi.c.o*(.text*)
        *libnxspi.a:nxspi.c.o*(.rodata*)
        *libnxspi.a:nxspi_port.c.o*(.text*)
        *libnxspi.a:nxspi_port.c.o*(.rodata*)

        *at_port_spi.c.o*(.text*)
        *at_port_spi.c.o*(.rodata*)
  
        __ram_lp_code_end__ = .;
        . = ALIGN(32);
    } > ram_lp 

    __text_load_addr = (__ram_lp_load_addr + SIZEOF (.ramlp));

    .text __text_load_addr : AT(__text_load_addr)
    {
        . = ALIGN(32);
        __text_code_start__ = .;

        *libqcc74x_macsw*.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libqcc74x_fhost*.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libqcc74x_os_adapter.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *liblwip.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libwifi6_lwip_adapter.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libqcc743_phyrf.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libm.a:*.o(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libc.a:*.o(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libgcc.a:*.o(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))
        *libatmodule.a:*.o*(SORT_BY_NAME(.text*) SORT_BY_NAME(.rodata*) SORT_BY_NAME(.srodata.*))

        *(.text)
        *(.text.*)
        *(.romwifi_code*)

        /* section information for shell */
        . = ALIGN(32);
        __fsymtab_start = .;
        KEEP(*(FSymTab))
        __fsymtab_end = .;

        /* section information for components's version */
        . = ALIGN(4);
        _version_info_section_start = .;
        KEEP(*(.version.*))
        _version_info_section_end = .;

        . = ALIGN(32);
        __vsymtab_start = .;
        KEEP(*(VSymTab))
        __vsymtab_end = .;

        /* section information for usb usbh_class_info */
        . = ALIGN(32);
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        . = ALIGN(32);
        __usbh_class_info_end__ = .;

        /*put .rodata**/
        *(EXCLUDE_FILE( *qcc743_glb*.o* \
                        *qcc743_pds*.o* \
                        *qcc743_common*.o* \
                        *qcc743_sf_cfg*.o* \
                        *qcc743_sf_ctrl*.o* \
                        *qcc743_sflash*.o* \
                        *qcc743_xip_sflash*.o* \
                        *qcc743_ef_ctrl*.o* \
                        *qcc743_romapi_patch*.o* ).rodata*)

        *(.srodata)
        *(.srodata.*)

        /* _dump_sections for qcc74x_coredump.c*/
        . = ALIGN(32);
        PROVIDE( _dump_sections = ABSOLUTE(.));
        LONG(ORIGIN(ram_memory));
        LONG(LENGTH(ram_memory));
        LONG(ORIGIN(ram_nocache));
        LONG(LENGTH(ram_nocache));
        LONG(ORIGIN(ram_wifi));
        LONG(LENGTH(ram_wifi));
        /*
        LONG(ORIGIN(ram_psram));
        LONG(LENGTH(ram_psram));
         */
        LONG(0x28010000); /* bt memory */
        LONG(__EM_SIZE);
        LONG(0xffffffff);
        LONG(0xffffffff);
        /* _dump_sections for qcc74x_coredump.c*/

        . = ALIGN(32);
        __text_code_end__ = .;
    } > xip_memory

    .build_id :
    {
        . = ALIGN(4);
        PROVIDE( elf_build_id = ABSOLUTE(.) + 16);
        *(.note.gnu.build-id)
    } > xip_memory
    

    . = ALIGN(32);
    __etext_final = (__text_load_addr + SIZEOF (.text) + SIZEOF(.build_id));
    ASSERT(__etext_final <= ORIGIN(xip_memory) + LENGTH(xip_memory), "code memory overflow")
    __lpfw_load_addr = __etext_final;

    /*.lpfw : AT (__lpfw_load_addr)*/
    .lpfw :
    {
        /* For DMA Burst copy lpfw binary from flash to ram */
        . = ALIGN(32);
        LONG(0x0);        /* LPFW SHA256 */
        . = ALIGN(32);
        LONG(0x5746504C); /* LPFW */
        . = ALIGN(32);
        PROVIDE( __lpfw_start = . );
        ASSERT(((ABSOLUTE(__lpfw_start) & 0x1F) == 0), "LPFW not aligned 32!");
    } > xip_memory


    PROVIDE(__binary_length = __lpfw_start - ORIGIN(xip_memory));


    PROVIDE(__LD_CONFIG_EM_SEL = __EM_SIZE);
}

