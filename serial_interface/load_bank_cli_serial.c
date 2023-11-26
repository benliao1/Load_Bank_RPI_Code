#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define FTDI_DEVICE_NAME "/dev/ttyUSB0"

uint32_t buf_to_mask (char *buf)
{
	uint32_t mask = 0;
	mask = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | (uint32_t)buf[3];

	return mask;
}

void mask_to_buf (char *buf, uint32_t mask)
{
	buf[0] = (char)((mask >> 24) & 0b11111111);
	buf[1] = (char)((mask >> 16) & 0b11111111);
	buf[2] = (char)((mask >> 8) & 0b11111111);
	buf[3] = (char)(mask & 0b11111111);
}

uint32_t binstring_to_mask (char *buf) {
	uint32_t mask = 0;
	for (int i = 0; i <= 18; i++) {
		if (buf[i] == '\n') {
			break;
		}
		if (buf[i] == '1') {
			mask |= (1 << i);
		} else if (buf[i] == '0') {
			mask &= ~(1 << i);
		} else {
			printf("Did not enter a string with only 1s and 0s in it, aborting\n");
			return 0xFFFFFFFF;
		}
	}
	return mask;
}

void wait_for_response (int usb_fd)
{
	char msg[32];
	uint8_t len;

	// Read the message length
	read(usb_fd, &len, 1);

	// Read the payload of message
	int i;
	for (i = 0; i < len; i++) {
		read(usb_fd, msg + i, 1);
	}
	msg[i] = '\0';

	// Print out the message
	if (strncmp(msg, "SW", 2) == 0) {
		// it's a switch state query response, print out all the bits
		printf("Current Switch State:\n\t");
		uint32_t reported_state = buf_to_mask(msg + 3);
		for (int i = 0; i < 18; i++) {
			printf("%c", (reported_state & (1 << i)) ? '1' : '0');
		}
		printf("\n");
	} else if (strncmp(msg, "PHASE", 5) == 0) {
		// it's a phase state query response, print out all the bits
		printf("Current Phase Definitions:\n");
		for (int phase = 1; phase <= 3; phase++) {
			printf("\tPhase %d: ", phase);
			uint32_t reported_def = buf_to_mask(msg + 6 + ((phase-1)*4));
			for (int i = 0; i < 18; i++) {
				printf("%c", (reported_def & (1 << i)) ? '1' : '0');
			}
			printf("\n");
		}
	} else {
		printf("Response received: %s", msg);
	}
}

void write_msg (int usb_fd, char *msg, uint8_t len)
{
	char *buf = (char *) malloc(len + 2);
	buf[0] = (char) len;
	memcpy(buf + 1, msg, len); // copy message over
	write(usb_fd, buf, len + 1); // send the buffer
	free(buf);
}

void prompt_zcs_msg (int usb_fd)
{
	char *buf = NULL;
	size_t len = 0;;

	printf("Turn ZCS ON or OFF?\n> ");
	getline(&buf, &len, stdin);

	if (strncmp(buf, "ON", 2) == 0) {
		write_msg(usb_fd, "ZCS ON\n", 7);
	} else if (strncmp(buf, "OFF", 3) == 0) {
		write_msg(usb_fd, "ZCS OFF\n", 8);
	} else {
		printf("Did not specify one of ON or OFF, aborting\n");
		free(buf);
		return;
	}
	wait_for_response(usb_fd);
	free(buf);
}

void prompt_sw_msg (int usb_fd)
{
	char *buf = NULL;
	size_t len = 0;
	printf("Please enter desired state of switches as a string of 1s and 0s.\n");
	printf("First switch is the first character. Type up to the number of switches (18)\n> ");
	getline(&buf, &len, stdin);

	uint32_t desired_state = binstring_to_mask(buf);
	if (desired_state == 0xFFFFFFFF) {
		free(buf);
		return;
	}

	char msg[32];
	sprintf(msg, "SW ");
	mask_to_buf(msg + 3, desired_state);
	msg[7] = '\n';
	msg[8] = '\0';
	write_msg(usb_fd, msg, 8);

	wait_for_response(usb_fd);
	free(buf);
}

void prompt_phase_msg (int usb_fd)
{
	char *buf = NULL;
	size_t len = 0;
	uint32_t phase_defs[3];

	for (int phase = 1; phase <= 3; phase++) {
		printf("Please enter phase %d definition as a string of 1s and 0s.\n", phase);
		printf("First switch is the first character. Type up to 18 characters\n> ");
		getline(&buf, &len, stdin);
		phase_defs[phase - 1] = binstring_to_mask(buf);
		if (phase_defs[phase - 1] == 0xFFFFFFFF) {
			free(buf);
			return;
		}
	}

	char msg[32];
	sprintf(msg, "PHASE ");
	mask_to_buf(msg + 6, phase_defs[0]);
	mask_to_buf(msg + 10, phase_defs[1]);
	mask_to_buf(msg + 14, phase_defs[2]);
	msg[18] = '\n';
	msg[19] = '\0';
	write_msg(usb_fd, msg, 19);

	wait_for_response(usb_fd);
	free(buf);
}

void send_zcs_query_msg (int usb_fd)
{
	write_msg(usb_fd, "ZCS?\n", 5);
	wait_for_response(usb_fd);
}

void send_sw_query_msg (int usb_fd)
{
	write_msg(usb_fd, "SW?\n", 4);
	wait_for_response(usb_fd);
}

void send_phase_query_msg (int usb_fd)
{
	write_msg(usb_fd, "PHASE?\n", 7);
	wait_for_response(usb_fd);
}

int serialport_open ()
{
	// open the file descriptor
	int fd = open(FTDI_DEVICE_NAME, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		printf("unable to open port\n");
		return -1;
	}

	// get the current device settings
	struct termios toptions;
	if (tcgetattr(fd, &toptions) < 0) {
		printf("unable to get terminal attribubes\n");
		return -1;
	}

	// set the baud rate to 57600
	speed_t brate = B57600;
	if (cfsetspeed(&toptions, brate) < 0) {
		printf("unable to set baudrate\n");
	}

	// update the serial port options for 8-N-1 (8 data bits, no parity, 1 stop bit)
	toptions.c_cflag &= ~CSIZE;
	toptions.c_cflag |= CS8;
	toptions.c_cflag &= ~PARENB;
	toptions.c_cflag &= ~CSTOPB;

	// disables special processing of input and output bytes
	cfmakeraw(&toptions);

	// define what happens on a call to read()
	toptions.c_cc[VMIN] = 1; 	// block until at least one byte has been read
	toptions.c_cc[VTIME] = 0;	// never timeout on a read call (could cause hangs but hopefully not)

	// save the changes we made to the options and have them take effect now
	tcsetattr(fd, TCSANOW, &toptions);
	if (tcsetattr(fd, TCSAFLUSH, &toptions) < 0) {
		printf("unable to set terminal attributes\n");
		return -1;
	}

	return fd;
}

int main()
{
	// open connection to ftdi device
	int usb_fd = serialport_open();

	char *buf = (char *) malloc(32);
	size_t len = 0;
	while (1) {
		printf("What message to send? SW, ZCS, PHASE, SW?, ZCS?, PHASE? ... EXIT to exit CLI.\n> ");
		getline(&buf, &len, stdin);
		if (strncmp(buf, "ZCS?", 4) == 0) {
			send_zcs_query_msg(usb_fd);
		} else if (strncmp(buf, "ZCS", 3) == 0) {
			prompt_zcs_msg(usb_fd);
		} else if (strncmp(buf, "SW?", 3) == 0) {
			send_sw_query_msg(usb_fd);
		} else if (strncmp(buf, "SW", 2) == 0) {
			prompt_sw_msg(usb_fd);
		} else if (strncmp(buf, "PHASE?", 6) == 0) {
			send_phase_query_msg(usb_fd);
		} else if (strncmp(buf, "PHASE", 5) == 0) {
			prompt_phase_msg(usb_fd);
		} else if (strncmp(buf, "EXIT", 4) == 0) {
			break;
		} else {
			printf("Did not enter a valid option\n");
		}
	}

	// Close the socket
	close(usb_fd);
	free(buf);

	return 0;
}
