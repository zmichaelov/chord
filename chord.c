
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
// tell the chord node defined my old to update
void send_update(chord_node old, chord_node new){

}


void handle_join(char* address, int port,unsigned int hash) {
    if( (me.hash == next.hash) || (hash < me.hash && prev.hash < hash) ||
        (hash < me.hash && prev.hash > me.hash)) {// found correct position in ring
        // send necessary pointers to
        int fd = Open_clientfd(address, port);// open connection to ring
        // send bytes request to join
        char request[MAXBUF] = "";
        char temp[128] = "";
        char update[128] = "";
        printf("Initial request buffer: %s\n", request);
        // give the ring our ip address, port, and hash
        size_t n = 0;
        n += sprintf(temp, "UPDATE|next:%s:%d:%u",me.address , me.port, me.hash);
        strcat(request, temp);
        n += sprintf(temp, "|prev:%s:%d:%u", prev.address , prev.port, prev.hash);
        strcat(request, temp);
        // the next2 and prev2 pointers have special cases for updating
        // Case 1: joining a 1-node ring
        if (prev2.hash == prev.hash) {
            // do nothing
            // next2 and prev2 pointers don't change, or should be set to themselves
        // Case 2: joining a 2-node ring
        } else if (prev.hash == next.hash){
            // new node's next2 should point to our next
            printf("Joining 2-node ring!\n");
            n += sprintf(temp, "|next2:%s:%d:%u", next.address , next.port, next.hash);
            strcat(request, temp);
            // new node's prev2 should point to us
            n += sprintf(temp, "|prev2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            strcat(request, temp);

            // tell our next to update its prev2 and next2
            int nextfd = Open_clientfd(next.address, next.port);
            int n2 = sprintf(temp, "UPDATE|prev2:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            strcat(update, temp);
            Rio_writep(nextfd, update, n2);
            Close(nextfd);

            // tell our prev2 to update its next2
            int prev2fd = Open_clientfd(prev2.address, prev2.port);
            n2 = sprintf(update, "UPDATE|next2:%s:%d:%u\r\n", address , port, hash);
            Rio_writep(prev2fd, update, n2);
            Close(prev2fd);

            // our next2 points to the new node
            strcpy(next2.address, address);
            next2.port = port;
            next2.hash = hash;
        // Case 3: joining a 3-node ring
        } else if (next.hash == prev2.hash){
            n += sprintf(temp, "|next2:%s:%d:%u", next.address , next.port, next.hash);
            strcat(request, temp);
            n += sprintf(temp, "|prev2:%s:%d:%u\r\n", prev2.address , prev2.port, prev2.hash);
            strcat(request, temp);

            // tell our next to update its prev2 and next2
            int nextfd = Open_clientfd(next.address, next.port);
            int n2 = sprintf(temp, "UPDATE|prev2:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", address , port, hash);
            strcat(update, temp);
            Rio_writep(nextfd, update, n2);
            Close(nextfd);

            // tell our next2 to update its next and next2
            int next2fd = Open_clientfd(next2.address, next2.port);
            memset(update, 0, 128);
            n2 = 0;
            n2 += sprintf(temp, "UPDATE|next:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            strcat(update, temp);
            Rio_writep(next2fd, update, n2);
            Close(next2fd);
        // Case 4: joining a 4-node or more ring
        } else {
            n += sprintf(temp, "|next2:%s:%d:%u", next.address , next.port, next.hash);
            strcat(request, temp);
            n += sprintf(temp, "|prev2:%s:%d:%u\r\n", prev2.address , prev2.port, prev2.hash);
            strcat(request, temp);

            // tell our next to update its prev2
            int nextfd = Open_clientfd(next.address, next.port);
            int n2 = sprintf(update, "UPDATE|prev2:%s:%d:%u\r\n", address , port, hash);
            Rio_writep(nextfd, update, n2);
            Close(nextfd);

            // tell our prev to update its next2
            int prevfd = Open_clientfd(prev.address, prev.port);
            memset(update, 0, 128);
            n2 = sprintf(update, "UPDATE|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            Rio_writep(prevfd, update, n2);
            Close(prevfd);

            // tell our prev2 to update its next2
            int next2fd = Open_clientfd(prev2.address, prev2.port);
            memset(update, 0, 128);
            n2 = 0;
            n2 += sprintf(temp, "UPDATE|next2:%s:%d:%u\r\n", address , port, hash);
            strcat(update, temp);
            Rio_writep(next2fd, update, n2);
            Close(next2fd);
        }
        printf("Update to send to new node: %s\n", request);
        Rio_writep(fd, request, n);// send update request to joining node
        Close(fd);

        // tell predecessor to update its next pointer
        fd = Open_clientfd(prev.address, prev.port);// open connection to ring
        n = sprintf(request, "UPDATE|next:%s:%d:%u\r\n",address , port, hash);
        Rio_writep(fd, request, n);// send update request to joining node
        Close(fd);


        // update our pointers
        // set our prev2 to our old prev
        strcpy(prev2.address, prev.address);
        prev2.port = prev.port;
        prev2.hash = prev.hash;
        // set our prev to the new guy
        strcpy(prev.address, address);
        prev.port = port;
        prev.hash = hash;


    } else {
        // forward join request to our successor
        printf("Forwarding Request to %s:%d\n", next.address, next.port);
        int nextfd = Open_clientfd(next.address, next.port);// open connection to ring
        // send bytes request to join
        char request[64];
        // give the ring our ip address, port, and hash
        size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", address, port, hash);
        Rio_writep(nextfd, request, n);
        Close(nextfd);
    }
}

void process_update (char* request){
    char* save;
    char* name = strtok_r(request, ":", &save);
    char* ip = strtok_r(NULL, ":", &save);
    //char* ip = Malloc(strlen(temp));
    //strcpy(ip, temp);
    int port = atoi(strtok_r(NULL, ":", &save));
    int hash = atoi(strtok_r(NULL, ":", &save));
    if(!strcmp(name, "prev2")) {
        strcpy(prev2.address, ip);
        prev2.port = port;
        prev2.hash= hash;
    } else if(!strcmp(name, "prev")){
        strcpy(prev.address, ip);
        prev.port = port;
        prev.hash= hash;
    } else if(!strcmp(name, "next")){
        strcpy(next.address, ip);
        next.port = port;
        next.hash= hash;
    } else if(!strcmp(name, "next2")){
        strcpy(next2.address, ip);
        next2.port = port;
        next2.hash= hash;
    }

}
int handle_connection(int connfd) {

    char* request = read_request(connfd);
    printf("request: %s\n", request);
    char* saveptr;
    char* cmd = strtok_r(request, "|", &saveptr);
    if(!strcmp(cmd, "JOIN")) {// a new node wants to join the ring
        char* ip = strtok_r(NULL, ":", &saveptr);
        int port = atoi(strtok_r(NULL, "|", &saveptr));
        int hash = atoi(strtok_r(NULL, "|", &saveptr));
        handle_join(ip, port, hash);
    } else if (!strcmp(cmd, "UPDATE")) {
        char* ptr = cmd;
        char* req = Malloc(sizeof(char)*256);
        while (1) {// parse out individual update requests
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
    char request[1024];
    // give the ring our ip address, port, and hash
    size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", me.address, me.port, me.hash);
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

    char* listen_address = argv[1];
    int listen_port = atoi(argv[2]); // listen for chord connections on this port
    // generate our hash
    size_t n = sprintf(ip_and_port, "%s%d", listen_address, listen_port);
    unsigned char myhash[SHA_DIGEST_LENGTH];
    SHA1(ip_and_port, n, myhash);
    unsigned int newhash = hash_to_int(&myhash);
    // listen for connections
    int listenfd = Open_listenfd(listen_port);
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));

    //me.address = listen_address;
    strcpy(me.address, listen_address);
    me.port = listen_port;
    me.hash = newhash;
    prev2 = me;
    prev = me;
    next = me;
    next2 = me;

    if (argc == 5) {// join an existing ring
        char* join_ip = argv[3];
        int join_port = atoi(argv[4]);
        join_chord_ring(join_ip, join_port);
    } else {
    }

    int connfd;
    struct sockaddr_in clientaddr;
    struct hostent *hp;
    char *haddrp ;
    while(1) {// listen for chord messages
        printf("prev2: %u\nprev: %u\nme: %u\nnext: %u\nnext2: %u\n\n\n", prev2.hash,
                prev.hash, me.hash, next.hash, next2.hash);
        int clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd <= 2) {
            continue;
        }

        hp = Gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
            sizeof(clientaddr.sin_addr.s_addr), AF_INET);

        haddrp = inet_ntoa(clientaddr.sin_addr);
        handle_connection(connfd);// TODO use detached threads

    }
}


