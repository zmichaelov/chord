
#include <stdio.h>
#include "csapp.h"
#include "chord.h"
#include <openssl/sha.h>

const char* my_address = "127.0.0.1";

int handle_connection(int connfd) {
    char request[MAXBUF];
    rio_t client;
    Rio_readinitb(&client, connfd);

    Rio_readlineb(&client, request, MAXBUF);
    printf("%s\n", request);
    return 0;
}
int main(int argc, char *argv[]) {
    int join_ip, join_port, connfd;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp;
    char ip_and_port[21];
    unsigned char myhash[SHA_DIGEST_LENGTH];
    // parse command line options
    if (argc < 2 || argc > 4) {
        printf("Usage: %s listen_port [join_ip] [join_port]\n", argv[0]);
        exit(1);
    } //else if (argc == 2) {

   // }
    else if (argc == 4) {
        join_ip = atoi(argv[2]);
        join_port = atoi(argv[3]);
    }
    // initialize listening
    int listen_port = atoi(argv[1]); // listen for chord connections on this port
    size_t n = sprintf(ip_and_port, "%s:%d", my_address, listen_port);
    printf("%s\n", ip_and_port);
    SHA1(ip_and_port, n, myhash);
    printf("My hash position is: %02x\n", myhash);

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
        handle_connection(connfd);

    }
    //char data[] = "JOIN|127.0.0.1:8085";
    //size_t length = sizeof(data);
    //unsigned char hash[SHA_DIGEST_LENGTH];
    //SHA1(data, length, hash);
    // hash now contains the 20-byte SHA-1 hash
    //printf("%s\n", hash);
    //printf("%d\n", SHA_DIGEST_LENGTH);
}


