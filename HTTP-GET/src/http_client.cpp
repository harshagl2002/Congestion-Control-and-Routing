/*
** client.cpp -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cctype> 
#include <arpa/inet.h>
#include <iostream>
#include <fstream>

using namespace std;

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 4096 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
    string url, protocol, host, port, path;

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
    

	url = argv[1];

    size_t find_double_back_slash = url.find("//");

    protocol = url.substr(0, find_double_back_slash - 1);

	url = url.substr(find_double_back_slash + 2);
    size_t find_path_back_slash = url.find('/');

	if (find_path_back_slash == url.npos) path = "";
	else path = url.substr(find_path_back_slash);

	host = url.substr(0, find_path_back_slash);


    size_t host_colon = host.find(':');



	if (host_colon != host.npos) {
        if (path != "") port = host.substr(host_colon + 1);
		else port = host.substr(host_colon + 1);
		host = host.substr(0, host_colon);
	}
	else port = "80";

	bool flag = false;

    for (char c : host) {
        if (isdigit(c) || c == '.') {
            continue;
        } else {
			flag = true;
            //path = path.substr(1);
            break;
        }
    }

    cout << "port = " << port << endl;
    cout << "path = " << path << endl;
    cout << "host = " << host << endl;


	if ((rv = getaddrinfo(host.data(), port.data(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure
	
	
    string request = "0GET " + path + " HTTP/1.1\r\n" + "User-Agent: Wget/1.12(linux-gnu)\r\n" +
	  				 "Host: " + host + ":" + port + "\r\n" + "Connection: Keep-Alive\r\n\r\n";


	if (flag) request = request.substr(1);
    
    cout << "request = " << request << endl;
    
	if (send(sockfd, request.c_str(), request.size(), 0) == -1) {
        perror("send");
        exit(1);
    };

	ofstream out;
    out.open("output", ios::binary);
    bool header = true;


    while (true) {
        memset(buf, '\0', MAXDATASIZE);
        numbytes = recv(sockfd, buf, MAXDATASIZE, 0);

        
        if (numbytes > 0) {

            if (header) {
				header = false;
                char* head = strstr(buf, "\r\n\r\n") + 4;
                out.write(head, strlen(head));
            } 
			else out.write(buf, sizeof(char) * numbytes);
        } 
		else break;
    }

	out.close(); 

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	close(sockfd);

	return 0;
}