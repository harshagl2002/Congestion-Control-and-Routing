/*
* Authors : Harsh Agarwal, Siddhant Nanavati
* Date - 08/03/2024
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <sys/time.h>
#include <errno.h>

using namespace std;

struct sockaddr_in other_address;
int socket_descriptor, address_length;
int fin_ack_bit, fin_bit, syn_ack_bit = 0;
FILE *fp;

void diep(char *message) {
    perror(message);
    exit(1);
}

enum status_t{
    SLOW_START,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

enum packet_t{
    DATA,
    ACK,
    FIN,
    FINACK
};

typedef struct datagram {
    int sequence_number;
    int acknowledgment_number;
    int acknowledgment_bit;
    int fin_bit;
    int bytes_to_write;
    int syn_bit;
    char data[1472];
}  datagram;

datagram their_datagram;

void set_timeout(int usec){
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = usec; 
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ) {
        diep("setsockopt failed");
        
    }
}

void reliablyTransfer(char* hostname, unsigned short int host_UDP_port, char* filename, unsigned long long int bytes_to_transfer) {
    
    
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    address_length = sizeof (other_address);

    if ((socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &other_address, 0, sizeof (other_address));
    other_address.sin_family = AF_INET;
    other_address.sin_port = htons(host_UDP_port);
    if (inet_aton(hostname, &other_address.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    int numbytes;
    while(1) {
        datagram listen_datagram;
        memset(&listen_datagram, 0, sizeof(listen_datagram));
        listen_datagram.sequence_number = 55;
        listen_datagram.syn_bit = 1;
        sprintf(listen_datagram.data, "First Listen Signal");

        if ((numbytes = sendto(socket_descriptor, (void *)&listen_datagram, sizeof(datagram), 0, (struct sockaddr *)&other_address, address_length)) < 0)  {
            diep("sendto");
            
        } else {
            datagram syn_sent_datagram;
            memset(&syn_sent_datagram, 0, sizeof(syn_sent_datagram));
            syn_sent_datagram.sequence_number = 55;
            syn_sent_datagram.syn_bit = 1;

            set_timeout(40000);

            if ((numbytes = recvfrom(socket_descriptor, &syn_sent_datagram, sizeof(datagram), 0, (struct sockaddr *)&other_address, (socklen_t *)&address_length)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cout << "timing out due to message drop" << endl;
                    continue;
                }
                diep("recvfrom");
                exit(1);
            }
            cout << "HANDSHAKE DONE TCP CONNECTION ESTABLISHED" << endl;
            syn_ack_bit += 1;
            break;
        }
    }
    
    datagram data_datagram;
    int read_count = sizeof(data_datagram.data);
    int i = 0;
    int cwnd = 30;
    int sequence_number = 0;
    int sequence_number_acked = 0;
    const int bytes_to_write = bytes_to_transfer;
    int bytes_left_acked = bytes_to_write;
    int bytes_left = bytes_to_write;

    while(sequence_number_acked != bytes_to_write) {
        memset(&data_datagram, 0, sizeof(data_datagram));
        data_datagram.sequence_number = sequence_number;
        data_datagram.syn_bit = 0;
        data_datagram.acknowledgment_bit = 0;
        data_datagram.acknowledgment_number = 0;
        data_datagram.bytes_to_write = sizeof(data_datagram.data);

        if(bytes_left < sizeof(data_datagram.data)){
            data_datagram.bytes_to_write = bytes_left;
            read_count = fread((void *)(data_datagram.data), 1, bytes_left, fp);
            if ((numbytes = sendto(socket_descriptor, &data_datagram, sizeof(datagram), 0, 
            (struct sockaddr *)&other_address, address_length)) == -1)  {
                diep("receiver_main: sendto");
            } 
        } else {
            printf("cur seq num %d\n", sequence_number);
            read_count = fread((void *)(data_datagram.data), 1, sizeof(data_datagram.data), fp);
            if ((numbytes = sendto(socket_descriptor, &data_datagram, sizeof(datagram), 0, 
                (struct sockaddr *)&other_address, address_length)) == -1) {
                diep("receiver_main: sendto");
            } 
        }

        sequence_number += sizeof(data_datagram.data);
        bytes_left -= sizeof(data_datagram.data);
        i++;
        if(i == cwnd) {
            i = 0;
            for (int j = 0; j < cwnd; j++) {
                set_timeout(40000);
                if ((numbytes = recvfrom(socket_descriptor, &their_datagram, sizeof(datagram) , 0,
                    (struct sockaddr *)&other_address, (socklen_t *)&address_length)) == -1)  {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        cout << "Timing out" << endl;
                        continue;
                    }
                    diep("recvfrom");
                    exit(1);
                }   else { 
                    cout << "Sequence number = " << their_datagram.sequence_number << endl;
                    cout << "ack number = " << their_datagram.acknowledgment_number << endl;
                    sequence_number_acked = their_datagram.acknowledgment_number;
                    if(sequence_number_acked == bytes_to_write){
                        break;
                    }
                }
            }    
            if(sequence_number_acked == bytes_to_write) break;
            bytes_left_acked = bytes_to_write - sequence_number_acked;
            cout << "remaining bytes = " << bytes_left << endl;

            bytes_left = bytes_left_acked;
            sequence_number = sequence_number_acked;
            fseek(fp, sequence_number_acked, SEEK_SET);
            continue;
        }    
    }


    int fin_count = 5;
    while(fin_count){
        fin_count--;
        datagram fin_datagram;
        memset(&fin_datagram, 0, sizeof(fin_datagram));
        fin_datagram.fin_bit = 1;
            if ((numbytes = sendto(socket_descriptor, &fin_datagram, sizeof(datagram), 0, 
            (struct sockaddr *)&other_address, address_length)) == -1) {
            diep("receiver_main: sendto");
        }
        
        set_timeout(40000);

        datagram fin_ack;

        if ((numbytes = recvfrom(socket_descriptor, &fin_ack, sizeof(datagram) , 0,
                    (struct sockaddr *)&other_address, (socklen_t *)&address_length)) == -1)  {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        cout << "recvfrom error" << endl;
                        continue;
                    } 
        }

        fin_bit += 1;
        cout  << "fin_bit = " << fin_ack.fin_bit << endl;

        if(fin_ack.fin_bit){
            break;
        }
    }

    cout << "Data transferred, closing socket" << endl;
    close(socket_descriptor);
    return;
}

int main(int argc, char** argv) {
    unsigned short int UDP_port;
    unsigned long long int num_bytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    UDP_port = (unsigned short int) atoi(argv[2]);
    num_bytes = atoll(argv[4]);

    reliablyTransfer(argv[1], UDP_port, argv[3], num_bytes);

    return (EXIT_SUCCESS);
}
