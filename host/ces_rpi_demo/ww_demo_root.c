#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <pigpio.h>

typedef int16_t samp_t;
#define SPI_CHANNELS 2
#define LENGTH 7000/15
#define AUDIO_FRAME_LENGTH 240

#define SPI_CS_TO_CLK_MIN_TIME_US 100
#define SPI_CS_TO_CS_MIN_TIME_US 1000

#define APP_CONTROL_I2C_ADDRESS                 0x42
#define APP_CONTROL_RESID_SYSTEM                   5
#define APP_CONTROL_CMD_INTERRUPT_ENABLED       0x00
#define APP_CONTROL_CMD_SYSTEM_INTERRUPT_STATUS 0x01
#define APP_CONTROL_CMD_SPI_FRAME_COUNT         0x02

#define WAKEWORD_EVENT 0
#define SPI_EVENT 1
#define DONE_RECORDING 2
#define START_RECORDING 3

volatile int recording_done;

int xcore_ctrl = PI_I2C_OPEN_FAILED;
int io_expander = PI_I2C_OPEN_FAILED;
int spi = PI_SPI_OPEN_FAILED;

static int frames_saved;


static void avona_dev_ctrl_write(uint8_t resid, uint8_t cmd, uint8_t len, const void *data)
{
	uint8_t ctrl_buf[3 + UINT8_MAX];

	ctrl_buf[0] = resid;
	ctrl_buf[1] = cmd & 0x7F;
	ctrl_buf[2] = len;

	assert(len <= UINT8_MAX);
	memcpy(&ctrl_buf[3], data, len);

	i2cWriteDevice(xcore_ctrl, (char *) ctrl_buf, 3 + len);
}

static void avona_dev_ctrl_read(uint8_t resid, uint8_t cmd, uint8_t len, void *data)
{
	uint8_t ctrl_buf[3];

	ctrl_buf[0] = resid;
	ctrl_buf[1] = cmd | 0x80;
	ctrl_buf[2] = len;

	i2cWriteDevice(xcore_ctrl, (char *) ctrl_buf, 3);
	i2cReadDevice(xcore_ctrl, (char *) data, len);
}

static size_t avona_spi_frames_available(void)
{
	uint32_t frames_available;

	avona_dev_ctrl_read(APP_CONTROL_RESID_SYSTEM,
	                    APP_CONTROL_CMD_SPI_FRAME_COUNT,
	                    sizeof(uint32_t),
	                    &frames_available);

	return frames_available;
}

static uint8_t avona_interrupt_status(void)
{
	uint8_t interrupt_status;
	
	avona_dev_ctrl_read(APP_CONTROL_RESID_SYSTEM,
	                    APP_CONTROL_CMD_SYSTEM_INTERRUPT_STATUS,
	                    sizeof(uint8_t),
	                    &interrupt_status);

	return interrupt_status;
}

static void avona_interrupt_enable(bool enable)
{
	uint8_t e = enable ? 1 : 0;
	
	avona_dev_ctrl_write(APP_CONTROL_RESID_SYSTEM,
	                    APP_CONTROL_CMD_INTERRUPT_ENABLED,
	                    sizeof(uint8_t),
	                    &e);
}

static void avona_spi_audio_read(samp_t audio[AUDIO_FRAME_LENGTH][SPI_CHANNELS])
{
	gpioWrite(8, 0);
	gpioDelay(SPI_CS_TO_CLK_MIN_TIME_US);
	(void) spiRead(spi, (char *) audio, sizeof(samp_t) * AUDIO_FRAME_LENGTH * SPI_CHANNELS);
	gpioWrite(8, 1);

	gpioDelay(SPI_CS_TO_CS_MIN_TIME_US);
}

int save_spi_data(void)
{
	samp_t audio[AUDIO_FRAME_LENGTH][SPI_CHANNELS];
	samp_t zeros[AUDIO_FRAME_LENGTH][SPI_CHANNELS] = {{0},{0}};
	size_t avail;
	static FILE *f;

	if (f == NULL) {
		f = fopen("audio.dat", "wb");
	}

	while ((avail = avona_spi_frames_available()) > 0) {

		for (; avail > 0; avail--) {

			if (frames_saved < LENGTH) {
				avona_spi_audio_read(audio);
				if (memcmp(audio, zeros, sizeof(audio)) != 0) {
					fwrite(audio, 1, sizeof(audio), f);
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


void intn_cb(int gpio, int level, uint32_t tick)
{
	static int recording;
	static int last_mute = 0;

	int input = i2cReadByteData(io_expander, 0x00);
	
	if (input < 0) {
		printf("i2c error\n");
		return;
	}

	int mute = (input >> 7) & 1;

	if (mute != last_mute) {
		printf("Mute changed to %d\n", mute);
		last_mute = mute;
	}

	if ((input & 0b00000010) == 0) {
		/* interrupt from the xcore */
		uint8_t interrupt_status = avona_interrupt_status();

		//printf("%02x\n", interrupt_status);

		if (interrupt_status & 0x01) {
			printf("Heard wakeword\n");
			//event_trigger(pi, WAKEWORD_EVENT);
			if (!recording) {
				recording = 1;
				//event_trigger(pi, START_RECORDING);
				printf("START_RECORDING event\n");
				frames_saved = 0;
				save_spi_data();
			}
		}
		
		if (interrupt_status & 0x02) {
			//printf("SPI\n");
			if (recording) {
				if (save_spi_data() == 0) {
					recording = 0;
					//event_trigger(pi, DONE_RECORDING);
					printf("DONE_RECORDING event\n");
					recording_done = 1;
				}
			}
			//event_trigger(pi, SPI_EVENT);
		}
	}
}

int main(int argc, char **argv)
{
	bool ok = true;

	if (gpioInitialise() < 0) {
		printf("Failed to init pigpio\n");
		ok = false;
	}

	if (ok) {
		io_expander = i2cOpen(1, 0x20, 0);

		if (io_expander >= 0) {
			/* SPI enabled, DAC in reset */
			i2cWriteByteData(io_expander, 0x01, 0b10101111);

			/* Set pin directions correctly */
			i2cWriteByteData(io_expander, 0x03, 0b10001010);

			/* Don't invert the inputs */
			i2cWriteByteData(io_expander, 0x02, 0b00000000);

			/* Latch the INT_N signal from the xcore */
			i2cWriteByteData(io_expander, 0x42, 0b00000010);

			/* Disable interrupts on the INT_N signal */
			i2cWriteByteData(io_expander, 0x45, 0b11111111);

		} else {
			ok = false;
		}
	}

	if (ok) {
		xcore_ctrl = i2cOpen(1, APP_CONTROL_I2C_ADDRESS, 0);
		if (xcore_ctrl < 0) {
			ok = false;
		}
	}

	if (ok) {
		gpioSetMode(8, PI_OUTPUT); /* SPI CE */
		
		gpioSetMode(27, PI_INPUT); /* INT_N */
		gpioSetPullUpDown(27, PI_PUD_OFF);

		spi = spiOpen(0, 4000000, 0b100011);

		if (spi >= 0) {
			gpioWrite(8, 1);
		} else {
			ok = false;
			printf("Failed to open SPI\n");
		}
	}

	if (ok) {
		/* Disable xcore interrupts */
		avona_interrupt_enable(false);

		/* Clear out any outstanding xcore interrupts */
		(void) avona_interrupt_status();

		/* At this point the xcore INT_N signal should be high */

		/* Enable interrupts on the xcore INT_N signal */
		i2cWriteByteData(io_expander, 0x45, 0b01111101);

		/* Ensure any outstanding i/o expander interrupts are cleared */
		i2cReadByteData(io_expander, 0x00);

		/* At this point, the expander's INT_N signal should be high */

		if (gpioSetISRFunc(27, FALLING_EDGE, 0, (gpioISRFunc_t) intn_cb) == 0) {
			/* Enable xcore interrupts */
			avona_interrupt_enable(true);
		} else {
			ok = false;
		}
	}

	if (ok) {

		printf("Listening\n");

		while (!recording_done);
		
		//int h = gpioNotifyOpen();
		#if 0
		if (wait_for_event(pi, START_RECORDING, 100)) {
			if (!wait_for_event(pi, DONE_RECORDING, 20)) {
				ok = false;
				printf("Recording failed\n");
			}
		} else {
			printf("INTN is %d\n", gpio_read(pi, 27));
			printf("Wakeword did not occur, exiting now\n");
			ok = false;
		}
		#endif
		
		gpioSetISRFuncEx(27, FALLING_EDGE, 0, NULL, NULL);
	}

	if (spi >= 0) {
		spiClose(spi);
	}

	if (io_expander >= 0) {
		i2cClose(io_expander);
	}

	if (xcore_ctrl >= 0) {
		i2cClose(xcore_ctrl);
	}

	gpioTerminate();

	return 0;
}

