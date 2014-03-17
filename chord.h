#include <openssl/sha.h>
typedef struct {
    char address[16];
    int port;
    unsigned int hash;
    //unsigned char hash[SHA_DIGEST_LENGTH];
} chord_node;
#define TIMEOUT 5
