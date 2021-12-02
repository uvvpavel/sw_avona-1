#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pigpiod_if2.h>

typedef int16_t samp_t;
#define SPI_CHANNELS 2
#define LENGTH 7000/15

#define WAKEWORD_EVENT 0
#define SPI_EVENT 1
#define DONE_RECORDING 2
#define START_RECORDING 3

int xcore_ctrl = PI_I2C_OPEN_FAILED;
int spi = PI_SPI_OPEN_FAILED;

static int frames_saved;


static size_t spi_frames_available(int pi)
{
	uint8_t ctrl_buf[3] = {5, 0x82, 4};
	uint32_t frames_available;
	i2c_write_device(pi, xcore_ctrl, ctrl_buf, 3);
	i2c_read_device(pi, xcore_ctrl, &frames_available, 4);
	
	return frames_available;
}

int save_spi_data(int pi)
{
	samp_t rx_buf[240][SPI_CHANNELS];
	samp_t zeros[240][SPI_CHANNELS] = {{0},{0}};
	size_t avail;
	static FILE *f;

	if (f == NULL) {
		f = fopen("audio.dat", "wb");
	}

	while ((avail = spi_frames_available(pi)) > 0) {

		for (; avail > 0; avail--) {

			if (frames_saved < LENGTH) {

				gpio_write(pi, 8, 0);
				int br = spi_read(pi, spi, (char *) rx_buf, sizeof(rx_buf));
				gpio_write(pi, 8, 1);
				//printf("br = %d\n", br);
				if (memcmp(rx_buf, zeros, sizeof(rx_buf)) != 0) {
					fwrite(rx_buf, 1, sizeof(rx_buf), f);
					frames_saved++;
				}
			}
			
			if (frames_saved == LENGTH) {
				fclose(f);
				f = NULL;
				return 0;
			}
		}
	}

	return 1;
}


void intn_cb(int pi, unsigned user_gpio, unsigned level, uint32_t tick, int *io_expander)
{
	static int recording;
	int input = i2c_read_byte_data(pi, *io_expander, 0x00);

	if (input >= 0 && (input & 0b00000010) == 0) {

		/* interrupt from the xcore */

		uint8_t ctrl_buf[3] = {5, 0x81, 1};
		uint8_t interrupt_status;
		i2c_write_device(pi, xcore_ctrl, ctrl_buf, 3);
		i2c_read_device(pi, xcore_ctrl, &interrupt_status, 1);

		//printf("%02x\n", interrupt_status);

		if (interrupt_status & 0x01) {
			printf("Heard wakeword\n");
			//event_trigger(pi, WAKEWORD_EVENT);
			if (!recording) {
				recording = 1;
				event_trigger(pi, START_RECORDING);
				frames_saved = 0;
				save_spi_data(pi);
			}
		}
		
		if (interrupt_status & 0x02) {
			//printf("SPI\n");
			if (recording) {
				if (save_spi_data(pi) == 0) {
					recording = 0;
					event_trigger(pi, DONE_RECORDING);
				}
			}
			//event_trigger(pi, SPI_EVENT);
		}
	}
}

int main(int argc, char **argv)
{
	bool ok = true;
	int pi;
	//int spi = PI_SPI_OPEN_FAILED;
	int io_expander = PI_I2C_OPEN_FAILED;
	//int xcore_ctrl = PI_I2C_OPEN_FAILED;

	pi = pigpio_start(NULL, NULL);
	if (pi < 0) {
		printf("Failed to connect to pigpio server\n");
		ok = false;
	}

	if (ok) {
		io_expander = i2c_open(pi, 1, 0x20, 0);

		if (io_expander >= 0) {
			/* SPI enabled, DAC in reset */
			i2c_write_byte_data(pi, io_expander, 0x01, 0b10101111);

			/* Set pin directions correctly */
			i2c_write_byte_data(pi, io_expander, 0x03, 0b10001010);

			/* Don't invert the inputs */
			i2c_write_byte_data(pi, io_expander, 0x02, 0b00000000);

			/* Don't latch the INT_N signal from the xcore */
			//i2c_write_byte_data(pi, io_expander, 0x42, 0b00000010);
			i2c_write_byte_data(pi, io_expander, 0x42, 0b00000000);

			/* Disable interrupts on the INT_N signal */
			i2c_write_byte_data(pi, io_expander, 0x45, 0b11111111);

		} else {
			ok = false;
		}
	}

	if (ok) {
		xcore_ctrl = i2c_open(pi, 1, 0x42, 0);
		if (xcore_ctrl < 0) {
			ok = false;
		}
	}

	if (ok) {
		set_mode(pi, 8, PI_OUTPUT); /* SPI CE */
		set_mode(pi, 27, PI_INPUT); /* WW */
		set_mode(pi, 15, PI_INPUT); /* SPI data available */

		spi = spi_open(pi, 0, 4000000, 0b100011);

		if (spi >= 0) {
			gpio_write(pi, 8, 1);
		} else {
			ok = false;
			printf("Failed to open SPI\n");
		}
	}

	if (ok) {
		int intn_cbid;
		
		uint8_t ctrl_buf[4] = {5, 0x00, 1, 0};
		/* Disable xcore interrupts */
		i2c_write_device(pi, xcore_ctrl, ctrl_buf, 4);
		
		/* Clear out any outstanding xcore interrupts */
		uint8_t ctrl2_buf[3] = {5, 0x81, 1};
		uint8_t interrupt_status;
		i2c_write_device(pi, xcore_ctrl, ctrl2_buf, 3);
		i2c_read_device(pi, xcore_ctrl, &interrupt_status, 1);
		
		intn_cbid = callback_ex(pi, 27, FALLING_EDGE, (CBFuncEx_t) intn_cb, &io_expander); 

		/* Enable interrupts on the INT_N signal */
		i2c_read_byte_data(pi, io_expander, 0x00);
		i2c_write_byte_data(pi, io_expander, 0x45, 0b11111101);
		
		/* Enable xcore interrupts */
		ctrl_buf[3] = 1;
		i2c_write_device(pi, xcore_ctrl, ctrl_buf, 4);

		//if (gpio_read(pi, 27) == PI_LOW) {
			
			/* Clear the active interrupt by reading the input */
			//printf("CLEARING\n");
		//	intn_cbid = callback_ex(pi, 27, FALLING_EDGE, (CBFuncEx_t) intn_cb, &io_expander); 
			//intn_cb(pi, 27, PI_LOW, 0, &io_expander);
		//} else {
		//	intn_cbid = callback_ex(pi, 27, FALLING_EDGE, (CBFuncEx_t) intn_cb, &io_expander); 
		//}
			
	//	(void) i2c_read_byte_data(pi, io_expander, 0x00);
		
		
		if (wait_for_event(pi, START_RECORDING, 10)) {
			if (!wait_for_event(pi, DONE_RECORDING, 20)) {
				ok = false;
				printf("Recording failed\n");
			}
		} else {
			printf("Wakeword did not occur, exiting now\n");
			ok = false;
		}
		
		callback_cancel(intn_cbid);
	}

#if 0
	if (ok) {

		samp_t rx_buf[240][SPI_CHANNELS];
		samp_t zeros[240][SPI_CHANNELS] = {{0},{0}};

		FILE *f = fopen("audio.dat", "wb");

		for (int i = 0; i < LENGTH; i++) {

#if 0
			if (gpio_read(pi, 15) == PI_LOW) {
				if (wait_for_edge(pi, 15, RISING_EDGE, 1) == false) {
					printf("timeout waiting for SPI audio. %d\n", gpio_read(pi, 15));
					break;
				}
				//printf("low!\n");
			}
#endif
//			while (gpio_read(pi, 15) == PI_LOW);

			
			if (!wait_for_event(pi, SPI_EVENT, 1)) {
				printf("Timeout waiting for SPI audio\n");
				break;
			}

			gpio_write(pi, 8, 0);
			int br = spi_read(pi, spi, (char *) rx_buf, sizeof(rx_buf));
			gpio_write(pi, 8, 1);
			//printf("br = %d\n", br);
			if (memcmp(rx_buf, zeros, sizeof(rx_buf)) != 0) {
				fwrite(rx_buf, 1, sizeof(rx_buf), f);
			} else {
				//printf("ZERO\n");
				i--;
			}
		}

		fclose(f);


		//printf("edge %d\n", edge);
	
	}
#endif

	if (spi >= 0) {
		spi_close(pi, spi);
	}

	if (io_expander >= 0) {
		i2c_close(pi, io_expander);
	}

	if (xcore_ctrl >= 0) {
		i2c_close(pi, xcore_ctrl);
	}

	if (pi >= 0) {
		pigpio_stop(pi);
	}

	return 0;
}

