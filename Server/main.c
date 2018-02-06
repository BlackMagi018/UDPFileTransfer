#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <unistd.h>
#include <ctype.h>

#define MAXBUF 1024
#define MAXSIZE 1028

int main(int argc, char **argv) {

    //new line character
    char nl[2] = "\n";

    // Connection Status
    int connection = 1;
    //Slide window is implemented as a pointer to an array of char arrays that hold 1024 bytes
    char **slide = malloc(5 * sizeof(char *));
    for (int i = 0; i < 5; i++) {
        slide[i] = (char *) malloc(MAXSIZE * sizeof(char));
    }

    //Create socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("There was an error creating the socket\n");
        return 1;
    }

    //Have user enter the port number to use
    //User prompted for input. Port Number to use, IP Address to send to
    printf("Enter Port Number: ");
    char port[10];
    fgets(port, 10, stdin);
    char *holder = strtok(port, nl);
    strcpy(port, holder);
    int ports = (int) strtol(port, NULL, 10);
    if (isdigit(ports) != 0) {
        printf("Invalid Port Number. Defaulting to port #9876\n");
        ports = 9876;
    }

    //setup UDP connection
    struct sockaddr_in serveraddr, clientaddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((uint16_t) strtol(port, NULL, 10));
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //Bind socket with connection
    bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

    //Process keeps running until the connection is closed
    while (connection) {

        //length of size of clientaddr object
        socklen_t len = sizeof(clientaddr);
        //holder for filename
        char file[150];
        char check[150];
        //sent method return value
        int sent = 0;
        //receive method return value
        int receive = 0;
        //resend timeout
        int timeout = 0;
        //acknowledgment sent
        char *receipt = "OK\0";
        //acknowledgement received
        int ack;
        // File Size
        long fsize = 0;

        //File pointer for file to be transferred
        FILE *fp = NULL;

        //receive file name
        while (receive <= 0) {
            receive = (int) recvfrom(sockfd, file, 150, 0,
                                     (struct sockaddr *) &clientaddr, &len);
            if (receive == -1) {
                fprintf(stderr, "Value of errno:  %d\n", errno);
                printf("Error sending:\n");
            } else if (receive == 0) {
                printf("Waiting for filename\n");
            } else {
                printf("Connection established, Filename received - %s\n", file);
                //Remove newline operator from the end of the string
                char *filename = strtok(file, nl);

                //File pointer for file to be transferred
                fp = fopen(filename, "rb");

                //Check to see if file opened properly. If not the connection is terminated
                if (fp == NULL) {
                    printf("File not found\n");
                    return -1;
                }

                //Get file size
                struct stat st;
                stat(file, &st);
                fsize = (int) st.st_size;
            }
        }

        receive = 0;

        //send acknowledgment of receipt
        while (sent <= 0) {
            sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                                (struct sockaddr *) &clientaddr, len);
            if (sent == -1) {
                fprintf(stdout, "Error sending:\n");
                fprintf(stderr, "Value of error:  %d\n", errno);
            } else {
                fprintf(stdout, "File Name Acknowledgement sent\n");
            }
            sleep(2);
        }

        clock_t start, end;
        start = clock();

        //Check to see if acknowledgment was received or not or 6 seconds have passed
        while (receive <= 0) {
            end = clock();
            receive = (int) recvfrom(sockfd, check, 150, 0,
                                     (struct sockaddr *) &clientaddr, &len);
            if (receive == -1) {
                fprintf(stderr, "Value of errno:  %d\n", errno);
                printf("Error sending:\n");
            } else if (receive == 0) {
                printf("Waiting for acknowledgment\n");
            } else {
                if (strncmp(receipt, check, strlen(receipt)) == 0 ||
                    (((double) end - (double) start) / CLOCKS_PER_SEC) > 6) {
                    printf("Acknowledgment Checks Complete\n");
                } else {
                    char *filename = strtok(check, nl);
                    if (strcmp(filename, file) == 0) {
                        sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                                            (struct sockaddr *) &clientaddr, len);
                        receive = 0;
                    }
                }
            }
        }


        sent = 0;
        receive = 0;

        //prepare to send file size
        char size[sizeof(long)];
        sprintf(size, "%ld", fsize);
        printf("File Size Sent: %s\n", size);


        //send file size
        while (sent <= 0) {
            sent = (int) sendto(sockfd, size, sizeof(size), 0,
                                (struct sockaddr *) &clientaddr, len);
            if (sent == -1) {
                fprintf(stderr, "Value of error:  %d\n", errno);
                fprintf(stderr, "Error sending file size:\n");
            } else {
                printf("File size sent to client\n");
            }
        }

        //receive acknowledgment of receipt
        while (receive <= 0) {
            receive = (int) recvfrom(sockfd, &ack, sizeof(int), 0,
                                     (struct sockaddr *) &clientaddr, &len);
            if (receive == -1) {
                fprintf(stderr, "Value of errno:  %d\n", errno);
                fprintf(stderr, "Error receiving acknowledgment:\n");
            } else if (receive == 0) {
                printf("Client has requested an orderly shutdown\n");
                return 1;
            } else {
                printf("File Size Acknowledgement received\n");
                sent = (int) sendto(sockfd, receipt, strlen(receipt), 0,
                                    (struct sockaddr *) &clientaddr, len);
                if (sent == -1) {
                    fprintf(stderr, "Value of error:  %d\n", errno);
                    printf("Error sending:\n");
                } else { ;
                }
            }
            timeout++;
            if (timeout == 3) {
                sent = (int) sendto(sockfd, (const void *) size, sizeof(size), 0,
                                    (struct sockaddr *) &clientaddr, len);
                if (sent == -1) {
                    fprintf(stderr, "Value of error:  %d\n", errno);
                    fprintf(stderr, "Error sending file size:\n");
                } else {
                    printf("File size resent to client\n");
                }
            }
        }

        sleep(4);

        //Storage will be used as the transfer buffer on the file contents
        char *storage;
        storage = (char *) malloc(MAXBUF * sizeof(char));

        //Load the first 5120 bytes into the sliding window
        for (int i = 0; i < 5; i++) {
            fread(storage, sizeof(char), MAXBUF, fp);
            sprintf(slide[i], "%02d%s", i, storage);
        }
        free(storage);

        //Sequence variable used to keep track of sliding window
        int seq = 0;

        //Send the first set of data
        for (int i = 0; i < 5; i++) {
            sent = (int) sendto(sockfd, slide[i], MAXSIZE, 0,
                                (struct sockaddr *) &clientaddr, len);
            if (sent > 0) {
                fprintf(stdout, "Sent packet #%i\n", seq);
            } else {
                fprintf(stderr, "Packet failed to send\n");
                fprintf(stderr, "Value of error:  %d\n", errno);
            }
            sleep(2);
            seq++;
        }

        //temp variables
        char code[3];
        char checker[3];

        while (!feof(fp)) {
            //hopefully receive acknowledgment
            receive = (int) recvfrom(sockfd, code, sizeof(code), 0,
                                     (struct sockaddr *) &clientaddr, &len);
            if (receive > 0) {
                fprintf(stdout, "Received acknowledgment packet\n");

                //read acknowledgment - sequence code of the packet acknowledged
                ack = (int) strtol((const char *) code, NULL, 10);

                //reset storage contents
                memset(storage, 0, MAXBUF);

                //read next 1024 bytes into sliding window
                (int) fread(storage, sizeof(char), MAXBUF, fp);

                //reset buffer contents
                memset(slide[ack % 5], 0, MAXSIZE);

                //increment sequence code by 5 cause place in array is sequence code % 5
                ack = ack + 5;

                //formatted buffer data and get buffer contents size
                int total = sprintf(slide[(ack % 5)], "%02i%s", ack, storage);

                //send data to client
                sent = (int) sendto(sockfd, slide[(ack % 5)], total * sizeof(char), 0,
                                    (struct sockaddr *) &clientaddr, len);
                if (sent > 0) {
                    fprintf(stdout, "Sent data packet #%i\n", ack);
                } else {
                    printf("Packet failed to send");
                }

            } else {
                printf("Failure to receive");
            }
            //Check to see if any data is left over and if so resend it
            if ((seq / 4) == 0) {
                for (int i = 0; i < 5; i++) {
                    strncpy(checker, slide[i], 2);
                    checker[3] = (char) "\0";
                    int temp = (int) strtol(checker, NULL, 10);
                    if (temp < seq) {
                        sent = (int) sendto(sockfd, slide[i], strlen(slide[i]) * sizeof(char), 0,
                                            (struct sockaddr *) &clientaddr, len);
                    }
                }
            }
            sleep(2);
        }
        connection = 0;
        close(sockfd);
    }
    return 0;
}