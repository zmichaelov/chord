
#include <stdio.h>
#include "csapp.h"
#include "chord.h"
#include <openssl/sha.h>

chord_node prev2;
chord_node prev;
chord_node me;
chord_node next;
chord_node next2;
char * read_request(int connfd) {
    char* request = Malloc(sizeof(char)*MAXBUF);
    rio_t client;
    Rio_readinitb(&client, connfd);
    Rio_readlineb(&client, request, MAXBUF);
    return request;
}

void handle_join(char* address, int port, int hash) {
    if(hash < me.hash) {// found correct position in ring
        // send necessary pointers to
        int fd = Open_clientfd(address, port);// open connection to ring
        // send bytes request to join
        char request[MAXBUF];
        char temp[1024];
        // give the ring our ip address, port, and hash
        size_t n = 0;
        n += sprintf(temp, "UPDATE|next:%s:%d:%u",me.address , me.port, me.hash);
        strcat(request, temp);
        n += sprintf(temp, "|prev:%s:%d:%u", prev.address , prev.port, prev.hash);
        strcat(request, temp);
        n += sprintf(temp, "|next2:%s:%d:%u",next.address , next.port, next.hash);
        strcat(request, temp);
        n += sprintf(temp, "|prev2:%s:%d:%u\r\n",prev2.address , prev2.port, prev2.hash);
        strcat(request, temp);
        //printf("request: %s", request);
        Rio_writep(fd, request, n);
    } else {
        // forward join request to our successor
        int nextfd = Open_clientfd(next.address, next.port);// open connection to ring
        // send bytes request to join
        char request[MAXBUF];
        // give the ring our ip address, port, and hash
        size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", address, port, hash);
        Rio_writep(nextfd, request, n);
    }
}

void process_update (char* request){
    char* save;
    char* name = strtok_r(request, ":", &save);
    char* ip = strtok_r(NULL, ":", &save);
    int port = atoi(strtok_r(NULL, ":", &save));
    int hash = atoi(strtok_r(NULL, ":", &save));
    if(!strcmp(name, "prev2")) {
        prev2.address = ip;
        prev2.port = port;
        prev2.hash= hash;
    } else if(!strcmp(name, "prev")){
        prev.address = ip;
        prev.port = port;
        prev.hash= hash;
    } else if(!strcmp(name, "next")){
        next.address = ip;
        next.port = port;
        next.hash= hash;
    } else if(!strcmp(name, "next2")){
        next2.address = ip;
        next2.port = port;
        next2.hash= hash;
    }

}
int handle_connection(int connfd) {

    char* request = read_request(connfd);
    printf("original request: %s\n", request);
    char* saveptr;
    char* cmd = strtok_r(request, "|", &saveptr);
    printf("command: %s\n", cmd);
    if(!strcmp(cmd, "JOIN")) {// a new node wants to join the ring
        char* ip = strtok_r(NULL, ":", &saveptr);
        int port = atoi(strtok_r(NULL, "|", &saveptr));
        int hash = atoi(strtok_r(NULL, "|", &saveptr));
        printf("address, port, hash: %s:%d|%u\n", ip, port, hash);
        handle_join(ip, port, hash);
    } else if (!strcmp(cmd, "UPDATE")) {
        char* ptr = cmd;
        char* req = Malloc(sizeof(char)*256);
        while (ptr != NULL) {// parse out individual update requests
            ptr = strtok_r(NULL, "|", &saveptr);// strip out one update request
            if (ptr == NULL){
                break;
            }
            strcpy(req, ptr);
            //printf("request: %s\n", req);
            process_update(req);
        }
        Free(req);
    } else if (!strcmp(cmd, "KEEP-ALIVE")){

    } else if (!strcmp(cmd, "LEAVE")){

    } else if (!strcmp(cmd, "SEARCH")){

    }
    // free dynamic memory
    Free(request);
    return 0;
}
// join an existing chord ring
void join_chord_ring(char* ip, int port){

    int joinfd = Open_clientfd(ip, port);// open connection to ring
    // send bytes request to join
    char request[8192];
    // give the ring our ip address, port, and hash
    printf("myhash: %u\n", me.hash);
    size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", me.address, me.port, me.hash);
    printf("myrequest: %s\n", request);
    Rio_writep(joinfd, request, n);
}

unsigned int hash_to_int(unsigned char* b){
    unsigned int h = 0;
    for (int i = 0; i < 20; i += 4){
        int x = 0;
        x = b[i];
        x = (x << 8) + b[i+1];
        x = (x << 8) + b[i+2];
        x = (x << 8) + b[i+3];
        if (h == 0){
            h = x;
        } else {
            h ^= x;
        }
    }
    return h;
}
int main(int argc, char *argv[]) {
    char ip_and_port[21];
    // parse command line options
    if (argc != 3 && argc != 5) {
        printf("Usage: %s listen_address listen_port [join_ip] [join_port]\n", argv[0]);
        exit(1);
    }

    char* listen_address= argv[1];
    int listen_port = atoi(argv[2]); // listen for chord connections on this port
    // generate our hash
    size_t n = sprintf(ip_and_port, "%s%d", listen_address, listen_port);
    unsigned char myhash[SHA_DIGEST_LENGTH];
    SHA1(ip_and_port, n, myhash);
    unsigned int newhash = hash_to_int(&myhash);
    printf("My hash position is: %u\n", newhash);
    // listen for connections
    int listenfd = Open_listenfd(listen_port);
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

    me.address = listen_address;
    me.port = listen_port;
    me.hash = newhash;
    //strcpy(me.hash,myhash);

    if (argc == 5) {// join an existing ring
        char* join_ip = argv[3];
        int join_port = atoi(argv[4]);
        join_chord_ring(join_ip, join_port);
    } else {// starting new ring
        prev2 = me;
        prev = me;
        next = me;
        next2 = me;
    }
    int connfd;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp ;
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
        //printf("Accepted connection from %s:%d\n", haddrp, clientaddr.sin_port);
        handle_connection(connfd);// TODO use detached threads

    }
}


