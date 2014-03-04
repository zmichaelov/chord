
#include <stdio.h>
#include "csapp.h"
#include "chord.h"
#include <openssl/sha.h>

const char* my_address = "127.0.0.1";

char * read_request(int connfd) {
    char* request = Malloc(sizeof(char)*MAXBUF);
    rio_t client;
    Rio_readinitb(&client, connfd);
    Rio_readlineb(&client, request, MAXBUF);
    return request;
}


int handle_connection(int connfd) {

    char* request = read_request(connfd);
    printf("%s\n", request);
    char* cmd = strtok(request, "|");
    printf("command: %s\n", cmd);
	if(!strcmp(cmd, "JOIN")) {
        char* ip = strtok(NULL, ":");
        int port = atoi(strtok(NULL, "\r\n"));
        printf("address and port: %s:%d\n", ip, port);

    }
    // free dynamic memory
    Free(request);
    return 0;
}
// join an existing chord ring
void join_chord_ring(char* ip, int port){
    int joinfd = Open_clientfd(ip, port);
    // send bytes request to join
    char request[MAXBUF];
    size_t n = sprintf(request, "JOIN|%s:%d\r\n", ip, port);
    Rio_writep(joinfd, request, n);

    // response should be pointers to successor node in ring
}
int main(int argc, char *argv[]) {
    int join_port, connfd;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp, *join_ip;
    char ip_and_port[21];
    unsigned char myhash[SHA_DIGEST_LENGTH];
    // parse command line options
    if (argc < 2 || argc > 4) {
        printf("Usage: %s listen_port [join_ip] [join_port]\n", argv[0]);
        exit(1);
    } else if (argc == 4) {
        join_ip = argv[2];
        join_port = atoi(argv[3]);
        join_chord_ring(join_ip, join_port);
    }
    // initialize listening
    int listen_port = atoi(argv[1]); // listen for chord connections on this port
    size_t n = sprintf(ip_and_port, "%s:%d", my_address, listen_port);
    //printf("%s\n", ip_and_port);
    SHA1(ip_and_port, n, myhash);
    printf("My hash position is: %x\n", myhash);

    int listenfd = Open_listenfd(listen_port);
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    // wait until quit
    while(1) {// listen for chord messages
        printf("Listening for chord messages on port: %d\n", listen_port);

        int clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd <= 2) {
            continue;
        }

        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
            sizeof(clientaddr.sin_addr.s_addr), AF_INET);

        haddrp = inet_ntoa(clientaddr.sin_addr);
        printf("Accepted connection from %s:%d\n", haddrp, clientaddr.sin_port);
        handle_connection(connfd);// TODO use detached threads

    }
    //char data[] = "JOIN|127.0.0.1:8085";
    //size_t length = sizeof(data);
    //unsigned char hash[SHA_DIGEST_LENGTH];
    //SHA1(data, length, hash);
    // hash now contains the 20-byte SHA-1 hash
    //printf("%s\n", hash);
    //printf("%d\n", SHA_DIGEST_LENGTH);
}


