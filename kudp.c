#include "kudp.h"

void receive_loop(Kudp *kudp){
    struct sockaddr_in address;
    struct sockaddr_in client_address;
    socklen_t client_length;
    address.sin_port = htons((uint16_t) kudp->listen_port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    int socket_fd;
    int option;
    if((socket_fd = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP)) < 0){
        return;
    }
    option = 1;
    if(setsockopt(socket_fd , SOL_SOCKET , SO_REUSEADDR , (const char*)&option, sizeof(option)) < 0){
        return;
    }
    option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, (const char*)&option, sizeof(option)) < 0){
        return;
    }
    if(bind(socket_fd , (const struct sockaddr *) &address, sizeof(address)) < 0){
        return;
    }
    char buffer[BUFFER_SIZE];
    int read;
    while (1) {
        bzero(&client_address , sizeof(struct sockaddr_in));
        bzero(&client_length , sizeof(socklen_t));
        bzero(&buffer , BUFFER_SIZE);
        if((read = (int) recvfrom(socket_fd , buffer , 1, 0 , (struct sockaddr *) &client_address, &client_length)) < 0){
            break;
        }
        char *host = inet_ntoa(client_address.sin_addr);
        int port = ntohs(client_address.sin_port);
        KudpPeer peer = {socket_fd , host , port};
        KudpData data = {buffer , read};
        kudp->onData(&peer , &data);
    }
}

Kudp *kudp_new(){
    Kudp *kudp = malloc(sizeof(Kudp));
    return kudp;
}

Kudp *kudp_bind(
        Kudp *kudp,
        int pool_size,
        int listen_port,
        void (*onData)(KudpPeer *peer , KudpData *data)
){
    kudp->listen_port = listen_port;
    kudp->onData = onData;
    kudp->processor = kprocessor_new(KPROCESSOR_POOL_TYPE_PROCESS,pool_size);
    kprocessor_start(kudp->processor);
    int socket_fd;
    int option;
    if((socket_fd = socket(AF_INET , SOCK_DGRAM , IPPROTO_UDP)) < 0){
        return NULL;
    }
    option = 1;
    if(setsockopt(socket_fd , SOL_SOCKET , SO_REUSEADDR , (const char*)&option, sizeof(option)) < 0){
        return NULL;
    }
    option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, (const char*)&option, sizeof(option)) < 0){
        return NULL;
    }
    kudp->socket_fd = socket_fd;
    kprocessor_post(kudp->processor , (void (*)(void *)) receive_loop, kudp);
    return kudp;
}

void kudp_sendto(KudpPeer *peer , KudpData *data){
    struct sockaddr_in address;
    address.sin_port = htons((uint16_t) peer->peer_port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(peer->peer_host);
    sendto(peer->socket_fd , data->data , (size_t) data->data_size, 0 , (const struct sockaddr *) &address, sizeof(address));
}

KudpPeer *kudp_peer(Kudp *kudp , char *host , int port){
    KudpPeer *peer = malloc(sizeof(KudpPeer));
    peer->peer_host = host;
    peer->peer_port = port;
    peer->socket_fd = kudp->socket_fd;
    return peer;
}

Kudp *kudp_close(Kudp *kudp){
    kprocessor_stop(kudp->processor);
    return kudp;
}

void kudp_free(Kudp *kudp){
    if (kudp != NULL) {
        if(kudp->processor != NULL){
            kprocessor_free(kudp->processor);
        }
        free(kudp);
    }
}