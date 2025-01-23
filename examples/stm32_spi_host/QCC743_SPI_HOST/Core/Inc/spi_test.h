/*
 * spi_test.h
 *
 *  Created on: Nov 22, 2024
 */

#ifndef INC_SPI_TEST_H_
#define INC_SPI_TEST_H_

void spi_start_tx_perf(void);

void spi_stop_tx_perf(void);

void spi_start_rx_perf(void);

void spi_stop_rx_perf(void);

void spi_write_test(int mode, int count, int len);

void spi_read_test(int mode, int count, int len);

void spi_start_show_tput(void);

void spi_stop_show_tput(void);

#endif /* INC_SPI_TEST_H_ */
