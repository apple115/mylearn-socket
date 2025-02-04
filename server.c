#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include<sys/socket.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#define PORT "3490"
#define BACKLOG 10

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa) {
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);

}

void init_hints(struct addrinfo *hints) {
    memset(hints, 0, sizeof(*hints));
    hints->ai_family =AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_PASSIVE;
}

int getservinfo(struct addrinfo **servinfo) {
    struct addrinfo hints;
    init_hints(&hints);
    int rv;
    if((rv = getaddrinfo(NULL, PORT,&hints, servinfo))!=0) {
        fprintf(stderr, "getaddrinfo:%s\n",gai_strerror(rv));
        return -1;
    }
    return 0;
}

int main() {
    struct addrinfo *servinfo, *p;
    struct sigaction sa;
    struct sockaddr_storage their_addr; // 连接者的地址资料
    socklen_t sin_size;

    int rc;
    if((rc = getservinfo(&servinfo)) == -1) {
        return rc;
    }

    int sockfd,new_fd;
    int yes=1;
    char s[INET6_ADDRSTRLEN] ;


    for (p=servinfo; p!=NULL; p=p->ai_next) {
        if((sockfd=socket(p->ai_family,p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server:socket");
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
        return 2;
    }
    freeaddrinfo(servinfo);
    if(listen(sockfd, BACKLOG)==-1) {
        perror("listen");
        exit(1);
    }
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGCHLD,&sa,NULL)==-1) {
        perror("sigaction");
        exit(1);
    }

    printf("server:wanting for connections...\n");
    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);
        if(new_fd == -1) {
            perror("accept");
            continue;
        }
        inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s,sizeof(s));
        printf("server:got connection from %s\n",s);
        if(!fork()) {
            close(sockfd);
            if(send(new_fd,"Hello world!",13,0)==-1)
                perror("send");

            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}

