#include "hbn_reg.h"
#include "pds_reg.h"
#include "glb_reg.h"
#include "qcc743_glb.h"
#include "qcc743_aon.h"

// #include "export/rwnx.h"
// #include "plf/refip/src/driver/rwnx_platform.h"
// #include "ip/lmac/src/rx/rxl/rxl_hwdesc.h"
// STA Info table
// #include "ip/lmac/src/mm/mm.h"

//#include "rwnx_platform.h"

#include "qcc74x_lp.h"

// #include "lpfw_printf.h"
#define ATTR_ROM_WIFI_SECTION __attribute__((section(".romwifi_code." ATTR_UNI_SYMBOL)))

#define WL_API_RMEM_ADDR      0x20010600

#define BEACON_SIZE_TH        (0x400 - 0xA0)

#define BEACON_SUBTYPE        0x80
#define SIZE_BEACON_SUBTYPE   1

#define TIM_IE_ID             5

/*  */
#define LP_ABS(a)       (((a) > 0)? (a) : (-(a)))
#define LP_DIFF(a, b)   (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))

typedef struct {
    volatile uint8_t ie_number;
    volatile uint8_t ie_length;
    volatile uint8_t ie_data[0];
} lp_fw_ie_t;


ATTR_ROM_WIFI_SECTION void lpfw_static_val_addr_init(uint32_t* addr)
{
    ((iot2lp_para_t *)IOT2LP_PARA_ADDR)->lpfw_static_val = (lp_fw_static_variable_t*) addr;
}

ATTR_ROM_WIFI_SECTION void lpfw_wifi_addr_init(   uint32_t buff_start_addr,
                            uint32_t beacon_start_addr,
                            uint32_t beacon_bssid_addr,
                            uint32_t beacon_fixed_part_addr,
                            uint32_t beacon_interval_part_addr,
                            uint32_t beacon_variable_part_addr)
{
    RAM_WIFI_START_ADD = buff_start_addr;
    ADDR_BEACON_START = beacon_start_addr;
    ADDR_BEACON_BSSID = beacon_bssid_addr;
    ADDR_BEACON_FIXED_PART = beacon_fixed_part_addr;
    ADDR_BEACON_INTERVAL_PART = beacon_interval_part_addr;
    ADDR_BEACON_VARIABLE_PART = beacon_variable_part_addr;
}

ATTR_ROM_WIFI_SECTION void lpfw_beacon_status_clear(void)
{
    /* Avoid simultaneous operation with MAC DMA */

    /* clear frame type */
    *((volatile uint32_t *)ADDR_BEACON_SUBTYPE) = 0;

    /* clear BSSID */
    *((volatile uint32_t *)ADDR_BEACON_BSSID) = 0;
    *((volatile uint32_t *)ADDR_BEACON_BSSID + 1) = 0;

    /* clear beacon interval */
    *(volatile uint16_t *)ADDR_BEACON_INTERVAL_PART = 0;
    __DSB();
}

ATTR_ROM_WIFI_SECTION int lpfw_check_beacon_interval(void)
{
    int bcn = *(volatile uint16_t *)ADDR_BEACON_INTERVAL_PART;

    /* if g_beacon_interval not equl 0, p_time_stamp is exactly ready */

    return bcn;
}

ATTR_ROM_WIFI_SECTION int lpfw_check_beacon_and_bssid(void)
{
    /* check beacon */
    if (*((volatile uint8_t *)ADDR_BEACON_SUBTYPE) != BEACON_SUBTYPE) {
        return 0;
    }

    /* check bssid */
    for (uint8_t i = 0; i < 6; i++) {
        if (iot2lp_para->bssid[i] != *(volatile uint8_t *)(ADDR_BEACON_BSSID + i)) {
            return 0;
        }
    }

    return 1;
}

ATTR_ROM_WIFI_SECTION volatile lp_fw_ie_t *lpfw_find_tim(uint32_t interval_start_mtimer, uint8_t beacon_rate_code)
{
    volatile lp_fw_ie_t *p_ie = (volatile lp_fw_ie_t *)ADDR_BEACON_VARIABLE_PART;
    uint32_t mtimer_now;
    uint32_t wait_dtim_us=0;

    while ((uint32_t)p_ie - ADDR_BEACON_START < BEACON_SIZE_TH) {
        /* nop() */
        __NOP();
        __NOP();
        __NOP();
        __NOP();

        /* Use time to get the amount of data that has been received */
        #ifdef SHARED_FUNC_EN
        mtimer_now = shared_cpu_get_mtimer_counter();
        #else
        mtimer_now = CPU_Get_MTimer_Counter();
        #endif
        if (mtimer_now - interval_start_mtimer < ((uint32_t)p_ie - ADDR_BEACON_FIXED_PART + 4) * 8 * 2 /  beacon_rate_code) {
            /* The data has not been refreshed before the time is up */
            continue;
        }

        if (p_ie->ie_length == 0) {
            if (p_ie->ie_number == 0 && (mtimer_now - interval_start_mtimer > ((uint32_t)p_ie - ADDR_BEACON_FIXED_PART + 16) * 8 * 2 / beacon_rate_code)) {
                /* When the BSSID is not broadcasted, the length of the BSSID can be considered as 0 */
            } else {
                continue;
            }
        }

        /* Move to the next ie */
        if (p_ie->ie_number != TIM_IE_ID) {
            p_ie = (void *)p_ie + p_ie->ie_length + 2;
            continue;
        }

        wait_dtim_us = (p_ie->ie_length * 8 * 2 /  beacon_rate_code);

        if(wait_dtim_us){
            /** for example, ie_length = 4
            * 1M    arch_delay_us(32) : 40us
            * 2M    arch_delay_us(16) : 24us
            * 5.5M  arch_delay_us(5) :  13us
            * 11M   arch_delay_us(3) :  10us
            */
            #ifdef SHARED_FUNC_EN
            shared_arch_delay_us(wait_dtim_us);
            #else
            arch_delay_us(wait_dtim_us);
            #endif
        } else {
            /* delay 1 us*/
            __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
            __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
            __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
            __NOP();__NOP();__NOP();__NOP();__NOP();__NOP();
        }

        /* return offset */
        return p_ie;
    }

    /* error */
    return NULL;
}

/* wifi gate */
ATTR_ROM_WIFI_SECTION void lpfw_wifi_clk_gate(void)
{
    uint32_t tmpVal = 0;
    tmpVal = QCC74x_RD_WORD(GLB_BASE + 0x830);
    tmpVal = QCC74x_SET_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV3);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV4);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV5);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV6);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV8);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV10);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV12);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV20);
    tmpVal = QCC74x_CLR_REG_BIT(tmpVal, GLB_WIFIPLL_EN_DIV30);
    QCC74x_WR_WORD(GLB_BASE + 0x830, tmpVal);
}

ATTR_ROM_WIFI_SECTION int lpfw_bcn_timestamp_check(uint64_t beacon_timestamp_now_us, uint64_t rtc_timestamp_now_us)
{
    uint64_t rtc_timestamp_last_us, beacon_timestamp_last_us;
    int64_t rtc_us, beacon_us;

    /* get last timestamp */
    rtc_timestamp_last_us = iot2lp_para->last_beacon_stamp_rtc_us;
    beacon_timestamp_last_us = iot2lp_para->last_beacon_stamp_beacon_us;

    /*  */
    if(iot2lp_para->last_beacon_stamp_rtc_valid == 0){
        return 1;
    }

    if (beacon_timestamp_now_us < beacon_timestamp_last_us) {
        /* The timestamp is abnormal. Abort */
        return -1;
    }

    /* Obtaining Duration */
    rtc_us = rtc_timestamp_now_us - rtc_timestamp_last_us;
    beacon_us = beacon_timestamp_now_us - beacon_timestamp_last_us;

    if ( beacon_us > 3 * 60 * 1000 * 1000 || rtc_us > 3 * 60 * 1000 * 1000) {
        /* The time span is too large. Abort */
        return -2;
    }

    return 0;
}

/* Calibrate RC32K and update the beacon timestamp */
ATTR_ROM_WIFI_SECTION int lpfw_recal_rc32k(uint64_t beacon_timestamp_now_us, uint64_t rtc_timestamp_now_us, uint32_t mode, int clock_ready_check)
{
    uint64_t rtc_timestamp_last_us, beacon_timestamp_last_us;
    int64_t rtc_us, beacon_us;
    int diff_us, diff_ppm;
    int ret = 0;

    /* get last timestamp */
    rtc_timestamp_last_us = iot2lp_para->last_rc32trim_stamp_rtc_us;
    beacon_timestamp_last_us = iot2lp_para->last_rc32trim_stamp_beacon_us;

    if(clock_ready_check && qcc74x_lp_get_32k_clock_ready() == 0) {
        /* wait 32k_clock ready */
        ret = -2;
        iot2lp_para->last_rc32trim_stamp_valid = 0;
        qcc74x_lp_set_32k_trim_ready(0);
        return ret;
    }

    if (iot2lp_para->last_rc32trim_stamp_valid == 0) {
        ret = -3;
        /* update timestamp */
        goto update_tsf;
    }

    if (iot2lp_para->rc32k_auto_cal_en == 0) {
        ret = -4;
        /* update timestamp */
        goto update_tsf;
    }

    if (rtc_timestamp_now_us < rtc_timestamp_last_us || beacon_timestamp_now_us < beacon_timestamp_last_us) {
        /* The timestamp is abnormal. Abort calibration */
        ret = -5;
        /* update timestamp */
        if(mode == BEACON_STAMP_APP){
            goto update_tsf;
        }else{
            return ret;
        }
    }

    /* Obtaining Duration */
    rtc_us = rtc_timestamp_now_us - rtc_timestamp_last_us;
    beacon_us = beacon_timestamp_now_us - beacon_timestamp_last_us;

    if (rtc_us > 5 * 1000 * 1000 || beacon_us > 5 * 1000 * 1000) {
        /* The time span is too large. Abort calibration */
        ret = 6;
        /* update timestamp */
        goto update_tsf;
    }

    if (rtc_us < 250 * 1000 || beacon_us < 250 * 1000) {
        /* Time is too short, accuracy is too low, abandon calibration */
        ret = 1;
        /* not update timestamp !!! */
        return ret;
    }

    /* Time difference */
    diff_us = rtc_us - beacon_us;
    diff_ppm = (int64_t)diff_us * 1000 * 1000 / beacon_us;

    if (LP_ABS(diff_ppm) > 10 * 1000) {
        /* The error is too large. Abort calibration */
        ret = -7;
        if(mode == BEACON_STAMP_APP){
            goto update_tsf;
        }else{
            return ret;
        }
    }

    if(clock_ready_check && qcc74x_lp_get_32k_trim_ready() == 0){
        /*  */
        ret = 2;
        if(LP_DIFF(iot2lp_para->rtc32k_error_ppm, diff_ppm) < 1500){
            /* error ppm diff < 1500, trim ready, (allow dtim sleep) */
            qcc74x_lp_set_32k_trim_ready(1);
            ret = 3;
        }
    }

    if (QCC74x_GET_REG_BITS_VAL(QCC74x_RD_REG(HBN_BASE, HBN_GLB), HBN_F32K_SEL)) {
        /* rtc use xtal32k, only soft recal */
        iot2lp_para->rtc32k_error_ppm = (diff_ppm * 1 + iot2lp_para->rtc32k_error_ppm * 7) / 8;

        /* update timestamp */
        goto update_tsf;

    } else {
        /* rtc32k error (ppm) */
        iot2lp_para->rtc32k_error_ppm = (diff_ppm * 2 + iot2lp_para->rtc32k_error_ppm * 6) / 8;

        if(iot2lp_para->rtc32k_error_ppm > 3000 || iot2lp_para->rtc32k_error_ppm < -3000){

            /* get rc32k recal code */
            uint32_t rc32k_reg = *((volatile uint32_t *)0x2000F200);
            uint32_t rc32k_code = rc32k_reg >> 22;

            if(iot2lp_para->rtc32k_error_ppm > 3000) {
                rc32k_code += 1;
                iot2lp_para->rtc32k_error_ppm -= 1800;

            }else if (iot2lp_para->rtc32k_error_ppm < -3000) {
                rc32k_code -= 1;
                iot2lp_para->rtc32k_error_ppm += 1800;
            }

            /* set rc32k code */
            rc32k_reg &= ~0xffc00000;
            rc32k_reg |= rc32k_code << 22;
            *((volatile uint32_t *)0x2000F200) = rc32k_reg;

            /* save the code */
            iot2lp_para->rc32k_fr_ext = rc32k_code;
        }

        /* update timestamp */
        goto update_tsf;
    }

update_tsf:
    /* update timestamp */
    iot2lp_para->last_rc32trim_stamp_rtc_us = rtc_timestamp_now_us;
    iot2lp_para->last_rc32trim_stamp_beacon_us = beacon_timestamp_now_us;
    iot2lp_para->last_rc32trim_stamp_valid = mode;
    return ret;
}

/* Take modulo of a 64-bit number, with the condition that the divisor should be less than 0x0fffffff. */
static uint64_t modulo64bit(uint64_t num, uint32_t divisor)
{
    uint32_t high = num >> 32; // Extract the higher 32 bits of the 64-bit number
    uint32_t low = (uint32_t)num; // Extract the lower 32 bits of the 64-bit number
    uint8_t bit_n = 32; // Initialize a counter to 32 bits

    /* Perform modulo operation on the higher 32 bits */
    high %= divisor;

    /* Merge the lower bits into the higher bits, 4 bits at a time, and perform modulo operation iteratively */
    do {
        bit_n -= 4;
        high = (high << 4) | ((low >> bit_n) & 0x0F);
        high %= divisor;
    } while (bit_n > 0);

    return high;
}

/* Bubble sort, store the sorted indices in indices */
static void bubble_sort(int32_t *arr, uint8_t *indices, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++) {
        indices[i] = i;
    }

    for (uint8_t i = 0; i < size - 1; i++) {
        /* Add a flag to indicate whether a swap operation was performed in this round */
        uint8_t flag = 0;
        /*  */
        for (uint8_t j = 0; j < size - i - 1; j++) {
            if (arr[indices[j]] > arr[indices[j + 1]]) {
                uint8_t temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
                /* Set the flag to 1, indicating a swap operation was performed */
                flag = 1;
            }
        }
        if (flag == 0) {
            /* If no swap operation was performed in this round, it means the array is already sorted,
            and we can terminate the sorting process early */
            break;
        }
    }
}

static ATTR_TCM_SECTION int32_t lpfw_beacon_delay_sliding_win_update(int32_t beacon_delay_us);

/* calculate_beacon_delay, and update timestamp */
ATTR_TCM_SECTION int32_t lpfw_calculate_beacon_delay(uint64_t beacon_timestamp_us, uint64_t rtc_timestamp_us, uint32_t mode)
{
    int32_t beacon_delay_us;
    uint32_t beacon_interval_us;

    /* clear continuous loss cnt */
    iot2lp_para->continuous_loss_cnt = 0;
    iot2lp_para->bcn_loss_level = 0;

    /* update timestamp_us */
    iot2lp_para->last_beacon_stamp_rtc_valid = mode;
    iot2lp_para->last_beacon_stamp_rtc_us = rtc_timestamp_us;
    iot2lp_para->last_beacon_stamp_beacon_us = beacon_timestamp_us;

    /* beacon interval us, dtim_num * TU * 1024 */
    beacon_interval_us = iot2lp_para->beacon_interval_tu * 1024;

    /* update beacon delay */
    beacon_delay_us = modulo64bit(beacon_timestamp_us, beacon_interval_us);

    if(iot2lp_para->bcn_delay_sliding_win_status < iot2lp_para->bcn_delay_sliding_win_size){
        iot2lp_para->last_beacon_delay_us = 0;
        iot2lp_para->bcn_delay_offset = 0;

        if(iot2lp_para->bcn_delay_sliding_win_status == iot2lp_para->bcn_delay_sliding_win_size - 1){
            lpfw_beacon_delay_sliding_win_update(beacon_delay_us);

            /* TODO: After recording 16 beacon times, calculate the delay offset
            by finding the 3rd smallest point (excluding the first two points) */

            /* Variable-length array */
            uint8_t indices[iot2lp_para->bcn_delay_sliding_win_size];
            /* Sort the sliding window buffer */
            bubble_sort(iot2lp_para->bcn_delay_sliding_win_buff, indices, iot2lp_para->bcn_delay_sliding_win_size);
            /* Get the delay offset from the third smallest value */
            iot2lp_para->bcn_delay_offset = iot2lp_para->bcn_delay_sliding_win_buff[indices[2]];
            /* Adjust values in the sliding window based on the delay offset */
            for(uint8_t i=0; i < iot2lp_para->bcn_delay_sliding_win_size; i++){
                if(iot2lp_para->bcn_delay_sliding_win_buff[i] > iot2lp_para->bcn_delay_offset){
                    iot2lp_para->bcn_delay_sliding_win_buff[i] -= iot2lp_para->bcn_delay_offset;
                } else {
                    iot2lp_para->bcn_delay_sliding_win_buff[i] = 0;
                }
            }

            /* Calculate the first beacon delay using the delay offset */
            if(beacon_delay_us > iot2lp_para->bcn_delay_offset){
                beacon_delay_us -= iot2lp_para->bcn_delay_offset;
            }else{
                beacon_delay_us = 0;
            }
            iot2lp_para->last_beacon_delay_us = beacon_delay_us;

        }else{
            /* Update sliding window with beacon delay */
            lpfw_beacon_delay_sliding_win_update(beacon_delay_us);
            /* For the first 15 times, only record the delay time without calculating the delay offset,
            but avoid sleeping */
        }

        return beacon_delay_us;
    }

    /* Subsequently, only iterate the delay_offset gradually, using a conservative approach with small increments */
    if(beacon_delay_us < iot2lp_para->bcn_delay_offset) {
        /* If the difference between bcn_delay_offset and beacon_delay_us is small,
         it indicates a transitional approximation, so we can correct it. However,
         if the difference is large, it might be an exceptional scenario */
        if(iot2lp_para->bcn_delay_offset - beacon_delay_us < 5000 ){
            uint32_t temp = (iot2lp_para->bcn_delay_offset - beacon_delay_us) / 2;
            if(iot2lp_para->bcn_delay_offset > temp){
                iot2lp_para->bcn_delay_offset -= temp;
            }else{
                iot2lp_para->bcn_delay_offset = 0;
            }
        }
        beacon_delay_us = 0;
    }else{
        /* If beacon_delay_us is greater than bcn_delay_offset, it indicates a delay.
         We gradually approach the delay_offset to reduce accumulated errors */
        beacon_delay_us -= iot2lp_para->bcn_delay_offset;
        iot2lp_para->bcn_delay_offset += iot2lp_para->beacon_interval_tu * iot2lp_para->dtim_num / 100;
    }

    /* Update the sliding window */
    lpfw_beacon_delay_sliding_win_update(beacon_delay_us);

    iot2lp_para->last_beacon_delay_us = beacon_delay_us;

    /* Testing modulo calculation */
    // uint32_t test = beacon_timestamp_us % beacon_interval_us;
    // if(test != beacon_delay_us){
    //     while(1);
    // }

    return beacon_delay_us;
}

static ATTR_TCM_SECTION int32_t lpfw_beacon_delay_sliding_win_update(int32_t beacon_delay_us)
{
    /* Overwrite the update beacon delay */
    iot2lp_para->bcn_delay_sliding_win_buff[iot2lp_para->bcn_delay_sliding_win_point] = beacon_delay_us;
    iot2lp_para->bcn_delay_sliding_win_point++;
    if (iot2lp_para->bcn_delay_sliding_win_point >= iot2lp_para->bcn_delay_sliding_win_size) {
        iot2lp_para->bcn_delay_sliding_win_point = 0;
    }

    /* update bcn_delay_sliding_win_status (Number of valid data) */
    if (iot2lp_para->bcn_delay_sliding_win_status < iot2lp_para->bcn_delay_sliding_win_size) {
        iot2lp_para->bcn_delay_sliding_win_status++;
    }

    return 0;
}

ATTR_TCM_SECTION int32_t lpfw_beacon_delay_sliding_win_get_average(void)
{
    int32_t average = 0;
    int32_t data_num;

    /* Insufficient sample quantity */
    if (iot2lp_para->bcn_delay_sliding_win_status < iot2lp_para->bcn_delay_sliding_win_size / 3) {
        return -2;
    }

    /* Sample quantity obtained */
    if (iot2lp_para->bcn_delay_sliding_win_status < iot2lp_para->bcn_delay_sliding_win_size) {
        data_num = iot2lp_para->bcn_delay_sliding_win_status;
    } else {
        data_num = iot2lp_para->bcn_delay_sliding_win_size;
    }

    /*  */
    for (uint16_t i = 0; i < data_num; i++) {
        average += iot2lp_para->bcn_delay_sliding_win_buff[i];
    }
    average = average / data_num;

    return average;
}
