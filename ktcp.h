#ifndef KSTD_KTCP_H
#define KSTD_KTCP_H

#undef __FD_SETSIZE
#define __FD_SETSIZE 1000000

#include "arpa/inet.h"
#include "signal.h"
#include "sys/select.h"
#include "sys/socket.h"
#include "netdb.h"
#include "netinet/in.h"
#include "stdlib.h"
#include "kprocessor.h"
#include "aio.h"

#define BUFFER_SIZE 4096
#define PORT_RANDOM 0

typedef enum KtcpPoolType{
    KTCP_POOL_TYPE_ASYNC_NONBLOCK,  // aio
    KTCP_POOL_TYPE_ASYNC_BLOCK,     // select
    KTCP_POOL_TYPE_SYNC_BLOCK,      // read , write , fork
} KtcpPoolType;

typedef struct KtcpData{
    void *data;
    int data_size;
} KtcpData;

typedef struct KtcpPeer{
    int socket_fd;
} KtcpPeer;

typedef struct Ktcp{
    char *target_host;
    int target_port;
    fd_set *master_read_set;
    struct aiocb *aio_cb;
    KtcpPoolType pool_type;
    Kprocessor *processor;
    int max_connections;
    int listen_port;
    void (*onOpen)(KtcpPeer *peer);
    void (*onData)(KtcpPeer *peer , KtcpData *data);
    void (*onClose)(KtcpPeer *peer);
} Ktcp;

Ktcp *ktcp_new();

Ktcp *ktcp_listen(
        Ktcp *ktcp,
        KtcpPoolType pool_type,
        int pool_size,
        int max_connections,
        int listen_port,
        void (*onOpen)(KtcpPeer *peer),
        void (*onData)(KtcpPeer *peer , KtcpData *data),
        void (*onClose)(KtcpPeer *peer)
);

Ktcp *ktcp_connect(
        Ktcp *ktcp,
        char *target_host ,
        int target_port ,
        void (*onOpen)(KtcpPeer *peer),
        void (*onData)(KtcpPeer *peer , KtcpData *data) ,
        void (*onClose)(KtcpPeer *peer)
);

void ktcp_send(KtcpPeer *peer , KtcpData *data);

void ktcp_shutdown(KtcpPeer *peer);

Ktcp *ktcp_close(Ktcp *ktcp);

void ktcp_free(Ktcp *ktcp);

#endif //KSTD_KTCP_H
