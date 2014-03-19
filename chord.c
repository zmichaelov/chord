
#include <stdio.h>
#include "csapp.h"
#include "chord.h"
#include <openssl/sha.h>
#include <pthread.h>
chord_node prev2;
chord_node prev;
chord_node me;
chord_node next;
chord_node next2;
// our finger tables
chord_node shortcuts[32];
int pending = 0;
int updated_pointers = 0;
pthread_mutex_t mutex;
struct timeval t1, t2;
long int elapsedTime = 0;

void log_pointers(){
    printf("prev2: %u\nprev: %u\nme: %u\nnext: %u\nnext2: %u\n\n", prev2.hash,
            prev.hash, me.hash, next.hash, next2.hash);

}
char * read_request(int connfd) {
    char* request = Malloc(sizeof(char)*MAXBUF);
    rio_t client;
    Rio_readinitb(&client, connfd);
    Rio_readlineb(&client, request, MAXBUF);
    return request;
}
void reinit_keepalive() {
    pthread_mutex_lock(&mutex);
    updated_pointers = 1;
    pthread_mutex_unlock(&mutex);
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
unsigned int get_hash(char* search) {
    // found correct position
    unsigned char myhash[SHA_DIGEST_LENGTH];
    SHA1(search, strlen(search), myhash);
    return hash_to_int(&myhash);
}

int append_update(char* request, const char* ptr, chord_node node){
    size_t n = 0;
    char temp[128] = "";
    n += sprintf(temp, "|%s:%s:%d:%u", ptr, node.address , node.port, node.hash);
    strcat(request, temp);
    return n;
}
// pass request to our next node
void forward_request (char* request, size_t n){
    int nextfd = open_clientfd(next.address, next.port);
    rio_writep(nextfd, request, n);
    close(nextfd);
}
// determine if we are the correct bucket for this resource
int found_correct_bucket(unsigned int resource) {
    return (me.hash == next.hash) || // one node ring
           (resource < me.hash && prev.hash < resource) ||// regular case
           (resource < me.hash && prev.hash > me.hash) || // edge case: we are the start of the ring
           (resource > prev.hash && prev.hash > me.hash); // edge case: we are the start of the ring
}

void handle_search(char* address, int port, char* search) {
    unsigned int hash = get_hash(search);
    printf("Hash for %s = %u\n",search, hash);
    if (found_correct_bucket(hash)) {// found correct position in ring
        //printf("Found correct bucket for search term: %s\n", search);
        char* request = Malloc(sizeof(char)*64);
        size_t n = sprintf(request, "FOUND|%s:%u\r\n",search, me.hash);
        printf("address: %s, port: %d\n", address, port);
        int foundfd = open_clientfd(address, port);
        rio_writep(foundfd, request, n);
        Free(request);
        close(foundfd);
    } else {
        char* request = Malloc(sizeof(char)*64);
        size_t n = sprintf(request, "SEARCH|%s:%d:%s\r\n",search, port, address);
        forward_request(request, n);
        Free(request);
        printf("Forwarding Search Request to %s:%d\n", next.address, next.port);
    }
}

void handle_leave() {
    char update[MAXBUF] = "UPDATE";
    // Case 1: leaving a 1-node ring
    if (prev2.hash == prev.hash) {
        return;
    // Case 2: leaving a 2-node ring
    } else if (prev.hash == next.hash){
        // prev and next should be reset it itself
        int prevfd = open_clientfd(prev.address, prev.port);
        append_update(update, "prev", prev);
        append_update(update, "next", next);
        strcat(update, "\r\n");
        printf("leave request for 2: %s", update);
        rio_writep(prevfd, update, strlen(update));
        close(prevfd);

    // Case 3: leaving a 3-node ring
    } else if (next.hash == prev2.hash){
        int nextfd = open_clientfd(next.address, next.port);
        append_update(update, "prev2", next);
        append_update(update, "prev", prev);
        append_update(update, "next2", next);
        strcat(update, "\r\n");
        rio_writep(nextfd, update, strlen(update));
        close(nextfd);

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        int prevfd = open_clientfd(prev.address, prev.port);
        append_update(update, "prev2", prev);
        append_update(update, "next", next);
        append_update(update, "next2", prev);
        strcat(update, "\r\n");
        rio_writep(prevfd, update, strlen(update));
        close(prevfd);
    // Case 4: leaving a 4-node or more ring
    } else {
        int nextfd = open_clientfd(next.address, next.port);
        append_update(update, "prev2", prev2);
        append_update(update, "prev", prev);
        strcat(update, "\r\n");
        rio_writep(nextfd, update, strlen(update));
        close(nextfd);

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        int prevfd = open_clientfd(prev.address, prev.port);
        append_update(update, "next", next);
        append_update(update, "next2", next2);
        strcat(update, "\r\n");
        rio_writep(prevfd, update, strlen(update));
        close(prevfd);

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        int prev2fd = open_clientfd(prev2.address, prev2.port);
        append_update(update, "next2", next);
        strcat(update, "\r\n");
        rio_writep(prev2fd, update, strlen(update));
        close(prev2fd);

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        int next2fd = open_clientfd(next2.address, next2.port);
        append_update(update, "prev2", prev);
        strcat(update, "\r\n");
        rio_writep(next2fd, update, strlen(update));
        close(next2fd);
    }

}
void handle_join(char* address, int port,unsigned int hash) {
    if (found_correct_bucket(hash)) {
        char request[MAXBUF] = "";
        char temp[128] = "";
        char update[128] = "";
    //    printf("Initial request buffer: %s\n", request);
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
            int nextfd = open_clientfd(next.address, next.port);
            int n2 = sprintf(temp, "UPDATE|prev2:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            strcat(update, temp);
            rio_writep(nextfd, update, n2);
            close(nextfd);

            // tell our prev2 to update its next2
            int prev2fd = open_clientfd(prev2.address, prev2.port);
            n2 = sprintf(update, "UPDATE|next2:%s:%d:%u\r\n", address , port, hash);
            rio_writep(prev2fd, update, n2);
            close(prev2fd);

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
            int nextfd = open_clientfd(next.address, next.port);
            int n2 = sprintf(temp, "UPDATE|prev2:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", address , port, hash);
            strcat(update, temp);
            rio_writep(nextfd, update, n2);
            close(nextfd);

            // tell our next2 to update its next and next2
            int next2fd = open_clientfd(next2.address, next2.port);
            memset(update, 0, 128);
            n2 = 0;
            n2 += sprintf(temp, "UPDATE|next:%s:%d:%u", address , port, hash);
            strcat(update, temp);
            n2 += sprintf(temp, "|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            strcat(update, temp);
            rio_writep(next2fd, update, n2);
            close(next2fd);
        // Case 4: joining a 4-node or more ring
        } else {
            n += sprintf(temp, "|next2:%s:%d:%u", next.address , next.port, next.hash);
            strcat(request, temp);
            n += sprintf(temp, "|prev2:%s:%d:%u\r\n", prev2.address , prev2.port, prev2.hash);
            strcat(request, temp);

            // tell our next to update its prev2
            int nextfd = open_clientfd(next.address, next.port);
            int n2 = sprintf(update, "UPDATE|prev2:%s:%d:%u\r\n", address , port, hash);
            rio_writep(nextfd, update, n2);
            close(nextfd);

            // tell our prev to update its next2
            int prevfd = open_clientfd(prev.address, prev.port);
            memset(update, 0, 128);
            n2 = sprintf(update, "UPDATE|next2:%s:%d:%u\r\n", me.address , me.port, me.hash);
            rio_writep(prevfd, update, n2);
            close(prevfd);

            // tell our prev2 to update its next2
            int next2fd = open_clientfd(prev2.address, prev2.port);
            memset(update, 0, 128);
            n2 = 0;
            n2 += sprintf(temp, "UPDATE|next2:%s:%d:%u\r\n", address , port, hash);
            strcat(update, temp);
            rio_writep(next2fd, update, n2);
            close(next2fd);
        }
        printf("Update to send to new node: %s\n", request);
        int fd = open_clientfd(address, port);// open connection to ring
        int r;
        while((r = rio_writep(fd, request, n)) < 0){
            printf("write value: %d\n", r);
        };// send update request to joining node
        close(fd);

        // tell predecessor to update its next pointer
        fd = open_clientfd(prev.address, prev.port);// open connection to ring
        n = sprintf(request, "UPDATE|next:%s:%d:%u\r\n",address , port, hash);
        rio_writep(fd, request, n);// send update request to joining node
        close(fd);


        // update our pointers
        // set our prev2 to our old prev
        strcpy(prev2.address, prev.address);
        prev2.port = prev.port;
        prev2.hash = prev.hash;
        // set our prev to the new guy
        strcpy(prev.address, address);
        prev.port = port;
        prev.hash = hash;

        // tell keepalive connections to re-initialize
        log_pointers();
        reinit_keepalive();
    } else {
        // forward join request to our successor
        printf("Forwarding Request to %s:%d\n", next.address, next.port);
        int nextfd = open_clientfd(next.address, next.port);// open connection to ring
        // send bytes request to join
        char request[64];
        // give the ring our ip address, port, and hash
        size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", address, port, hash);
        rio_writep(nextfd, request, n);
        close(nextfd);
    }
}

void rpc() { // for node after the one that died
    // update ourselves
    char update[MAXBUF] = "UPDATE";

    int prev2fd = open_clientfd(prev2.address, prev2.port);
    append_update(update, "next2", next);
    strcat(update, "\r\n");
    rio_writep(prev2fd, update, strlen(update));
    close(prev2fd);

    memset(update, 0, MAXBUF);
    strcat(update, "UPDATE");
    int next2fd = open_clientfd(next.address, next.port);
    append_update(update, "prev2", prev2);
    strcat(update, "\r\n");
    rio_writep(next2fd, update, strlen(update));
    close(next2fd);
    reinit_keepalive();
}
void handle_timeout(){// what to do when next becomes unresponsive
    char update[MAXBUF] = "UPDATE";
    // Case 1: leaving a 1-node ring
    if (prev2.hash == prev.hash) {
    // Case 2: leaving a 2-node ring
    } else if (prev.hash == next.hash){
        strcpy(prev.address, me.address);
        prev.port = me.port;
        prev.hash = me.hash;

        strcpy(next.address, me.address);
        next.port = me.port;
        next.hash = me.hash;
    // Case 3: leaving a 3-node ring
    } else if (next.hash == prev2.hash){

        strcpy(prev2.address, me.address);
        prev2.port = me.port;
        prev2.hash = me.hash;

        strcpy(next.address, next2.address);
        next.port = next2.port;
        next.hash = next2.hash;

        strcpy(next2.address, me.address);
        next2.port = me.port;
        next2.hash = me.hash;


        int prevfd = open_clientfd(prev.address, prev.port);
        append_update(update, "prev2", prev);
        append_update(update, "prev", me);
        append_update(update, "next2", prev);
        strcat(update, "\r\n");
        rio_writep(prevfd, update, strlen(update));
        close(prevfd);

    // Case 4: leaving a 4-node or more ring
    } else {
        // put the rpc call first
        strcat(update, "|RPC\r\n"); // send RPC call to our next2
        int next2fd = open_clientfd(next2.address, next2.port);
        rio_writep(next2fd, update, strlen(update));
        close(next2fd);

        //remaingin updates
        strcpy(next.address, next2.address);
        next.port = next2.port;
        next.hash = next2.hash;

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        int prevfd = open_clientfd(prev.address, prev.port);
        append_update(update, "next2", next2);
        strcat(update, "\r\n");
        rio_writep(prevfd, update, strlen(update));
        close(prevfd);

        memset(update, 0, MAXBUF);
        strcat(update, "UPDATE");
        next2fd = open_clientfd(next2.address, next2.port);
        append_update(update, "prev", me);
        append_update(update, "prev2", prev);
        rio_writep(next2fd, update, strlen(update));
        close(next2fd);
    }

    log_pointers();
    //reinit_keepalive();
}

void process_update (char* request){
    if(!strcmp(request, "RPC\r\n")) {
        rpc();
        return;
    }
    char* save;
    char* name = strtok_r(request, ":", &save);
    char* ip = strtok_r(NULL, ":", &save);
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
        //reinit_keepalive();
    } else if(!strcmp(name, "next")){
        strcpy(next.address, ip);
        next.port = port;
        next.hash= hash;
        reinit_keepalive();
    } else if(!strcmp(name, "next2")){
        strcpy(next2.address, ip);
        next2.port = port;
        next2.hash= hash;
    }
}
int handle_connection(int connfd) {

    char* request = read_request(connfd);
    //printf("request: %s\n", request);
    char* saveptr;
    char* cmd = strtok_r(request, "|", &saveptr);
    if(!strcmp(cmd, "JOIN")) {// a new node wants to join the ring
        char* ip = strtok_r(NULL, ":", &saveptr);
        int port = atoi(strtok_r(NULL, "|", &saveptr));
        int hash = atoi(strtok_r(NULL, "|", &saveptr));
        handle_join(ip, port, hash);
    } else if (!strcmp(cmd, "UPDATE")) {
        char* ptr = cmd;
        char* req = Malloc(sizeof(char)*MAXBUF);
        while (1) {// parse out individual update requests
            ptr = strtok_r(NULL, "|", &saveptr);// strip out one update request
            if (ptr == NULL){
                break;
            }
            strcpy(req, ptr);
            printf("request: %s\n", req);
            process_update(req);
        }
        Free(req);
        log_pointers();
    } else if (!strcmp(cmd, "PONG")){
        // handle response for prev
        pthread_mutex_lock(&mutex);

            gettimeofday(&t2, NULL); // stop timer
            elapsedTime = (t2.tv_sec - t1.tv_sec);
            if (elapsedTime > TIMEOUT ) {
                printf("Timeout detected on our successor!\n");

                // initiate splicing of node
                handle_timeout();
            }
            pending = 0; // we have finished processing ping-pong
            elapsedTime = 0;
        pthread_mutex_unlock(&mutex);

    } else if (!strcmp(cmd, "PING")){
        //respond with pong
        char* ip = strtok_r(NULL, ":", &saveptr);
        int port = atoi(strtok_r(NULL, "|", &saveptr));
        int hash = atoi(strtok_r(NULL, "|", &saveptr));

        char request[64] = "";
        size_t n = sprintf(request, "PONG|%s:%d|%u\r\n", me.address, me.port, me.hash);
        int returnfd = open_clientfd(ip, port);
        rio_writep(returnfd, request, n);
        close(returnfd);
    } else if (!strcmp(cmd, "FOUND")){// FOUND|query:hash\r\n
        char* query = strtok_r(NULL, ":", &saveptr);
        int hash = atoi(strtok_r(NULL, ":", &saveptr));
        printf("Found %s at node %u\n", query, hash);
    } else if (!strcmp(cmd, "SEARCH")){// SEARCH|address:port:query\r\n
        char* query = strtok_r(NULL, ":", &saveptr);
        int port = atoi(strtok_r(NULL, ":", &saveptr));
        char* ip = strtok_r(NULL, ":", &saveptr);
        handle_search(ip, port, query);
    }
    // free dynamic memory
    Free(request);
    return 0;
}
// join an existing chord ring
void join_chord_ring(char* ip, int port){

    int joinfd = open_clientfd(ip, port);// open connection to ring
    // send bytes request to join
    char request[64];
    // give the ring our ip address, port, and hash
    size_t n = sprintf(request, "JOIN|%s:%d|%u\r\n", me.address, me.port, me.hash);
    rio_writep(joinfd, request, n);
    close(joinfd);
}

void* keepalive () {
    pthread_mutex_lock(&mutex);
    updated_pointers = 0;
    pthread_mutex_unlock(&mutex);
    char request[64] = "";
    printf("Keep-Alive reinitialized\n");
    size_t n = sprintf(request, "PING|%s:%d|%u\r\n", me.address, me.port, me.hash);
    while (1) {
        sleep(5);
        pthread_mutex_lock(&mutex);
            if (updated_pointers) {
                pthread_mutex_unlock(&mutex);
                break;
            }
            if (!pending) {// if not waiting on existing ping, go ahead and issue another
                pending = 1;
                int nextfd = open_clientfd(next.address, next.port);
                gettimeofday(&t1, NULL); // start timer
                rio_writep(nextfd, request, n);
                close(nextfd);
            } else {
                gettimeofday(&t2, NULL); // stop timer
                // compute and print the elapsed time in millisec
                elapsedTime = (t2.tv_sec - t1.tv_sec);
                if (elapsedTime > TIMEOUT) {
                    printf("Timeout detected on our next\n");
                    // reset
                    // initiate splicing of nodes
                    handle_timeout();
                    pending = 0; // we have finished processing ping-pong
                    elapsedTime = 0;
                    pthread_mutex_unlock(&mutex);
                    break;
                }

            }
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// continuously spawn threads to handle keepalives
void* keepalive_manager(){
    while(1){
        pthread_t tid;
        Pthread_create(&tid, NULL, keepalive, NULL);
        Pthread_join(tid, NULL);
        puts("Keep-Alive thread joined");
    }
    puts("Keep-Alive Manager done\n");
}
void repl () {
    char command[64] = "";// should be 'quit' or search term
    while(1) {
        printf("chord-shell > ");
        if(fgets(command, 64, stdin) == NULL || !strcmp(command, "\n")) {
            if (feof(stdin)) { /* End of file (ctrl-d) */
                fflush(stdout);
                printf("\n");
                exit(EXIT_SUCCESS);
            }
            continue; /* NOOP; user entered return or spaces with return */
        }
        int n = strcmp(command, "quit\n");
        if (!n){
            handle_leave();
            exit(0);
        }
        int len = strlen(command);
        if( command[len-1] == '\n' ) {
            command[len-1] = 0;
        }
        handle_search(me.address, me.port, command);
    }
}


void* connections(void* listen_port) {
    int listenfd = Open_listenfd(*(int*)listen_port);
    int optval = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    // this should be done in threads
    int connfd;
    struct sockaddr_in clientaddr;
    int clientlen = sizeof(clientaddr);

    while(1) {// listen for chord messages
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd <= 2) {
            continue;
        }
        handle_connection(connfd);
    }

}
int main(int argc, char *argv[]) {
    // parse command line options
    if (argc != 3 && argc != 5) {
        printf("Usage: %s listen_address listen_port [join_ip] [join_port]\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutex, NULL);

    char* listen_address = argv[1];
    int listen_port = atoi(argv[2]); // listen for chord connections on this port
    // generate our hash
    char ip_and_port[21];
    size_t n = sprintf(ip_and_port, "%s%d", listen_address, listen_port);
    unsigned char myhash[SHA_DIGEST_LENGTH];
    SHA1(ip_and_port, n, myhash);
    unsigned int newhash = hash_to_int(&myhash);

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
    } else{
        log_pointers();
    }

    pthread_t tid;
    Pthread_create(&tid, NULL, connections, (void*) &listen_port);
    Pthread_detach(tid);

    pthread_t tid2;
    /* spawn a thread to process the new connection */
    Pthread_create(&tid2, NULL, keepalive_manager, NULL);
    Pthread_detach(tid2);

    repl();

}


