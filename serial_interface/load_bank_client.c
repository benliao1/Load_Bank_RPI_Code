
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>   //for inet_addr, bind, listen, accept, socket types
#include <netinet/in.h>  //for structures relating to IPv4 addresses

#define NETBURNER_ADDR "192.168.68.107"
#define NETBURNER_PORT 23

int main()
{
	// Make a socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// Fill out server address
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(NETBURNER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(NETBURNER_ADDR);

	// Connect the socket
	connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	// Write something to the board
	char msg[16];
	uint32_t state = 0b010101010101010101;
	char first_byte = (char)((state >> 24) & 0b11111111);
	char second_byte = (char)((state >> 16) & 0b11111111);
	char third_byte = (char)((state >> 8) & 0b11111111);
	char fourth_byte = (char)(state & 0b11111111);
	sprintf(msg, "SW %c%c%c%c\n", first_byte, second_byte, third_byte, fourth_byte);
	write(sockfd, msg, 8);

	// Read the message back
	int i = 0;
	do {
		read(sockfd, msg + i, 1);
	} while (msg[i++] != '\n');
	msg[i] = '\0';

	// Print out the message
	printf("Message received: %s", msg);


	// Close the socket
	close(sockfd);

	return 0;
}
