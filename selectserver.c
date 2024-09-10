#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include<unistd.h>
#include <arpa/inet.h>

#define PORT "9034"

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void hints_init(struct addrinfo *hints) {
    memset(hints,0,sizeof(struct addrinfo));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags =AI_PASSIVE;
}

int getservinfo(const char *ip,struct addrinfo **servinf) {
    struct addrinfo hints;
    hints_init(&hints);
    int rv;
    if((rv=getaddrinfo(ip,PORT,&hints,servinf))!=0) {
        fprintf(stderr,"getaddrinfo: %s\n",gai_strerror(rv));
        return 1;
    }
    return 0;
}


int bind_socket(struct addrinfo *servinf,int *sorkfd) {
    int yes=1;
    struct addrinfo *p;
    for(p=servinf; p!=NULL; p=p->ai_next) {
        *sorkfd = socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if((*sorkfd)<0) {
            continue;
        }
        setsockopt(*sorkfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));
        if(bind(*sorkfd, p->ai_addr,p->ai_addrlen)<0) {
            close(*sorkfd);
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }
    return 0;
}

void do_recv_data(const int i,const int fdmax,fd_set *master,const int sockfd) {
    char buf[256]; // 储存 client 数据的缓冲区
    int nbytes;
    //接收数据
    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
        if (nbytes == 0) {
            // 关闭连接
            printf("selectserver: socket %d hung up\n", i);
        } else {
            perror("recv");
        }
        close(i);
        FD_CLR(i, master); // 从 master set 中移除
    } else {
        for(int j = 0; j <= fdmax; j++) {
            // 送给大家！
            if (FD_ISSET(j, master)) {
                // 不用送给 listener 跟我们自己
                if (j != sockfd && j != i) {
                    if (send(j, buf, nbytes, 0) == -1) {
                        perror("send");
                    }
                }
            }
        }
    }
}

void handle_new_connect(const int i,const int sockfd,fd_set *master,int *fdmax) {
        int newfd; // 新接受的 accept() socket descriptor
        struct sockaddr_storage remoteaddr; // client address
        socklen_t addrlen;
        char remoteIP[INET6_ADDRSTRLEN];
        addrlen = sizeof(remoteaddr);
        newfd = accept(sockfd, (struct sockaddr *)&remoteaddr,&addrlen);
        if(newfd == -1) {
            perror("accept");
        } else {
            FD_SET(newfd, master);
            if(newfd>*fdmax) {
                *fdmax = newfd;
            }
            printf("selectserver:new connection from %s on socket %d\n",inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, INET6_ADDRSTRLEN),newfd);
        }
    }

int selectserver(int sockfd) {
    fd_set master;
    fd_set read_fds;
    int fdmax;
    FD_ZERO(&master); // 清除 master 与 temp sets
    FD_ZERO(&read_fds);// 将监听 socket 加入 master set
    FD_SET(sockfd,&master);// 记录最大的文件描述符
    fdmax = sockfd;
    while(1) {
        read_fds = master;
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }
        for(int i=0; i<= fdmax; i++) {
            if(FD_ISSET(i,&read_fds)) {//i在read_fds中
              if(i==sockfd){//如果
                handle_new_connect(i,sockfd,&master,&fdmax);// 处理新的连接
              }else{
                do_recv_data(i, fdmax,&master,sockfd);// 处理client数据
              }
            }
        }
    }
}
int main(void) {

    struct addrinfo *servinf;
    int sockfd;
    int rv;


    // 获取地址信息
    if((rv=getservinfo(NULL,&servinf)!=0)) {
        exit(1);
    }
    // 绑定端口
    if((rv=bind_socket(servinf,&sockfd)!=0)) {
        exit(1);
    }
    // 释放地址信息
    freeaddrinfo(servinf);
    //设置等待连接，最大连接数为10
    if(listen(sockfd, 10)==-1) {
        perror("listen");
        exit(3);
    }
    // 开始监听
    selectserver(sockfd);
    return 0;
}
