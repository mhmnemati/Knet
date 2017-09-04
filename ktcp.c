#include "ktcp.h"
#include "stdio.h"

void async_nonblock_handler(int signo , siginfo_t *info , void *context){
    if(info->si_signo == SIGIO){
        Ktcp *ktcp = info->si_value.sival_ptr;
        int buffer_length = (int) aio_return(ktcp->aio_cb);
        KtcpPeer peer = {ktcp->aio_cb->aio_fildes};
        if(buffer_length > 0){
            FD_SET(ktcp->aio_cb->aio_fildes , ktcp->master_read_set);
            KtcpData data = {(void *) ktcp->aio_cb->aio_buf, buffer_length};
            ktcp->onData(&peer,&data);
        }else{
            ktcp->onClose(&peer);
        }
        if(ktcp->aio_cb->aio_buf != NULL){
            free(ktcp->aio_cb->aio_buf);
        }
    }
}

void async_nonblock_loop(int listen_fd , Ktcp *ktcp){
    printf("LISTENING\n");fflush(stdout);
    int max_fd = listen_fd;
    fd_set read_set,master_read_set;
    FD_ZERO(&master_read_set);
    FD_SET(listen_fd , &master_read_set);
    while(1){
        read_set = master_read_set;
        if(select(max_fd+1 , &read_set , NULL , NULL , NULL) < 0){
            continue;
        }
        for(int cursor = 0 ; cursor <= max_fd ; cursor++){
            if(FD_ISSET(cursor , &read_set)){
                if(cursor == listen_fd){
                    struct sockaddr_in address;
                    socklen_t address_length = sizeof(address);
                    bzero(&address, sizeof(address));
                    int accept_fd = accept(listen_fd , (struct sockaddr *) &address, &address_length);
                    if(accept_fd > max_fd){
                        max_fd = accept_fd;
                    }
                    FD_SET(accept_fd , &master_read_set);
                    KtcpPeer peer = {accept_fd};
                    ktcp->onOpen(&peer);
                }else{
                    void *buffer = malloc(BUFFER_SIZE);
                    bzero(buffer,BUFFER_SIZE);
                    struct sigaction signal_action;
                    struct aiocb aio_cb;
                    bzero(&signal_action , sizeof(struct sigaction));
                    bzero(&aio_cb , sizeof(struct aiocb));
                    sigemptyset(&signal_action.sa_mask);
                    signal_action.sa_flags = SA_SIGINFO;
                    signal_action.sa_sigaction = async_nonblock_handler;
                    //signal_action.sa_handler = async_nonblock_handler;
                    aio_cb.aio_buf = buffer;
                    aio_cb.aio_offset = 0;
                    aio_cb.aio_nbytes = BUFFER_SIZE;
                    aio_cb.aio_fildes = cursor;
                    aio_cb.aio_lio_opcode = LIO_READ;
                    aio_cb.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
                    aio_cb.aio_sigevent.sigev_signo = SIGIO;
                    ktcp->master_read_set = &master_read_set;
                    ktcp->aio_cb = &aio_cb;
                    aio_cb.aio_sigevent.sigev_value.sival_ptr = ktcp;
                    sigaction(SIGIO,&signal_action,NULL);
                    if(aio_read(&aio_cb) == 0){
                        FD_CLR(cursor , &master_read_set);
                    }
                }
            }
        }
    }
}

void async_block_loop(int listen_fd , Ktcp *ktcp){
    int max_fd = listen_fd;
    fd_set read_set,master_read_set;
    FD_ZERO(&master_read_set);
    FD_SET(listen_fd , &master_read_set);
    while(1){
        read_set = master_read_set;
        if(select(max_fd+1 , &read_set , NULL , NULL , NULL) < 0){
            continue;
        }
        for(int cursor = 0 ; cursor <= max_fd ; cursor++){
            if(FD_ISSET(cursor , &read_set)){
                if(cursor == listen_fd){
                    struct sockaddr_in address;
                    socklen_t address_length = sizeof(address);
                    bzero(&address, sizeof(address));
                    int accept_fd = accept(listen_fd , (struct sockaddr *) &address, &address_length);
                    if(accept_fd > max_fd){
                        max_fd = accept_fd;
                    }
                    FD_SET(accept_fd , &master_read_set);
                    KtcpPeer peer = {accept_fd};
                    ktcp->onOpen(&peer);
                }else{
                    char buffer[BUFFER_SIZE];
                    bzero(&buffer,BUFFER_SIZE);
                    int buffer_length = (int) read(cursor , buffer , BUFFER_SIZE);
                    KtcpPeer peer = {cursor};
                    if(buffer_length > 0){
                        KtcpData data = {buffer , buffer_length};
                        ktcp->onData(&peer,&data);
                    }else{
                        FD_CLR(cursor , &master_read_set);
                        ktcp->onClose(&peer);
                    }
                }
            }
        }
    }
}

void sync_block_loop(int listen_fd , Ktcp *ktcp){
    while(1){
        struct sockaddr_in address;
        socklen_t address_length = sizeof(address);
        bzero(&address, sizeof(address));
        int accept_fd = accept(listen_fd , (struct sockaddr *) &address, &address_length);
        int pid = fork();
        if(pid == 0){
            // add client host , port to map
            KtcpPeer peer = {accept_fd};
            ktcp->onOpen(&peer);
            void *buffer = malloc(BUFFER_SIZE);
            bzero(buffer,BUFFER_SIZE);
            int buffer_length;
            while((buffer_length = (int) read(accept_fd , buffer , BUFFER_SIZE)) > 0){
                KtcpData data = {buffer , buffer_length};
                ktcp->onData(&peer,&data);
            }
            ktcp->onClose(&peer);
            kill(getpid(),SIGKILL);
        }else if(pid < 0){
            exit(0);
        }
    }
}

void accept_loop(Ktcp *ktcp){
    int listen_fd;
    int option;
    struct sockaddr_in address;
    address.sin_port = htons((uint16_t) ktcp->listen_port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if((listen_fd = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP)) < 0){
        return;
    }
    option = 1;
    if(setsockopt(listen_fd , SOL_SOCKET , SO_REUSEADDR , (const char*)&option, sizeof(option)) < 0){
        return;
    }
    option = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, (const char*)&option, sizeof(option)) < 0){
        return;
    }
    if(bind(listen_fd , (const struct sockaddr *) &address, sizeof(address)) < 0){
        return;
    }
    if(listen(listen_fd , ktcp->max_connections) < 0){
        return;
    }
    switch (ktcp->pool_type){
        case KTCP_POOL_TYPE_ASYNC_NONBLOCK:
            async_nonblock_loop(listen_fd , ktcp);
            break;
        case KTCP_POOL_TYPE_ASYNC_BLOCK:
            async_block_loop(listen_fd , ktcp);
            break;
        case KTCP_POOL_TYPE_SYNC_BLOCK:
            sync_block_loop(listen_fd , ktcp);
            break;
    }
}

void connect_loop(Ktcp *ktcp){
    int socket_fd;
    struct sockaddr_in address;
    address.sin_port = htons((uint16_t) ktcp->target_port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ktcp->target_host);
    if((socket_fd = socket(AF_INET , SOCK_STREAM , IPPROTO_TCP)) < 0){
        return;
    }
    if(connect(socket_fd , (const struct sockaddr *) &address, sizeof(address)) < 0){
        return;
    }
    KtcpPeer peer = {socket_fd};
    ktcp->onOpen(&peer);
    void *buffer = malloc(BUFFER_SIZE);
    bzero(buffer,BUFFER_SIZE);
    int buffer_length;
    while((buffer_length = (int) read(socket_fd , buffer , BUFFER_SIZE)) > 0){
        KtcpData data = {buffer , buffer_length};
        ktcp->onData(&peer,&data);
    }
    ktcp->onClose(&peer);
    kill(getpid(),SIGKILL);
}

Ktcp *ktcp_new(){
    Ktcp *ktcp = malloc(sizeof(Ktcp));
    return ktcp;
}

Ktcp *ktcp_listen(
        Ktcp *ktcp,
        KtcpPoolType pool_type,
        int pool_size,
        int max_connections, // max connections !
        int listen_port,
        void (*onOpen)(KtcpPeer *peer),
        void (*onData)(KtcpPeer *peer , KtcpData *data),
        void (*onClose)(KtcpPeer *peer)
){
    ktcp->pool_type = pool_type;
    ktcp->max_connections = max_connections;
    ktcp->listen_port = listen_port;
    ktcp->onOpen = onOpen;
    ktcp->onData = onData;
    ktcp->onClose = onClose;
    ktcp->processor = kprocessor_new(KPROCESSOR_POOL_TYPE_PROCESS,pool_size);
    kprocessor_start(ktcp->processor);
    while(pool_size-- > 0){
        kprocessor_post(ktcp->processor , (void (*)(void *)) accept_loop, ktcp);
    }
    return ktcp;
}

Ktcp *ktcp_connect(
        Ktcp *ktcp,
        char *target_host ,
        int target_port ,
        void (*onOpen)(KtcpPeer *peer),
        void (*onData)(KtcpPeer *peer , KtcpData *data) ,
        void (*onClose)(KtcpPeer *peer)
){
    ktcp->target_host = target_host;
    ktcp->target_port = target_port;
    ktcp->onOpen = onOpen;
    ktcp->onData = onData;
    ktcp->onClose = onClose;
    ktcp->processor = kprocessor_new(KPROCESSOR_POOL_TYPE_PROCESS,1);
    kprocessor_start(ktcp->processor);
    kprocessor_post(ktcp->processor , (void (*)(void *)) connect_loop, ktcp);
    return ktcp;
}

void ktcp_send(KtcpPeer *peer , KtcpData *data){
    write(peer->socket_fd , data->data , (size_t) data->data_size);
}

void ktcp_shutdown(KtcpPeer *peer){
    close(peer->socket_fd);
}

Ktcp *ktcp_close(Ktcp *ktcp){
    kprocessor_stop(ktcp->processor);
    return ktcp;
}

void ktcp_free(Ktcp *ktcp) {
    if (ktcp != NULL) {
        if(ktcp->processor != NULL){
            kprocessor_free(ktcp->processor);
        }
        free(ktcp);
    }
}