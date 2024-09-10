#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT "3490"
#define MAXDATASIZE 100

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void init_hints(struct addrinfo *hints) {
    memset(hints,0,sizeof(struct addrinfo));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
}

int getservinfo(const char *ip,struct addrinfo **servinf) {
    struct addrinfo hints;
    init_hints(&hints);
    int rv;
    if((rv=getaddrinfo(ip,PORT,&hints,servinf))!=0) {
        fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(rv));
        return 1;
    }
    return 0;
}

int connect_socket(int *sockfd,struct addrinfo *servinfo,struct addrinfo **p) {
    for (*p=servinfo; (*p)!=NULL; (*p)=(*p)->ai_next) {
        if((*sockfd = socket((*p)->ai_family,(*p)->ai_socktype,(*p)->ai_protocol))==-1) {
            perror("client: socket");
            continue;
        }
        if(connect(*sockfd, (*p)->ai_addr, (*p)->ai_addrlen)==-1) {
            close(*sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    return 0;
}

void print_ip(const struct addrinfo *servinfo,const struct addrinfo *p) {
    char s[INET6_ADDRSTRLEN];
    inet_ntop(p->ai_family,get_in_addr((struct sockaddr*)p->ai_addr),s, sizeof(s));
    printf("client: connecting to %s\n", s);
}

void recv_data(int sockfd) {
    char buf[MAXDATASIZE];
    int numbytes;
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    buf[numbytes] = '\0';
    printf("client: received '%s'\n",buf);
}


int main(int argc,char *argv[]) {
    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
    struct addrinfo *servinfo,*p;
    getservinfo(argv[1], &servinfo);
    int sockfd;
    int rc;
    if((rc=connect_socket(&sockfd,servinfo,&p))!=0) {
        return rc;
    }
    print_ip(servinfo, p);
    freeaddrinfo(servinfo);
    recv_data(sockfd);
    close(sockfd);
    return 0;
}
