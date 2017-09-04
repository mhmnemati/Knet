#include <stdio.h>
#include "kpipe.h"
#include "ktcp.h"
#include "kudp.h"

void onUdpData(KudpPeer *peer , KudpData *data){
    printf("UDP DATA\n");fflush(stdout);
}

void onOpen(KtcpPeer *peer){
    printf("ON OPEN %d\n",peer->socket_fd);fflush(stdout);
}

void onData(KtcpPeer *peer,KtcpData *data){
    printf("ON DATA %d = %s\n", peer->socket_fd, (char *) data->data);fflush(stdout);
    ktcp_shutdown(peer);
}

void onClose(KtcpPeer *peer){
    printf("ON CLOSE %d\n",peer->socket_fd);fflush(stdout);
}

int main() {

    Ktcp *ktcp = ktcp_new();
    ktcp_listen(
            ktcp,
            KTCP_POOL_TYPE_ASYNC_NONBLOCK,
            1,
            1000,
            1234,
            onOpen,
            onData,
            onClose
    );

    sleep(4);

    Ktcp *kt = ktcp_new();
    ktcp_connect(
            kt,
            "127.0.0.1",
            1234,
            onOpen,
            onData,
            onClose
    );



    Kudp *udp = kudp_new();
    kudp_bind(udp,1, 1235 , onUdpData);

    Kudp *u1 = kudp_new();
    u1 = kudp_bind(u1,1,1236,onUdpData);

    sleep(1);

    KudpPeer *peer = kudp_peer(u1, "127.0.0.1",1235);
    char *dat = malloc(BUFFER_SIZE+1);
    strcpy(dat,"ASASAASAS");
    KudpData data = {dat,BUFFER_SIZE+1};
    kudp_sendto(peer,&data);

    sleep(100);

    return 0;
}