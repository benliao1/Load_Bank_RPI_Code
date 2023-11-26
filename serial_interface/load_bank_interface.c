#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <semaphore.h>

#define BUFSIZE 32
#define NUM_SWITCHES 18

#define SEMAPHORE_NAME "/usbfd-sem"	// semaphore on the file descriptor to prevent attempted parallel access to file descriptor
#define FTDI_DEVICE_NAME "/dev/ttyUSB0"	// name of ftdi chip on the raspberry pi; opening this device allows us to talk to the board

// ************************************ DATA REPRESENTATION CONVERSION UTILITIES ********************************** //

// convert from 4-char buffer to 32-bit bitmask
uint32_t buf_to_mask (char *buf)
{
	uint32_t mask = 0;
	mask = (((uint32_t)buf[0]) << 24) | (((uint32_t)buf[1]) << 16) | (((uint32_t)buf[2]) << 8) | (uint32_t)buf[3];

	return mask;
}

// convert from 32-bit bitmask to 4-char buffer
void mask_to_buf (char *buf, uint32_t mask)
{
	buf[0] = (char)((mask >> 24) & 0b11111111);
	buf[1] = (char)((mask >> 16) & 0b11111111);
	buf[2] = (char)((mask >> 8) & 0b11111111);
	buf[3] = (char)(mask & 0b11111111);
}

// convert from string of length 18 consisting of 1s and 0s to 32-bit bitmask (for switch state representation)
uint32_t binstring_to_mask (char *buf)
{
	uint32_t mask = 0;
	for (int i = 0; i < NUM_SWITCHES; i++) {
		if (buf[i] == '\n') {
			break;
		}
		if (buf[i] == '1') {
			mask |= (1 << i);
		} else if (buf[i] == '0') {
			mask &= ~(1 << i);
		} else {
			return 0xFFFFFFFF;
		}
	}
	return mask;
}

// convert from 4-char buffer to string of length 18 consisting of 1s and 0s (for switch state representation)
void buf_to_binstring (char *buf, char *binstring)
{
	// convert to 32-bit mask first
	uint32_t mask = buf_to_mask(buf);

	// go through the mask and generate binstring
	int i;
	for (i = 0; i < NUM_SWITCHES; i++) {
		if (mask & (1 << i)) {
			binstring[i] = '1';
		} else {
			binstring[i] = '0';
		}
	}
	binstring[i] = '\0';
}

// convert from string of 1s, 2s, and 3s (the "phasestring") to three 4-char buffers in a row (for phase state representation)
// return 0 on success, -1 on failure
int phasestring_to_bufs (char *phasestring, char *bufs)
{
	// convert the phasestring into three bitmasks
	uint32_t phase1_mask = 0, phase2_mask = 0, phase3_mask = 0;
	for (int i = 0; i < NUM_SWITCHES; i++) {
		if (phasestring[i] == '1') {
			phase1_mask |= (1 << i);
		} else if (phasestring[i] == '2') {
			phase2_mask |= (1 << i);
		} else if (phasestring[i] == '3') {
			phase3_mask |= (1 << i);
		} else {
			// bad request!
			return -1;
		}
	}

	// then put the three bitmasks into bufs sequentially
	mask_to_buf(bufs, phase1_mask);
	mask_to_buf(bufs + 4, phase2_mask);
	mask_to_buf(bufs + 8, phase3_mask);

	return 0;
}

// convert from three 4-char buffers in a row to string of 1s, 2s, and 3s (the "phasestring") (for phase state representation)
void bufs_to_phasestring (char *bufs, char *phasestring)
{
	// extract the three bitmasks from  the bufs string
	uint32_t phase1_mask = buf_to_mask(bufs);
	uint32_t phase2_mask = buf_to_mask(bufs + 4);
	uint32_t phase3_mask = buf_to_mask(bufs + 8);

	// convert three bitmasks into phasestring
	int i;
	for (i = 0; i < NUM_SWITCHES; i++) {
		if (phase1_mask & (1 << i)) {
			phasestring[i] = '1';
		} else if (phase2_mask & (1 << i)) {
			phasestring[i] = '2';
		} else if (phase3_mask & (1 << i)) {
			phasestring[i] = '3';
		} else {
			printf("Fatal Error? Should not get here...\n");
		}
	}
	phasestring[i] = '\0';
}

// **************************************************** SYSTEM UTILITIES *************************************** //

// open a file descriptor to the serial device
// errors here should return a 500 internal server error message
int serialport_open ()
{
	// open the file descriptor
	int fd = open(FTDI_DEVICE_NAME, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		printf("{\"status\": \"Internal Server Error\", \"msg\": \"Unable to open port\"}");
		return -1;
	}

	// get the current device settings
	struct termios toptions;
	if (tcgetattr(fd, &toptions) < 0) {
		printf("{\"status\": \"Internal Server Error\",  \"msg\": \"Unable to get terminal attributes\"}");
		return -1;
	}

	// set the baud rate to 57600
	speed_t brate = B57600;
	if (cfsetspeed(&toptions, brate) < 0) {
		printf("{\"status\": \"Internal Server Error\", \"msg\": \"Unable to set baudrate\"}");
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
		printf("{\"status\":  \"Internal Server Error\", \"msg\": \"Unable to get termnal attributes\"}");
		return -1;
	}

	return fd;
}

void wait_for_response (int usb_fd, char *ret)
{
	uint8_t len;

	// Read the message length
	read(usb_fd, &len, 1);

	// Read the payload of message one byte at a time into ret
	// (inefficient but we do not care about speed in this step)
	int i;
	for (i = 0; i < len; i++) {
		read(usb_fd, ret + i, 1);
	}
	ret[i] = '\0';
}

void write_msg (int usb_fd, char *msg, uint8_t len)
{
	// allocate a buffer
	char *buf = (char *) malloc(len + 2);
	buf[0] = (char) len;		// copy the length of the message into first byte of the buffer
	memcpy(buf + 1, msg, len); 	// copy message to be sent into the buffer starting at the second byte
	write(usb_fd, buf, len + 1); 	// send the message onto the file descriptor
	free(buf);
}

// ***************************************************** MESSAGE HANDLING FUNCTIONS ******************************************* //

void send_zcs_msg (int usb_fd, char *arg, char *ret)
{
	// send the appropiate command
	if (strncmp(arg, "ON", 2) == 0) {
		write_msg(usb_fd, "ZCS ON\n", 7);
	} else if (strncmp(arg, "OFF", 3) == 0) {
		write_msg(usb_fd, "ZCS OFF\n", 8);
	} else {
		// if the argument is bad, report it
		sprintf(ret, "ERR BAD REQUEST\n");
		return;
	}

	// put the response from the c2000 into the return buffer (should always be "OK")
	wait_for_response(usb_fd, ret);
}

void send_sw_msg (int usb_fd, char *switches, char *ret)
{
	// if switches binstring is not exactly NUM_SWITCHES characters long, incorrect length
	if (strlen(switches) != NUM_SWITCHES) {
		sprintf(ret, "ERR BAD REQUEST\n");
		return;
	}

	// get the 32-bit bit mask fromt the binstring
	uint32_t desired_state = binstring_to_mask(switches);
	if (desired_state == 0xFFFFFFFF) {
		// if there was an error converting to mask, report it and return
		sprintf(ret, "ERR BAD REQUEST\n");
		return;
	}

	// construct the message to be sent and send it
	char msg[BUFSIZE];
	sprintf(msg, "SW ");
	mask_to_buf(msg + 3, desired_state);
	msg[7] = '\n';
	msg[8] = '\0';
	write_msg(usb_fd, msg, 8);

	// put the response from the c2000 into the return buffer (should alwayse be "OK")
	wait_for_response(usb_fd, ret);
}

void send_phase_msg (int usb_fd, char *phasestring, char *ret)
{
	// if phasestring is not exactly NUM_SWITCHES characters long, incorrect length
	if (strlen(phasestring) != NUM_SWITCHES) {
		sprintf(ret, "ERR BAD REQUEST\n");
		return;
	}

	char msg[BUFSIZE];
	sprintf(msg, "PHASE ");
	if (phasestring_to_bufs(phasestring, msg + 6) != 0) {
		// if there was an error converting phasestring into buffer, report it and return
		sprintf(ret, "ERR BAD REQUEST\n");
		return;
	}

	// construct the rest of the message to be send and send it
	msg[18] = '\n';
	msg[19] = '\0';
	write_msg(usb_fd, msg, 19);

	// put the response from the c2000 into the return buffer (should always be "OK")
	wait_for_response(usb_fd, ret);
}

void send_zcs_query_msg (int usb_fd, char *ret)
{
	// send the appropriate command, and put the response into return buffer
	write_msg(usb_fd, "ZCS?\n", 5);
	wait_for_response(usb_fd, ret);
}

void send_sw_query_msg (int usb_fd, char *ret)
{
	// send the appropriate command, and put the response into the return buffer
	write_msg(usb_fd, "SW?\n", 4);
	wait_for_response(usb_fd, ret);
}

void send_phase_query_msg (int usb_fd, char *ret)
{
	// send the appropriate command, and put the response into the return buffer
	write_msg(usb_fd, "PHASE?\n", 7);
	wait_for_response(usb_fd, ret);
}

// *************************************************** REQUEST HANDLING FUNCTIONS ***************************************** //

// handle a standalone zcs query request from the client
void handle_zcs_query_request (int usb_fd)
{
	// send a query message to the c2000 and report back the data in a 200 ok message
	char ret[BUFSIZE];
	send_zcs_query_msg(usb_fd, ret);
	if (strncmp(ret, "ZCS ON", 6) == 0) {
		printf("{\"status\": \"OK\", \"zcs\": \"1\"}");
	} else {
		printf("{\"status\": \"OK\", \"zcs\": \"0\"}");
	}
}


// handle a standalone switch query request from the client
void handle_sw_query_request (int usb_fd)
{
	// send a query message to the c2000 and report back the data in a 200 ok message
	char ret[BUFSIZE];
	char binstring[BUFSIZE];
	send_sw_query_msg(usb_fd, ret);
	buf_to_binstring(ret + 3, binstring);
	printf("{\"status\": \"OK\", \"switches\": \"%s\"}", binstring);
}

// handle a standalone phase query request from the client
void handle_phase_query_request (int usb_fd)
{
	// send a query message to the c2000 and report the data in a 200 ok message
	char ret[BUFSIZE];
	char phasestring[BUFSIZE];
	send_phase_query_msg(usb_fd, ret);
	bufs_to_phasestring(ret + 6, phasestring);
	printf("{\"status\": \"OK\", \"phases\": \"%s\"}", phasestring);
}

// handle a zcs request from the client
void handle_zcs_request (int usb_fd, char *arg)
{
	// first, send a zcs command message to execute the specified command in arg
	char ret[BUFSIZE];
	send_zcs_msg(usb_fd, arg, ret);

	// if the response from the c2000 is not "OK", print a 400 bad request message
	if (strncmp(ret, "OK", 2) != 0) {
		printf("{\"status\": \"Bad Request\", \"msg\": \"Argument \"%s\" is not \"ON\" or \"OFF\" \"}", arg);
		return;
	}

	// if we made it here, get the reported zcs status
	handle_zcs_query_request(usb_fd);
}

// handle a switch request from the client
void handle_sw_request (int usb_fd, char *arg)
{
	// first, send a switch command message to execute the specified command in arg
	char ret[BUFSIZE];
	send_sw_msg(usb_fd, arg, ret);

	// if the response from the c2000 is not "OK", print a 400 bad request message
	// if ZCS timout, then print a 408 request timeout message
	if (strncmp(ret, "OK", 2) != 0) {
		if (strncmp(ret, "ERR ZCS TMOUT", 13) == 0) {
			printf("{\"status\": \"Request Timeout\", \"msg\": \"No Zero-Crossing detected for 10 seconds\"}");
		} else {
			printf("{\"status\": \"Bad Request\", \"msg\": \"Argument had incorrect length, or characters other than '0' or '1'\"}");
		}
		return;
	}

	// if we made it here, get the reported switch status
	handle_sw_query_request(usb_fd);
}

// handle a phase request from the client
void handle_phase_request (int usb_fd, char *arg)
{
	// first, send a phase command message to execute the specified command in arg
	char ret[BUFSIZE];
	send_phase_msg(usb_fd, arg, ret);

	// if the response from the c2000 is not "OK", print a 400 bad request message
	if (strncmp(ret, "OK", 2) != 0) {
		printf("{\"status\": \"Bad Request\", \"msg\": \"Argument \"%s\" had incorrect length, or characters other than '1', '2', and '3'\"}", arg);
		return;
	}

	// if we made it here, get the reported phase status
	handle_phase_query_request(usb_fd);
}

// ************************************************************ MAIN FUNCTION ***************************************** //

// program takes in command line arguments
// will output stuff to stdout
int main (int argc, char **argv)
{
	// open the semaphore; create it if it doesn't already exist
	sem_t *usb_fd_sem = sem_open(SEMAPHORE_NAME, O_CREAT, 0660, 1);
	if (usb_fd_sem == SEM_FAILED) {
		printf("{\"status\": \"Internal Server Error\", \"msg\": \"Could not open or create semaphore\"}");
		return 1;
	}
	// wait on the semaphore before opening the file descriptor
	sem_wait(usb_fd_sem);

	// open connection to ftdi device (which talks to the c2000 on the master board)
	int usb_fd = serialport_open();

	int ret = 0;

	// determine what request was made
	if (argc == 2 || argc == 3) {
		if (strncmp(argv[1], "ZCS?", 4) == 0) {
			handle_zcs_query_request(usb_fd);
		} else if (strncmp(argv[1], "ZCS", 3) == 0) {
			handle_zcs_request(usb_fd, argv[2]);
		} else if (strncmp(argv[1], "SW?", 3) == 0) {
			handle_sw_query_request(usb_fd);
		} else if (strncmp(argv[1], "SW", 2) == 0) {
			handle_sw_request(usb_fd, argv[2]);
		} else if (strncmp(argv[1], "PHASE?", 6) == 0) {
			handle_phase_query_request(usb_fd);
		} else if (strncmp(argv[1], "PHASE", 5) == 0) {
			handle_phase_request(usb_fd, argv[2]);
		} else {
			// a request other than the ones defined above was made
			if (argc == 2) {
				printf("{\"status\": \"Bad Request\", \"msg\": \"Invalid request %s\"}", argv[1]);
			} else {
				printf("{\"status\": \"Bad Request\", \"msg\": \"Invalid request %s %s\"}", argv[1], argv[2]);
			}
			ret = 1;
		}
	} else {
		printf("{\"status\": \"Bad Request\", \"msg\": \"Incorrect number of arguments to server\"}");
	}

	// for nicer display of returned message
	printf("\n");

	// close the file descriptor talking to the ftdi device
	close(usb_fd);

	// post the semaphore so next access can proceed
	sem_post(usb_fd_sem);

	return ret;
}
