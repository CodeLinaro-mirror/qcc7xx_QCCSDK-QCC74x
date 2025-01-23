#include <FreeRTOS.h>
#include <task.h>
#include <event_groups.h>
#include <timers.h>
#include <stdio.h>
#include <string.h>


#include "ring_buffer.h"
#include "mfg_main.h"
#include "queue.h"
#include "rfparam_adapter.h"
#include "sdk_version.h"
#if defined(MFG_QCC743)
#include "qcc743_hbn.h"
#include "qcc743_glb.h"
#endif

#include <qcc74x_gpio.h>
#include <qcc74x_uart.h>

#include "log.h"
#include "qcc743_gpio.h"
#include "qcc743_clock.h"

extern const qcc74xverinf_t app_ver;

#define UART1_BAUDRATE (2000000)

static struct qcc74x_device_s *uart0;


extern uint8_t _heap_start;
extern uint8_t _heap_size;

static HeapRegion_t xHeapRegions[] =
{
        { &_heap_start,  (unsigned int) &_heap_size}, //set on runtime
        { NULL, 0 }, /* Terminates the array. */
        { NULL, 0 }, /* Terminates the array. */
        { NULL, 0 } /* Terminates the array. */
};

EventGroupHandle_t xEventGroupMFG;

void phycli_intc_isr(int irq, void *arg)
{
    phycli_intc_irq();
}

void phycli_intc_disable(void)
{
    qcc74x_irq_clear_pending(WIFI_IRQn);
    qcc74x_irq_disable(WIFI_IRQn);
}

void phycli_intc_enable(void)
{
    qcc74x_irq_clear_pending(WIFI_IRQn);
    qcc74x_irq_attach(WIFI_IRQn, phycli_intc_isr, NULL);
    qcc74x_irq_enable(WIFI_IRQn);
}

#if defined(BUILD_ROM_CODE)
static Ring_Buffer_Type uartRB;
static uint8_t uartBuf[128];
#else
void user_vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName )
{
    /*empty*/
}
#endif

#if defined(BUILD_ROM_CODE)
void vApplicationMallocFailedHook(void)
#else
void user_vApplicationMallocFailedHook(void)
#endif
{
    /*empty*/
}

#if defined(BUILD_ROM_CODE)
void vApplicationIdleHook(void)
#else
void user_vApplicationIdleHook(void)
#endif
{
    __asm volatile(
            "   wfi     "
    );
    /*empty*/
}

#if defined(BUILD_ROM_CODE)
void vApplicationSleep(TickType_t xExpectedIdleTime)
#else
void user_vApplicationSleep(TickType_t xExpectedIdleTime)
#endif
{
    /*empty*/
}

#if defined(BUILD_ROM_CODE)
void vApplicationTickHook( void )
#else
void user_vApplicationTickHook( void )
#endif
{
    /*empty*/
}

#if defined(BUILD_ROM_CODE)
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
#else
void user_vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
#endif
{
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
#if defined(BUILD_ROM_CODE)
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
#else
void user_vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize)
#endif
{
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static - otherwise they will be allocated on
    the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#if defined(BUILD_ROM_CODE)
void vAssertCalled(void)
{
    volatile uint32_t ulSetTo1ToExitFunction = 0;

    taskDISABLE_INTERRUPTS();
    while( ulSetTo1ToExitFunction != 1 ) {
        __asm volatile( "NOP" );
    }
}
#endif

void user_vAssertCalled(void)
{
    volatile uint32_t ulSetTo1ToExitFunction = 0;

    taskDISABLE_INTERRUPTS();
    while( ulSetTo1ToExitFunction != 1 ) {
        __asm volatile( "NOP" );
    }
}

#if !defined(BUILD_ROM_CODE)
static void __update_rom_api(void)
{
    struct romapi_freertos_map *romapi_freertos;

    romapi_freertos = hal_sys_romapi_get();

    romapi_freertos->vApplicationGetTimerTaskMemory = user_vApplicationGetTimerTaskMemory;
    romapi_freertos->vApplicationMallocFailedHook = user_vApplicationMallocFailedHook;
    romapi_freertos->vApplicationTickHook = user_vApplicationTickHook;
    romapi_freertos->vAssertCalled = user_vAssertCalled;
    romapi_freertos->vApplicationGetIdleTaskMemory = user_vApplicationGetIdleTaskMemory;
    romapi_freertos->vApplicationStackOverflowHook = user_vApplicationStackOverflowHook;
    romapi_freertos->vApplicationSleep = user_vApplicationSleep;
    romapi_freertos->vApplicationIdleHook = user_vApplicationIdleHook;

    hal_sys_romapi_update(romapi_freertos);
}
#endif

#if defined(MFG_PSRAM_TEST)
#include "psram_reg.h"

#include "qcc743_psram.h"
#include "qcc743_ef_ctrl.h"
#include "ef_data_reg.h"
#include "qcc743_gpio.h"
#include "qcc743_glb.h"
#include "qcc743_glb_gpio.h"

#define HAL_BOOT2_PSRAM_ID1_WINBOND_4MB  (0x5f)
#define HAL_BOOT2_PSRAM_ID2_WINBOND_32MB (0xe86)

/****************************************************************************/ /**
 * @brief  init psram gpio
 *
 * @param
 *
 * @return
 *
*******************************************************************************/
static void _qcc74x_init_psram_gpio(void)
{
    GLB_GPIO_Cfg_Type cfg;

    cfg.pullType = GPIO_PULL_NONE;
    cfg.drive = 0;
    cfg.smtCtrl = 1;

    for (uint8_t i = 0; i < 12; i++) {
        cfg.gpioPin = 41 + i;
        cfg.gpioMode = GPIO_MODE_INPUT;

        GLB_GPIO_Init(&cfg);
    }
}

/****************************************************************************/ /**
 * @brief  psram_winbond_init_dqs
 *
 * @param
 *
 * @return
 *
*******************************************************************************/
static uint16_t psram_winbond_init_dqs(int8_t burst_len, uint8_t is_fixLatency, uint8_t latency, uint16_t dqs_delay)
{
    uint16_t reg_read = 0;
    PSRAM_Ctrl_Cfg_Type psramCtrlCfg = {
        .vendor = PSRAM_CTRL_VENDOR_WINBOND,
        .ioMode = PSRAM_CTRL_X8_MODE,
        .size = PSRAM_SIZE_32MB,
        .dqs_delay = 0xffc0,
    };

    PSRAM_Winbond_Cfg_Type winbondCfg = {
        .rst = DISABLE,
        .clockType = PSRAM_CLOCK_DIFF,
        .inputPowerDownMode = DISABLE,
        .hybridSleepMode = DISABLE,
        .linear_dis = ENABLE,
        .PASR = PSRAM_PARTIAL_REFRESH_FULL,
        .disDeepPowerDownMode = ENABLE,
        .fixedLatency = DISABLE,
        .brustLen = PSRAM_WINBOND_BURST_LENGTH_64_BYTES,
        .brustType = PSRAM_WRAPPED_BURST,
        .latency = PSRAM_WINBOND_6_CLOCKS_LATENCY,
        .driveStrength = PSRAM_WINBOND_DRIVE_STRENGTH_35_OHMS_FOR_4M_115_OHMS_FOR_8M,
    };

    winbondCfg.brustLen = burst_len;
    winbondCfg.fixedLatency = is_fixLatency;
    winbondCfg.latency = latency;

    psramCtrlCfg.dqs_delay = dqs_delay;
    PSram_Ctrl_Init(PSRAM0_ID, &psramCtrlCfg);
    // PSram_Ctrl_Winbond_Reset(PSRAM0_ID);
    PSram_Ctrl_Winbond_Write_Reg(PSRAM0_ID, PSRAM_WINBOND_REG_CR0, &winbondCfg);

    PSram_Ctrl_Winbond_Read_Reg(PSRAM0_ID, PSRAM_WINBOND_REG_ID0, &reg_read);
    return reg_read;
}

typedef unsigned long ul;
typedef unsigned long long ull;
typedef unsigned long volatile ulv;
typedef unsigned char volatile u8v;
typedef unsigned short volatile u16v;
// typedef unsigned int off_t;

struct test {
    char *name;
    int (*fp)();
};
#define rand32() ((unsigned int)rand() | ((unsigned int)rand() << 16))

// #if (ULONG_MAX == 4294967295UL)
#if __riscv_xlen == 32
#define rand_ul()     rand32()
#define UL_ONEBITS    0xffffffff
#define UL_LEN        32
#define CHECKERBOARD1 0x55555555
#define CHECKERBOARD2 0xaaaaaaaa
#define UL_BYTE(x)    ((x | x << 8 | x << 16 | x << 24))
// #elif (ULONG_MAX == 18446744073709551615ULL)
#elif __riscv_xlen == 64
#define rand64()      (((ul)rand32()) << 32 | ((ul)rand32()))
#define rand_ul()     rand64()
#define UL_ONEBITS    0xffffffffffffffffUL
#define UL_LEN        64
#define CHECKERBOARD1 0x5555555555555555
#define CHECKERBOARD2 0xaaaaaaaaaaaaaaaa
#define UL_BYTE(x)    (((ul)x | (ul)x << 8 | (ul)x << 16 | (ul)x << 24 | (ul)x << 32 | (ul)x << 40 | (ul)x << 48 | (ul)x << 56))
#else
#error long on this platform is not 32 or 64 bits
#endif

#define EXIT_FAIL_NONSTARTER   0x01
#define EXIT_FAIL_ADDRESSLINES 0x02
#define EXIT_FAIL_OTHERTEST    0x04

char progress[] = "-\\|/";
#define PROGRESSLEN   4
#define PROGRESSOFTEN 2500
#define ONE           0x00000001L

int use_phys = 0;
off_t physaddrbase = 0;
int fail_stop = 1;

union {
    unsigned char bytes[UL_LEN / 8];
    ul val;
} mword8;

union {
    unsigned short u16s[UL_LEN / 16];
    ul val;
} mword16;

int compare_regions(ulv *bufa, ulv *bufb, size_t count)
{
    int r = 0;
    size_t i;
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    off_t physaddr;

    for (i = 0; i < count; i++, p1++, p2++) {
        if (*p1 != *p2) {
            if (use_phys) {
                physaddr = physaddrbase + (i * sizeof(ul));
                mfg_print("FAILURE: 0x%08x != 0x%08x at physical address "
                    "0x%08x.\n",
                    (ul)*p1, (ul)*p2, physaddr);
                // fprintf(stderr,
                // "FAILURE: 0x%08lx != 0x%08lx at physical address "
                // "0x%08lx.\n",
                // (ul)*p1, (ul)*p2, physaddr);
            } else {
                mfg_print("FAILURE: 0x%08x != 0x%08x at offset 0x%08x.\n",
                    (ul)*p1, (ul)*p2, (ul)(i * sizeof(ul)));
                // fprintf(stderr,
                // "FAILURE: 0x%08lx != 0x%08lx at offset 0x%08lx.\n",
                // (ul)*p1, (ul)*p2, (ul)(i * sizeof(ul)));
            }
            /* mfg_print("Skipping to next test..."); */
            r = -1;
        }
    }
    return r;
}

int test_stuck_address(ulv *bufa, size_t count)
{
    ulv *p1 = bufa;
    unsigned int j;
    size_t i;
    off_t physaddr;

    for (j = 0; j < 16; j++) {
        p1 = (ulv *)bufa;
        // mfg_print("p1:%08x %08x\r\n", p1, *p1);
        // mfg_print("setting %3u", j);
        for (i = 0; i < count; i++) {
            *p1 = ((j + i) % 2) == 0 ? (ul)p1 : ~((ul)p1);
            *p1++;
        }
        // mfg_print("testing %3u", j);

        p1 = (ulv *)bufa;
        for (i = 0; i < count; i++, p1++) {
            if (*p1 != (((j + i) % 2) == 0 ? (ul)p1 : ~((ul)p1))) {
                if (use_phys) {
                    physaddr = physaddrbase + (i * sizeof(ul));
                    // mfg_print("FAILURE: possible bad address line at physical address: 0x%08lx\r\n", physaddr);
                    // fprintf(stderr,
                    // "FAILURE: possible bad address line at physical "
                    // "address 0x%08lx.\n",
                    // physaddr);
                } else {
                    // fprintf(stderr,
                    // "FAILURE: possible bad address line at offset "
                    // "0x%08lx.\n",
                    // (ul)(i * sizeof(ul)));
                }
                // mfg_print("Skipping to next test...\n");

                return -1;
            }
        }
    }

    return 0;
}

int test_random_value(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    ul j = 0;
    size_t i;

    // mfg_print("*p %08x, %08x\r\n", *p1, *p2);
    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = rand_ul();
        if (!(i % PROGRESSOFTEN)) {
            // putchar(progress[++j % PROGRESSLEN]);
        }
    }
    return compare_regions(bufa, bufb, count);
}
int test_xor_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ ^= q;
        *p2++ ^= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_sub_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ -= q;
        *p2++ -= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_mul_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ *= q;
        *p2++ *= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_div_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        if (!q) {
            q++;
        }
        *p1++ /= q;
        *p2++ /= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_or_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ |= q;
        *p2++ |= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_and_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ &= q;
        *p2++ &= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_seqinc_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    size_t i;
    ul q = rand_ul();

    for (i = 0; i < count; i++) {
        *p1++ = *p2++ = (i + q);
    }
    return compare_regions(bufa, bufb, count);
}
int test_solidbits_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? UL_ONEBITS : 0;
        // printf("setting %3u", j);
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        // printf("testing %3u", j);

        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }

    return 0;
}

int test_checkerboard_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    ul q;
    size_t i;

    for (j = 0; j < 64; j++) {
        q = (j % 2) == 0 ? CHECKERBOARD1 : CHECKERBOARD2;
        // printf("setting %3u", j);
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
        }
        // printf("testing %3u", j);

        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }

    return 0;
}

int test_blockseq_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    for (j = 0; j < 256; j++) {
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        // printf("setting %3u", j);
        for (i = 0; i < count; i++) {
            *p1++ = *p2++ = (ul)UL_BYTE(j);
        }
        // printf("testing %3u", j);

        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }

    return 0;
}

int test_walkbits0_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        // printf("setting %3u", j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = ONE << j;
            } else { /* Walk it back down. */
                *p1++ = *p2++ = ONE << (UL_LEN * 2 - j - 1);
            }
        }
        // printf("testing %3u", j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }

    return 0;
}

int test_walkbits1_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        // printf("setting %3u", j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << j);
            } else { /* Walk it back down. */
                *p1++ = *p2++ = UL_ONEBITS ^ (ONE << (UL_LEN * 2 - j - 1));
            }
        }
        // printf("testing %3u", j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }

    return 0;
}

int test_bitspread_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j;
    size_t i;

    for (j = 0; j < UL_LEN * 2; j++) {
        p1 = (ulv *)bufa;
        p2 = (ulv *)bufb;
        // printf("setting %3u", j);
        for (i = 0; i < count; i++) {
            if (j < UL_LEN) { /* Walk it up. */
                *p1++ = *p2++ = (i % 2 == 0) ? (ONE << j) | (ONE << (j + 2)) : UL_ONEBITS ^ ((ONE << j) | (ONE << (j + 2)));
            } else { /* Walk it back down. */
                *p1++ = *p2++ = (i % 2 == 0) ? (ONE << (UL_LEN * 2 - 1 - j)) | (ONE << (UL_LEN * 2 + 1 - j)) : UL_ONEBITS ^ (ONE << (UL_LEN * 2 - 1 - j) | (ONE << (UL_LEN * 2 + 1 - j)));
            }
        }
        // printf("testing %3u", j);
        if (compare_regions(bufa, bufb, count)) {
            return -1;
        }
    }
    return 0;
}

int test_bitflip_comparison(ulv *bufa, ulv *bufb, size_t count)
{
    ulv *p1 = bufa;
    ulv *p2 = bufb;
    unsigned int j, k;
    ul q;
    size_t i;

    for (k = 0; k < UL_LEN; k++) {
        q = ONE << k;
        for (j = 0; j < 8; j++) {
            q = ~q;
            // printf("setting %3u", k * 8 + j);
            p1 = (ulv *)bufa;
            p2 = (ulv *)bufb;
            for (i = 0; i < count; i++) {
                *p1++ = *p2++ = (i % 2) == 0 ? q : ~q;
            }
            // printf("testing %3u", k * 8 + j);
            if (compare_regions(bufa, bufb, count)) {
                return -1;
            }
        }
    }

    return 0;
}

struct test tests[] = {
    { "Random Value", test_random_value },
    { "Compare XOR", test_xor_comparison },
    { "Compare SUB", test_sub_comparison },
    { "Compare MUL", test_mul_comparison },
    { "Compare DIV", test_div_comparison },
    { "Compare OR", test_or_comparison },
    { "Compare AND", test_and_comparison },
    { "Sequential Increment", test_seqinc_comparison },
    { "Solid Bits", test_solidbits_comparison },
    { "Block Sequential", test_blockseq_comparison },
    { "Checkerboard", test_checkerboard_comparison },
    { "Bit Spread", test_bitspread_comparison },
    { "Bit Flip", test_bitflip_comparison },
    { "Walking Ones", test_walkbits1_comparison },
    { "Walking Zeroes", test_walkbits0_comparison },
// #ifdef TEST_NARROW_WRITES
//     { "8-bit Writes", test_8bit_wide_random },
//     { "16-bit Writes", test_16bit_wide_random },
// #endif
    { NULL, NULL }
};

int memtester_main(ul phystestbase, ul wantraw, char *memsuffix, ul loops, ul pagesize, ul test_num)
{
    uint32_t loop, i;
    size_t wantmb, wantbytes, wantbytes_orig, bufsize, halflen, count;

    void volatile *buf, *aligned;
    ulv *bufa, *bufb;

    int exit_code = 0;
    int /*memfd, opt,*/ memshift;
    size_t maxbytes = -1;                /* addressable memory, in bytes */
    size_t maxmb = (maxbytes >> 20) + 1; /* addressable memory, in MB */
    /* Device to mmap memory from with -p, default is normal core */

    ul testmask = 0;
    physaddrbase = phystestbase;

    switch (*memsuffix) {
        case 'G':
        case 'g':
            memshift = 30; /* gigabytes */
            break;
        case 'M':
        case 'm':
            memshift = 20; /* megabytes */
            break;
        case 'K':
        case 'k':
            memshift = 10; /* kilobytes */
            break;
        case 'B':
        case 'b':
            memshift = 0; /* bytes*/
            break;
        case '\0':         /* no suffix */
            memshift = 20; /* megabytes */
            break;
        default:
            /* bad suffix */
            mfg_print("ERR!!! memsuffix input not B|K|M|G  \r\n");
            goto __MMETESTER_EXIT__;
            /* doesn't return */
    }
    /*set use_phys*/
    use_phys = 1;
    wantbytes_orig = wantbytes = ((size_t)wantraw << memshift);
    wantmb = (wantbytes_orig >> 20);

    if (wantmb > maxmb) {
        // fprintf(stderr, "This system can only address %llu MB.\n", (ull)maxmb);
        mfg_print("This system can only address %u MB.\r\n", maxmb);
        goto __MMETESTER_EXIT__;
    }

    // mfg_print("want %lluMB (%llu bytes)\r\n", (ull)wantmb, (ull)wantbytes);
    buf = NULL;

    bufsize = wantbytes; /* accept no less */
    buf = (unsigned int *)physaddrbase;
    aligned = buf;

    halflen = bufsize / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *)aligned;
    bufb = (ulv *)((size_t)aligned + halflen);
    // mfg_print("aligned %08x, *aligned %08x\r\n", aligned, *(ulv *)aligned);

    for (loop = 1; ((!loops) || loop <= loops); loop++) {
        // mfg_print("Loop %lu", loop);
        if (loops) {
            // mfg_print("/%lu", loops);
        }
        // mfg_print(":\r\n");
        // mfg_print("  %-20s: ", "Stuck Address");
        if (!test_stuck_address(aligned, bufsize / sizeof(ul))) {
            // mfg_print("ok\r\n");
        } else {
            exit_code |= EXIT_FAIL_ADDRESSLINES;
            if (fail_stop)
                break;
        }
        for (i = 0; i < test_num; i++) {
            if (!tests[i].name)
                break;
            /* If using a custom testmask, only run this test if the
               bit corresponding to this test was set by the user.
             */
            if (testmask && (!((1 << i) & testmask))) {
                continue;
            }
            // mfg_print("bufa %08x, bufb %08x %08x %08x\r\n", bufa, bufb, *bufa, *bufb);
            // mfg_print("  %-20s: ", tests[i].name);
            if (!tests[i].fp(bufa, bufb, count)) {
                // mfg_print("ok\r\n");
            } else {
                exit_code |= EXIT_FAIL_OTHERTEST;
                if (fail_stop)
                    break;
            }
            /* clear buffer */
            memset((void *)buf, 255, wantbytes);
        }
        mfg_print("\r\n");
    }

    if (exit_code) {
        mfg_print("Done and Failed!\r\n");
        return exit_code;
    } else
        // mfg_print("Done and Passed!\r\n");
    // mfg_print("exit_code 0x%x \r\n", exit_code);

__MMETESTER_EXIT__:
    mfg_print("\r\n");
    return 0;
}
#define PSRAM_BASIC_ADDR (0xA8000000)
#define TEST_MEMORY_SIZE (1 * 1024 * 1024)
#define MEMTESTER_SIZE   (4 * 1024 * 1024)

uint32_t test_pattern_val[] = {
    0xaa55aa55,
    0x33cc33cc,
    0x55aa55aa,
    0xcc33cc33,
    0xff00ff00,
    0x00ff00ff,
    0x99669966,
    0x66996699,
};

/****************************************************************************/ /**
 * @brief  psram_rw_check
 *
 * @param  None
 *
 * @return int8_t
 *
*******************************************************************************/
int8_t psram_rw_check(void)
{
    uint32_t i = 0;
    volatile uint32_t *p = (uint32_t *)PSRAM_BASIC_ADDR;
    volatile uint32_t q = 0;

    p[0] = 0xaa55aa55;
    p[1] = 0x33cc33cc;
    p[2] = 0x55aa55aa;
    p[3] = 0xcc33cc33;
    p[4] = 0xff00ff00;
    p[5] = 0x00ff00ff;
    p[6] = 0x99669966;
    p[7] = 0x66996699;

    __ISB();

    for (i = 0; i < 8; i++) {
        q = p[i];
        if (q != test_pattern_val[i]) {
            mfg_print("p 0x%x RW ERROR 0x%x != 0x%x\r\n", p, i, q);
            return -1;
        }
    }

    // char memsuffix = 'B';
    // // mfg_print("start memtester ...\r\n");
    // if(0 != memtester_main(0xA8000000, 0x10000, &memsuffix, 0x1, (1 * 1024)))
    // {
    //     mfg_print("memtester failed!\r\n");
    //     return -1;
    // }
    /* clear PSRAM 1M Byte */
    for (i = 0; i < TEST_MEMORY_SIZE; i++) {
        p[i] = 0xffffffff;
    }
    /* check PSRAM 1M Byte */
    for (i = 0; i < TEST_MEMORY_SIZE; i++) {
        p[i] = i;
    }
    for (i = 0; i < TEST_MEMORY_SIZE; i++) {
        q = p[i];
        if (q != i) {
            mfg_print("RW ERROR 0x%x != 0x%x\r\n", i, q);
            return -1;
        }
    }
    return 0;
}

int memtest_psram_init(int32_t *psram_dqs_win_num, int test_num)
{
    int16_t psram_id = 0, before_ef = 0;
    int32_t left_flag = 0, right_flag = 0, c_val = 0;
    int32_t dqs_win_min = 16, dqs_win_max = 0;
    uint16_t dqs_val[] = {
        0x8000,
        0xC000,
        0xE000,
        0xF000,
        0xF800,
        0xFC00,
        0xFE00,
        0xFF00,
        0xFF80,
        0xFFC0,
        0xFFE0,
        0xFFF0,
        0xFFF8,
        0xFFFC,
        0xFFFE,
        0xFFFF,
    };

    EF_Ctrl_Sw_AHB_Clk_0();
    EF_Ctrl_Load_Efuse_R0();
    uint32_t psram_trim = 0, psram_t_en = 0, psram_t_parity = 0;
    psram_trim = *(uint32_t *)0x200560E8;
    psram_t_en = (psram_trim >> 12) & 0x1;
    psram_t_parity = (psram_trim >> 11) & 0x1;
    mfg_print("efuse trim: 0x%x\r\n", (psram_trim & 0x7ff));

    mfg_print("start c ef...\r\n");
    for (uint32_t dqs_index = 0; dqs_index < 16; dqs_index++) {
        psram_id = psram_winbond_init_dqs(PSRAM_WINBOND_BURST_LENGTH_64_BYTES, 0, PSRAM_WINBOND_6_CLOCKS_LATENCY, dqs_val[dqs_index]);
        // #if (!CONFIG_BUILD_TYPE)
        // mfg_print("dqs:0x%04x;read psram id:0x%x\r\n", dqs_val[dqs_index], psram_id);
        // #endif
        if ((psram_id == HAL_BOOT2_PSRAM_ID1_WINBOND_4MB) || (psram_id == HAL_BOOT2_PSRAM_ID2_WINBOND_32MB)) {
            if (psram_rw_check() == SUCCESS) {
                if (dqs_index < dqs_win_min) {
                    dqs_win_min = dqs_index;
                    dqs_win_max = dqs_index;
                } else if (dqs_index > dqs_win_max) {
                    dqs_win_max = dqs_index;
                }
            }
        }
    }

    left_flag = dqs_win_min;
    right_flag = dqs_win_max;
    c_val = ((left_flag + right_flag) >> 1);
    mfg_print("window: 0x%02x ~ 0x%02x; c_val: 0x%02x; dqs:0x%04x; code num:%d\r\n", left_flag, right_flag, c_val, dqs_val[c_val], (right_flag - left_flag));

    // g_efuse_cfg.psram_dqs_cfg = (((left_flag << 0x4) | (right_flag)) & (0xff));

    // mfg_print("c ef:0x%08lx\r\n", g_efuse_cfg.psram_dqs_cfg);
    /* mfg_print("window: 0x%02x ~ 0x%02x; c_val: 0x%02x; dqs:0x%08x; code num:%d\r\n", left_flag, right_flag, c_val, dqs_val[c_val], (right_flag - left_flag)); */

    *psram_dqs_win_num = right_flag - left_flag;
    if (((*psram_dqs_win_num) <= 4) || ((*psram_dqs_win_num) > 0xf)) {
        return -1;
    }
    psram_id = psram_winbond_init_dqs(PSRAM_WINBOND_BURST_LENGTH_64_BYTES, 0, PSRAM_WINBOND_6_CLOCKS_LATENCY, dqs_val[c_val]);
    if ((psram_id != HAL_BOOT2_PSRAM_ID1_WINBOND_4MB) && (psram_id != HAL_BOOT2_PSRAM_ID2_WINBOND_32MB)) {
        return -1;
    }
    mfg_print("dqs:0x%04x;read psram id:0x%x\r\n", dqs_val[c_val], psram_id);
    /* to do write efuse psram dqs delay */
    // if (!(before_ef & 0x1fff)) {
        // qcc74x_ef_ctrl_write_common_trim(NULL,"psram",g_efuse_cfg.psram_dqs_cfg,1);
    // }
    char memsuffix = 'B';
    mfg_print("start memtester %d pattern...\r\n", test_num);
    if (memtester_main(PSRAM_BASIC_ADDR, MEMTESTER_SIZE, &memsuffix, 1, (1 * 1024), test_num) != 0)
    // if (psram_rw_check() != SUCCESS)
    {
        return -1;
    }
    mfg_print("memtester success done!\r\n");
    return 0;
}

int mfg_psram_test(int test_num)
{
    _qcc74x_init_psram_gpio();
    GLB_Set_PSRAMB_CLK_Sel(1, GLB_PSRAMB_EMI_WIFIPLL_320M, 0);

    uint32_t tmpVal;
    uint32_t psram_info = 0;
    int32_t psram_win = 0;

    /* Trigger read data from efuse */
    EF_Ctrl_Load_Efuse_R0();
    tmpVal = QCC74x_RD_REG(EF_DATA_BASE, EF_DATA_EF_WIFI_MAC_HIGH);
    psram_info = ((tmpVal & 0x3000000) >> 24);

    if (psram_info == 0x01) {
        memtest_psram_init(&psram_win, test_num);
    } else {
        mfg_print("This Chip None PSRAM!\r\n");
    }
    mfg_print("psram win:%d\r\n", psram_win);
    return psram_win;
}
#endif

static void system_init(void)
{
    *(volatile uint8_t*)(0x2000060c) = 0xFF;
}

uint32_t task_counter1 = 0;
uint32_t task_counter2 = 0;

static void proc_hellow_entry1(void *pvParameters)
{
    while (1) {
        task_counter1++;
        vTaskDelay(1000);
    }
    vTaskDelete(NULL);
}

static void proc_hellow_entry2(void *pvParameters)
{
    while (1) {
        task_counter2++;
        vTaskDelay(2000);
    }
    vTaskDelete(NULL);
}

extern int32_t mfg_cmd_input(uint8_t *data, uint16_t len, int from_isr);
void flush_cli_buf()
{
    Ring_Buffer_Reset(&uartRB);
}

void mfg_deal_raw_input(uint8_t *buf,uint32_t len, int from_isr)
{
    uint32_t i=0;

    buf[len]=0;
    Ring_Buffer_Write(&uartRB,buf,len);

    while(1){
        len=Ring_Buffer_Get_Length(&uartRB);
        if(len<=1){
            return ;
        }
        Ring_Buffer_Peek(&uartRB,buf,len);
        //buf[len]=0;
        //printf("peek:%s\r\n",buf);

        #if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
        if(buf[0] == BT_LE_HCI_CMD){
            if(len < BT_LE_HCI_CMD_HDR_LEN || len < buf[3] + BT_LE_HCI_CMD_HDR_LEN)
                return;
            else{
                Ring_Buffer_Read(&uartRB,buf,buf[3] + BT_LE_HCI_CMD_HDR_LEN);
                mfg_cmd_input(buf, buf[3] + BT_LE_HCI_CMD_HDR_LEN, from_isr);
                return;
            }
        }else{
        #endif
            for(i=0;i<len;i++){
                if(buf[i]=='\r'||buf[i]=='\n'){
                    Ring_Buffer_Read(&uartRB,buf,i+1);
                    buf[i+1]=0;
                    //printf("deal:%s\r\n",buf);
                    if(buf[0]=='\r'||buf[0]=='\n'){
                        mfg_cmd_input(buf+1, i, from_isr);
                    }else{
                        mfg_cmd_input(buf, i+1, from_isr);
                    }
                    break;
                }
            }
            if(i>=len){
                return;
            }
        #if defined(CONFIG_BLE_MFG_HCI_CMD) || defined(CONFIG_BT_MFG_HCI_CMD)
        }
        #endif
    }
}

struct qcc74x_device_s *uartx;

int uart_sample_get_data(uint8_t *data, uint32_t expect_size)
{
    int ch;
    uint32_t counter = 0;
    while (counter < expect_size) {
        ch = qcc74x_uart_getchar(uartx);
        if (ch < 0) {
            break;
        }
        ((uint8_t*)data)[counter] = ch;
        counter++;
    }

    return counter;
}

uint8_t uart_rxbuf[128+4];
void uart_isr(int irq, void *arg)
{
    uint16_t uart_rx_count = 0;

    uint32_t intstatus = qcc74x_uart_get_intstatus(uartx);
    #if 0
    uartx = qcc74x_device_get_by_name("uart0");
    #endif
    #if 0
    if (intstatus & UART_INTSTS_RX_FIFO) {
        while (qcc74x_uart_rxavailable(uartx)) {
            uart_rxbuf[uart_rx_count++] = qcc74x_uart_getchar(uartx);
        }

        mfg_deal_raw_input(uart_rxbuf, uart_rx_count, 1);
    }
    #endif

    if (intstatus & UART_INTSTS_RTO) {
        qcc74x_uart_int_clear(uartx, UART_INTCLR_RTO);
    #if 0
        while (qcc74x_uart_rxavailable(uartx)) {
            uart_rxbuf[uart_rx_count++] = qcc74x_uart_getchar(uartx);
        }

        mfg_deal_raw_input(uart_rxbuf, uart_rx_count, 1);
    #endif
    }
    while (qcc74x_uart_rxavailable(uartx)) {
        uart_rxbuf[uart_rx_count++] = qcc74x_uart_getchar(uartx);
    }
    if (uart_rx_count){
        mfg_deal_raw_input(uart_rxbuf, uart_rx_count, 1);
    }
}

void uart_gpio_sample_init(void)
{
    struct qcc74x_device_s *gpio;
    gpio = qcc74x_device_get_by_name("gpio");

    qcc74x_gpio_uart_init(gpio, GPIO_PIN_21, GPIO_UART_FUNC_UART0_TX);
    qcc74x_gpio_uart_init(gpio, GPIO_PIN_22, GPIO_UART_FUNC_UART0_RX);

    uartx = qcc74x_device_get_by_name("uart0");

    struct qcc74x_uart_config_s cfg;
    cfg.baudrate = 2000000;
    cfg.data_bits = UART_DATA_BITS_8;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.parity = UART_PARITY_NONE;
    cfg.bit_order = UART_LSB_FIRST;
    cfg.flow_ctrl = 0;
    cfg.tx_fifo_threshold = 7;
    cfg.rx_fifo_threshold = 7;
    qcc74x_uart_init(uartx, &cfg);

    qcc74x_uart_txint_mask(uartx, true);
    qcc74x_uart_rxint_mask(uartx, false);
    qcc74x_irq_attach(uartx->irq_num, uart_isr, NULL);
    qcc74x_irq_enable(uartx->irq_num);
}

static void btble_init_task_entry(void *pvParameters)
{
    btble_controller_init(configMAX_PRIORITIES - 1);
    vTaskDelete(NULL);
}

int isDigit(char c) {
    return (c >= '0' && c <= '9');
}

/* unsigned number*/
int stringToNumber(char *str) {
    int result = 0;
    int i = 0;
    for (; str[i] != '\0'; i++) {
        if (isDigit(str[i])) {
            result = result * 10 + (str[i] - '0');
        } else {
            break;
        }
    }
    return result;
}

static void mfg_dump_boot_info(void)
{
    puts("MFG Version: ");
    puts(PROJECT_SDK_VERSION);
#if 0
    uint8_t i;
    uint8_t tmp;
    uint8_t cnt = 0;
    while (PROJECT_SDK_VERSION[i] != '\0') {
        /* skip non number char */
        while (PROJECT_SDK_VERSION[i] != '\0' && !isDigit(PROJECT_SDK_VERSION[i])) {
            i++;
        }
        /* process the numbers */
        if (isDigit(PROJECT_SDK_VERSION[i])) {
            if (cnt == 0){
                //app_ver.x = stringToNumber(&PROJECT_SDK_VERSION[i]);
                cnt++;
            } else if (cnt == 1) {
                //app_ver.y = stringToNumber(&PROJECT_SDK_VERSION[i]);
                cnt++;
            } else if (cnt == 2) {
                //app_ver.z = stringToNumber(&PROJECT_SDK_VERSION[i]);
                cnt++;
            }
            /*else if (cnt == 3) {
                app_ver.rsvd0 = stringToNumber(&PROJECT_SDK_VERSION[i]);
                cnt++;
            }*/
        }
        /* search for next number */
        while (PROJECT_SDK_VERSION[i] != '\0' && isDigit(PROJECT_SDK_VERSION[i])) {
            i++;
        }
    }
#endif
    /* check if dirty
    if ((i > 4) && (PROJECT_SDK_VERSION[i-5] == 'd') && (PROJECT_SDK_VERSION[i-4] == 'i') && (PROJECT_SDK_VERSION[i-3] == 'r') && (PROJECT_SDK_VERSION[i-2] == 't') && (PROJECT_SDK_VERSION[i-1] == 'y')){
        app_ver.rsvd1 = 1;
    }
    */
#if ( PM_PDS_LDO_LEVEL_DEFAULT == 8 )
    puts("_dcdc\r\n");
#else
    puts("_ldo\r\n");
#endif
    puts("\r\n");
    puts("Build Date: ");
    puts(__DATE__);
    puts("\r\n");
    puts("Build Time: ");
    puts(__TIME__);
    puts("\r\n");
    puts("------------------------------------------------------------\r\n");
}



void _dump_media_ver(void)
{
    mfg_print("ver:\r\n");
    mfg_print("anti_rollback:%d\r\n", app_ver.anti_rollback);
    mfg_print("%d.%d.%d\r\n", app_ver.x, app_ver.y, app_ver.z);
    mfg_print("name:%s\r\n", (char *)app_ver.name);
    mfg_print("build time:%s\r\n", (char *)app_ver.build_time);
    mfg_print("commit_id:%s\r\n", (char *)app_ver.commit_id);
    mfg_print("rscv0:%d\r\n", app_ver.rsvd0);
    mfg_print("rscv1:%d\r\n", app_ver.rsvd1);
    mfg_print("------------------------------------------------------------\r\n");
}



void main()
{
    int ret;
    static StackType_t aos_loop_proc_stack[1024];
    static StaticTask_t aos_loop_proc_task;
    static StackType_t proc_hellow_stack1[512];
    static StaticTask_t proc_hellow_task1;
    static StackType_t proc_hellow_stack2[512];
    static StaticTask_t proc_hellow_task2;
	static StackType_t qcc74x_mfg_entry_stack[512];
    static StaticTask_t qcc74x_mfg_entry_task;

    static StackType_t qcc74x_ble_entry_stack[512];
    static StaticTask_t qcc74x_ble_entry_task;

    static StackType_t qcc74x_temp_trim_stack[1024];
    static StaticTask_t qcc74x_temp_trim_task;

    /* trim RC32k based on efuse's value */
    HBN_Trim_RC32K();

    board_init();
#if defined(MFG_LPGU)
    /* FIXME: move these to system init */
    //HBN_Set_Ldo11_Rt_Vout(0); //internal LDO 0.6V, use external DCDC
    //HBN_Set_Ldo11_Soc_Vout(0);
    extern void hal_pm_ldo11_use_ext_dcdc();
    hal_pm_ldo11_use_ext_dcdc();
    GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_PDS);
#endif

#if defined(MFG_RAM)
    GLB_Power_On_XTAL_And_PLL_CLK(MFG_XTAL_TYPE, GLB_PLL_WIFIPLL);
    GLB_Set_MCU_System_CLK(GLB_MCU_SYS_CLK_TOP_WIFIPLL_320M);
    GLB_Set_MCU_System_CLK_Div(0, 3);//320M clock div
    HBN_Set_MCU_XCLK_Sel(HBN_MCU_XCLK_XTAL);
    GLB_Set_UART_CLK(1, HBN_UART_CLK_XCLK, 0);

    //arch_delay_ms(200);
#endif
#if defined(MFG_HTOL)
    struct qcc74x_device_s *wdg;
    struct qcc74x_wdg_config_s wdg_cfg;
    wdg_cfg.clock_source = WDG_CLKSRC_32K;
    wdg_cfg.clock_div = 31;
    wdg_cfg.comp_val = 32000;
    wdg_cfg.mode = WDG_MODE_RESET;

    wdg = qcc74x_device_get_by_name("watchdog");
    qcc74x_wdg_init(wdg, &wdg_cfg);
    qcc74x_wdg_start(wdg);
#endif

#if defined(MFG_GU)
    GLB_Set_MCU_System_CLK_Div(3, 0);//320M clock div
    *((volatile uint32_t*)0x20000580) = 1;
    *((volatile uint32_t*)0x20000584) = (1<<17)|(1<<16)|(1<<11)|(1<<7);
    *((volatile uint32_t*)0x20000588) = 1<<4;
#endif
    CPU_Set_MTimer_CLK(1, QCC74x_MTIMER_SOURCE_CLOCK_MCU_XCLK, 39);

    Ring_Buffer_Init(&uartRB,uartBuf,sizeof(uartBuf),NULL,NULL);

    //qcc74x_uart_init(1, UART_TX_PIN, UART_RX_PIN, 255, 255, UART1_BAUDRATE);
    //uart_gpio_sample_init();
    mfg_dump_boot_info();
    //_dump_media_ver();


    //uart0 = qcc74x_device_get_by_name("uart0");
    //shell_init_with_task(uart0);

    if (0 != rfparam_init(0, NULL, 0)) {
        LOG_I("PHY RF init failed!\r\n");
        return 0;
    }

    LOG_I("PHY RF init success!\r\n");



#ifdef MFG_GU
    //ret = qcc74x_flash_init();
    mfg_print("flash init ret %d\r\n",ret);
#endif

#if defined(MFG_PSRAM_TEST)
    // mfg_print("PSRAM test\r\n");
    // mfg_psram_test(1);
    /* exception will occur,if power off psram clk before cache clean */
    // L1C_DCache_Clean_Invalid_All();
#endif
    system_init();

    Message_Queue= xQueueCreate(MFG_Q_NUM,MFG_CMD_MAX_LEN);
    if(Message_Queue==NULL){
        mfg_print("MFG Queue create fail\r\n");
    }
    for(int i=0;i<sizeof(phy_cli_list)/sizeof(phy_cli_list[0]);i++){
        phy_cli_list[i]=phy_cli_cmd[i];
    }

    rfparam_init(0,NULL,1);

    xEventGroupMFG = xEventGroupCreate();


    uart_gpio_sample_init();
    xEventGroupSetBits( xEventGroupMFG, (1 << 1) );

	puts("[OS] Starting MFG task...\r\n");
    xTaskCreateStatic(mfg_main, (char*)"mfg", 512, NULL, 14, qcc74x_mfg_entry_stack, &qcc74x_mfg_entry_task);
#ifdef CONFIG_BLE_MFG
    puts("[OS] Starting btble task...\r\n");
    xTaskCreateStatic(btble_init_task_entry, (char*)"bleinit", 512, NULL, 14, qcc74x_ble_entry_stack, &qcc74x_ble_entry_task);
#endif
    puts("[OS] Starting TCal task...\r\n");
    xTaskCreateStatic((TaskFunction_t)mfg_temp_trim, (char*)"temp_trim", sizeof(qcc74x_temp_trim_stack)/4, NULL, 15, qcc74x_temp_trim_stack, &qcc74x_temp_trim_task);

    puts("[OS] Starting OS Scheduler...\r\n");
    vTaskStartScheduler();
}
