#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>
#include<sys/socket.h>
#include<unistd.h>
#include<pthread.h>
#include"../lib/connector/request_ipgw.h"

#define DEFAULT_UDP_PORT 1026
#define MAX_DATA_SIZE 1536

u_char start_signal[] = {0x0d,0x00,0x05,0x01,0x02,0x03,0x04,0x05};
int sock_udp_fd;

int init(){
    if((sock_udp_fd = socket(AF_INET,SOCK_DGRAM,0))<0){
        printf("open udp error\n");
        return -1;
    }
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DEFAULT_UDP_PORT);
    bzero(&addr.sin_zero,8);
    if(bind(sock_udp_fd,(sockaddr*)&addr,sizeof(sockaddr))<0){
        printf("bind udp port error!\n");
    }
}

void* main_thread(void*){
    init();
    while(1){
        u_char buf[MAX_DATA_SIZE];
        sockaddr_in from;
        socklen_t socklen = sizeof(sockaddr_in);
        recvfrom(sock_udp_fd,buf,MAX_DATA_SIZE,0,(sockaddr*)&from,&socklen);
        printf("recv udp dgram from %s\n",inet_ntoa(from.sin_addr));
        if(memcmp(buf,start_signal,8)==0){    /*核验通过 可以发包 */
            int result = flood_request();
            if(result == 0){        /*发送数据包成功 */
                printf("connect ipgw success!\n");
                char mes[] = "connect ipgw success!";
                u_char data[strlen(mes)+3];
                data[0] = 14;
                u_int16_t datalen = htons(strlen(mes));
                memcpy(data+1,&datalen,2);
                memcpy(data+3,mes,strlen(mes));
                int n = sendto(sock_udp_fd,data,strlen(mes)+3,0,(sockaddr*)&from,sizeof(sockaddr_in));
                if(n<0)
                    printf("sendto error!\n");
            }else{         /*发送数据包失败 */
                char mes[] = "connect ipgw fail!";
                u_char data[strlen(mes)+3];
                data[0] = 14;
                u_int16_t datalen = htons(strlen(mes));
                memcpy(data+1,&datalen,2);
                memcpy(data+3,mes,strlen(mes));
                int n = sendto(sock_udp_fd,data,strlen(mes)+3,0,(sockaddr*)&from,sizeof(sockaddr_in));
            }
        }
    }
}

int main(){
    pthread_t thread_main;
    pthread_create(&thread_main,NULL,main_thread,NULL);
    pthread_join(thread_main,NULL);
}