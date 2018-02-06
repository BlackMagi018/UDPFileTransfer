#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAXBUF 1024
#define MAXSIZE 1028

int main(int argc, char **argv) {
    //new line character
    char nl[2] = "\n";

    //Created socket file descriptor
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("There was an error creating the socket\n");
        return 1;
    }

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
    printf("Enter IP Address: ");
    char IP[15];
    fgets(IP, 15, stdin);
    holder = strtok(IP, nl);
    strcpy(IP, holder);

    //setup UDP connection
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons((uint16_t) ports);
    serveraddr.sin_addr.s_addr = inet_addr(IP);

    //connect to server
    int e = connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr));
    if (e < 0) {
        printf("There was an error connecting\n");
        return 1;
    }

    //User  prompted for input. File name to retrieve
    socklen_t len = sizeof(serveraddr);
    printf("What file would you like to retrieve. ");

    //filename holder
    char line1[150];
    char line2[150];
    char check[sizeof(long)];
    //Data buffer
    char buff[MAXSIZE];

    fgets(line1, 151, stdin);
    strcpy(line2, line1);
    //file size
    long fsize = 0;
    //received data size
    long rsize = 0;
    //sent method return value
    int sent = 0;
    //receive method return value
    int receive = 0;
    //resend timeout
    int timeout = 0;
    //acknowledgment
    char *receipt = "OK\0";

    //send file name
    while (sent <= 0) {
        sent = (int) sendto(sockfd, line1, 151, 0,
                            (struct sockaddr *) &serveraddr, len);
        if (sent == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error sending:\n");
        } else {
            fprintf(stdout, "File name sent to server\n");
        }
    }

    //receive acknowledgment of receipt
    while (receive <= 0) {
        receive = (int) recvfrom(sockfd, &fsize, sizeof(long), 0,
                                 (struct sockaddr *) &serveraddr, &len);
        if (receive == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error receiving:\n");
        } else if (receive == 0) {
            printf("Server has requested and orderly shutdown\n");
            return 1;
        } else {
            printf("File Name Acknowledgement received\n");
            sent = (int) sendto(sockfd, receipt, strlen(receipt), 0,
                                (struct sockaddr *) &serveraddr, len);
            if (sent == -1) {
                fprintf(stderr, "Value of error:  %d\n", errno);
                printf("Error sending:\n");
            } else {
                fprintf(stdout, "Acknowledgment Check sent\n");
            }
        }
        timeout++;
        //If file name packet was lost, resend file name
        if (timeout == 3) {
            sent = (int) sendto(sockfd, line1, 151, 0,
                                (struct sockaddr *) &serveraddr, len);
            if (sent == -1) {
                fprintf(stderr, "Value of error:  %d\n", errno);
                printf("Error sending:\n");
            } else {
                fprintf(stdout, "File name resent to server\n");
            }
            timeout = 0;
        }
        sleep(2);
    }

    sleep(6);

    sent = 0;
    receive = 0;
    char size[sizeof(long)];


    //receive file size
    while (receive <= 0) {
        receive = (int) recvfrom(sockfd, size, sizeof(long), 0,
                                 (struct sockaddr *) &serveraddr, &len);
        if (receive == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error receiving file size:\n");
        } else if (receive == 0) {
            printf("Waiting for filename");
        } else {
            fsize = strtol(size, NULL, 10);
            printf("File size received - %ld\n", fsize);
        }
    }


    receive = 0;

    //send acknowledgment of receipt
    while (sent <= 0) {
        sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                            (struct sockaddr *) &serveraddr, len);
        if (sent == -1) {
            fprintf(stderr, "Value of error:  %d\n", errno);
            printf("Error sending acknowledgment:\n");
        } else {
            printf("File Size Acknowledgement sent\n");
        }

    }

    clock_t start, end;
    start = clock();

    //Check to see if acknowledgment was received or not
    while (receive <= 0) {
        end = clock();
        receive = (int) recvfrom(sockfd, check, sizeof(check), 0,
                                 (struct sockaddr *) &serveraddr, &len);
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
                if (strcmp(size, check) == 0) {
                    sent = (int) sendto(sockfd, &receipt, sizeof(long), 0,
                                        (struct sockaddr *) &serveraddr, len);
                    receive = 0;
                }
            }
        }
    }


    int transferring = 1;
    FILE *rfp = fopen(line2, "wb");
    if (rfp == NULL) {
        printf("File not opened. Transfer canceled\n");
        return -1;
    }

    int MAXPAKS = (int) (fsize / MAXBUF) + 1;
    int ack = 0;
    char code[3];
    strcpy(code, "\0");

    char **data_array = malloc(MAXPAKS * sizeof(char *));
    for (int i = 0; i < MAXPAKS; i++) {
        data_array[i] = (char *) malloc(MAXBUF * sizeof(char));
    }

    while (transferring) {
        while (fsize > rsize) {
            sleep(1);
            receive = (int) recvfrom(sockfd, buff, MAXSIZE, 0,
                                     (struct sockaddr *) &serveraddr, &len);
            if (receive > 0) {
                //Get sequence code
                strncpy(code, buff, 2);
                code[3] = (char) "\0";
                ack = (int) strtol(code, NULL, 10);
                printf("\nReceived data packet #%i\n", ack);

                //Move pointer pass sequence code
                char *text = &buff[2];

                //Save data into data array storage
                strcpy(data_array[ack], text);
                rsize += sizeof(buff) - 4;

                //send acknowledgement
                sent = (int) sendto(sockfd, code, sizeof(code) + 1, 0,
                                    (struct sockaddr *) &serveraddr, sizeof(serveraddr));

                fprintf(stdout, "Sent acknowledgment packet\n");

                //reset buffer contents
                memset(buff, 0, MAXSIZE);

            } else {
                printf("%i", receive);
            }
        }
        //Stops transferring loop
        transferring = 0;

        //
        for (int i = 0; i < MAXPAKS; i++) {
            fwrite(data_array[i], sizeof(char), MAXBUF, rfp);
        }

        fclose(rfp);


        //Get file size
        struct stat st;
        stat(line2, &st);
        rsize = (long) st.st_size;
        if (fsize < rsize) {
            printf("\nFile Transfer Complete");
        } else {
            printf("\nData Missing fsize: %ld --- rsize: %ld", fsize, rsize);
        }
    }

    close(sockfd);
    return 0;
}