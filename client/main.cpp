#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>
#include<pthread.h>

#define DEFAULT_UDP_PORT 1026
#define IPGW "202.118.1.87"
#define IPGW_DEFAULT_PORT 80
#define SERVER_A_IP_ADDR "192.168.1.102"

/*************************************
 * 
 * 接收来自IPGW的所有tcp数据报
 * 
 *************************************/

int recv_tcp_dgram(int sock_ip_fd,u_char *ip_tcp_data,int *data_len){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    sockaddr_in ipgw;
    socklen_t socklen = sizeof(sockaddr_in);
    int len=recvfrom(sock_ip_fd,ip_tcp_data,1024,0,(sockaddr*)&ipgw,&socklen);
    if(len<0){
        printf("tcp dgram recvfrom error");
        ret = -1;
        return ret;
    }
    *data_len = len;
    return ret;
}

/*************************************
 * 
 * 将tcp数据报封装到udp并发送之
 * 
 *************************************/

int send_udp_dgram_from_tcp_data(int sock_dgram_fd,u_char *ip_tcp_data,int data_len,sockaddr_in *addr_in){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    socklen_t socklen = sizeof(sockaddr_in);
    int len = sendto(sock_dgram_fd,ip_tcp_data,data_len,0,(sockaddr*)addr_in,socklen);
    if(len < 0){
        printf("udp dgram sendto error");
        ret = -1;
        return ret;
    }
    return ret;
}

void *main_thread(void*){
    /*初始化ip层raw socket */
    int sock_ip_tcp_fd=socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
    if(sock_ip_tcp_fd < 0){
        perror("ip raw socket open error");
    }
    sockaddr_in ipgw;
    ipgw.sin_family = AF_INET;
    ipgw.sin_addr.s_addr = inet_addr(IPGW);
    ipgw.sin_port = htons(IPGW_DEFAULT_PORT);
    if(connect(sock_ip_tcp_fd,(sockaddr*)&ipgw,sizeof(ipgw))<0){
        perror("fail to connect ipgw");
    }

    /*初始化发送udp数据报 */
    int sock_dgram_fd=socket(AF_INET,SOCK_DGRAM,0);
    if(sock_dgram_fd < 0){
        perror("dgram socket open error");
    }
    sockaddr_in serverA_addr_in;
    serverA_addr_in.sin_family = AF_INET;
    serverA_addr_in.sin_addr.s_addr = inet_addr(SERVER_A_IP_ADDR);
    serverA_addr_in.sin_port = htons(DEFAULT_UDP_PORT);
    
    u_char buf[1024]={0};

    while(1){
       int ip_tcp_len = -1;
       int recv = recv_tcp_dgram(sock_ip_tcp_fd,buf,&ip_tcp_len);
       if(recv < 0){
           perror("recv sock ip tcp error");
           continue;
       } 
       int send = send_udp_dgram_from_tcp_data(sock_dgram_fd,buf,ip_tcp_len,&serverA_addr_in);
       if(recv < 0)
           perror("send sock udp error");
    }
}

int main(){
    pthread_t tid;
    int err = pthread_create(&tid, NULL, main_thread, NULL);
    if(err != 0){
        perror("fail to create thread");
        return -1;
    }
    pthread_join(tid,NULL);
    return 0;
}