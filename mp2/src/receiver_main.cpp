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
#include <errno.h>
#include <iostream>

using namespace std;

int syn_ack_bit, fin_ack_bit = 0;
FILE* fp;

typedef struct DataPacket {
    int sequence_number;
    int acknowledgment_number;
    int acknowledgment_bit;
    int fin_bit;
    int bytes_to_write;
    int syn_bit;
    char data[1472];
} DataPacket;

struct sockaddr_in my_address, other_address;
int socket_descriptor, slen;

void diep(char *message) {
    perror(message);
    exit(1);
}

void reliablyReceive(unsigned short int my_UDP_port, char* destination_file) {
    
    slen = sizeof (other_address);

    if ((socket_descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &my_address, 0, sizeof (my_address));
    my_address.sin_family = AF_INET;
    my_address.sin_port = htons(my_UDP_port);
    my_address.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(socket_descriptor, (struct sockaddr*) &my_address, sizeof (my_address)) == -1)
        diep("bind");


    cout << "Socket binding done. Now listening" << endl;

    int num_bytes_received;
    int number_of_chunks_remaining = 0;
    int cur_bytes_read = 0;

    DataPacket received_packet;

	if ((num_bytes_received = recvfrom(socket_descriptor, &received_packet, sizeof(DataPacket), 0,
		(struct sockaddr *)&other_address, (socklen_t *)&slen)) == -1) {
		diep("recvfrom");
	} else {
        cout << "Packet Received" << endl;
        cout << "sequence number of received packet = " << received_packet.sequence_number << endl;
        cout << "Ack number of received packet = " << received_packet.acknowledgment_number << endl;

        DataPacket syn_received_packet;

        memset(&syn_received_packet, 0, sizeof(syn_received_packet));
        syn_received_packet.sequence_number = 56;
        syn_received_packet.syn_bit = 1;
        syn_received_packet.acknowledgment_bit = 1;
        syn_received_packet.acknowledgment_number = received_packet.sequence_number + 1;
        syn_ack_bit += 1;
        sprintf(syn_received_packet.data, "SYN RCVD ack");

        if ((num_bytes_received = sendto(socket_descriptor, &syn_received_packet, sizeof(DataPacket), 0, 
            (struct sockaddr *)&other_address, slen)) == -1) {
            diep("receiver: sendto");
        } 
    }

    DataPacket data_packet;
    DataPacket data_ack_packet;
    int len = 0;

	fp = fopen(destination_file, "w");
    int counter = 1;
    int running = 1;

    while(1) {
        cout << "chunks remaning = " << number_of_chunks_remaining;
        cout << ", bytes read = " << cur_bytes_read << endl;
    

	    if ((counter = recvfrom(socket_descriptor, &data_packet, sizeof(DataPacket), 0,
		    (struct sockaddr *)&other_address, (socklen_t *)&slen)) == -1) {
		    diep("recvfrom");
	    } else {
            if(data_packet.fin_bit || number_of_chunks_remaining == 5){
                memset(&data_ack_packet, 0, sizeof(data_ack_packet));
                data_ack_packet.fin_bit = 1;
                data_ack_packet.acknowledgment_bit = 1;

                if ((num_bytes_received = sendto(socket_descriptor, &data_ack_packet, sizeof(DataPacket), 0, 
                    (struct sockaddr *)&other_address, slen)) == -1) {
                    diep("receiver_main: sendto");
                } 

                fclose(fp);
                break;
            }
            
            if(number_of_chunks_remaining){
                memset(&data_ack_packet, 0, sizeof(data_ack_packet));
                data_ack_packet.sequence_number = 0;
                data_ack_packet.syn_bit = 0;
                data_ack_packet.acknowledgment_bit = 1;
                data_ack_packet.acknowledgment_number = cur_bytes_read;

                if ((num_bytes_received = sendto(socket_descriptor, &data_ack_packet, sizeof(DataPacket), 0, 
                    (struct sockaddr *)&other_address, slen)) == -1) {
                    diep("receiver: sendto");
                } 
                number_of_chunks_remaining ++;
                continue;
            }

            cout << "Sequence number = " << data_packet.sequence_number << endl;
            cout << "ack number = " << data_packet.acknowledgment_number << endl;
    
            if(cur_bytes_read == data_packet.sequence_number && data_packet.syn_bit != 1){
                int len = counter;

                if((data_packet.bytes_to_write != 0 && data_packet.bytes_to_write < sizeof(data_packet.data))){
                    fwrite(data_packet.data, sizeof(char), data_packet.bytes_to_write, fp);
                    cur_bytes_read += data_packet.bytes_to_write;
                    number_of_chunks_remaining = 1;
                    continue;
                }

                fwrite(data_packet.data, sizeof(char), sizeof(data_packet.data), fp);
                cur_bytes_read += sizeof(data_packet.data);

                memset(&data_ack_packet, 0, sizeof(data_ack_packet));
                data_ack_packet.sequence_number = 0;
                data_ack_packet.syn_bit = 0;
                data_ack_packet.acknowledgment_bit = 1;
                data_ack_packet.acknowledgment_number = data_packet.sequence_number + data_packet.bytes_to_write;

                if ((num_bytes_received = sendto(socket_descriptor, &data_ack_packet, sizeof(DataPacket), 0, 
                    (struct sockaddr *)&other_address, slen)) == -1) {
                    diep("receiver: sendto");
                } 
            }   
        }
    }

    close(socket_descriptor);
	printf("%s received.", destination_file);
    return;
}


int main(int argc, char** argv) {
    unsigned short int UDP_port;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    UDP_port = (unsigned short int) atoi(argv[1]);

    reliablyReceive(UDP_port, argv[2]);
}
