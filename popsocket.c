#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#define PORT "3490"

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family==AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}

int get_listen_socket(void) {
    int sockfd;
    int yes=1;
    int rv;
    struct addrinfo *servinfo;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype =SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((rv=getaddrinfo(NULL,PORT,&hints,&servinfo))!=0) {
        fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv) );
        exit(1);
    }

    struct addrinfo *p;
    for(p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd=(socket(p->ai_family,p->ai_socktype, p->ai_protocol)))==-1) {
            perror("socket:error");
            continue;
        }
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1) {
            perror("setsockopt");
            exit(1);
        }
        if(bind(sockfd,p->ai_addr,p->ai_addrlen)==-1) {
            close(sockfd);
            perror("server:bind");
            continue;
        }
        break;
    }

    if(p==NULL) {
        fprintf(stderr,"server:failed to bind\n");
        return -1;
    }


    freeaddrinfo(servinfo);

    if(listen(sockfd,10)==-1) {
        perror("listen");
        return -1;
    }
    return sockfd;
}


void add_to_pfds(struct pollfd *pfds[],int newfd,int *fd_count,int *fd_size) {
    if(*fd_count==*fd_size) {
        *fd_size = 2*(*fd_size);
        *pfds = realloc(*pfds, sizeof((**pfds)) * (*fd_size));
    }
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN;
    (*fd_count)++;
}

void del_from_pfds(struct pollfd *pfds,int i,int *fd_count) {
// Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

int main() {
    int listener;
    int newfd;

    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;
    char remoteIP[INET6_ADDRSTRLEN];
    char buffer[256];
    int fd_count =0 ;
    int fd_size = 5;
    struct pollfd *pfds = malloc(sizeof(*pfds) * fd_size);
    listener = get_listen_socket();
    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
    pfds[0].fd = listener;
    pfds[0].events = POLLIN;
    fd_count++;

    while(1) {
        int poll_count = poll(pfds,fd_count,-1);
        if(poll_count == -1) {
            perror("poll");
            exit(1);
        }
        for(int i=0; i<fd_count; i++) {
            if(pfds[i].revents&POLLIN) {
                if(pfds[i].fd == listener) {
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,(struct sockaddr *)&remoteaddr,&addrlen);
                    if(newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&pfds,newfd,&fd_count,&fd_size);
                        printf("pollserver: new connection from %s on "
                               "socket %d\n",
                               inet_ntop(remoteaddr.ss_family,
                                         get_in_addr((struct sockaddr*)&remoteaddr),
                                         remoteIP, INET6_ADDRSTRLEN),
                               newfd);
                    }
                } else {
                    int nbt = recv(pfds[i].fd,buffer,sizeof(buffer),0);
                    int sender_fd = pfds[i].fd;
                    if (nbt <=0) {
                        if(nbt==0) {
                            printf("pollserver: socket %d hung up\n",sender_fd);
                        } else {
                            perror("recv");
                        }
                        close(pfds[i].fd);
                        del_from_pfds(pfds, i, &fd_count);
                    } else {
                        for(int j=0; j<fd_count; j++) {
                            int dest_fd = pfds[j].fd;
                            if(dest_fd != listener && dest_fd != sender_fd) {
                                if(send(dest_fd,buffer,nbt,0)==-1) {
                                    perror("send");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
