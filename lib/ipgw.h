#ifndef _HIPGW_H_
#define _HIPGW_H_

#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>
#include<sys/socket.h>
#include"nettools.h"    

#define IPGW_IP_ADDR "202.118.1.87"
#define IPGW_DEFAULT_PORT 80
#define SERVER_A_SUBNET_IP_ADDR "172.17.0.3"
#define SERVER_B_SUBNET_IP_ADDR "172.17.0.2"
#define SERVER_A_IP_ADDR "58.154.192.58"
#define CLIENT_NAT_IP_ADDR "58.154.192.75"
#define CLIENT_SUBNET_IP_ADDR "192.168.1.104"
#define DEFAULT_UDP_PORT 1026
#define DEFAULT_SERVER_B_UDP_PORT 1026
#define DEFAULT_SERVER_PORT 1030
#define DEFAULT_DEVICE_NAME "docker0"
#define DEFAULT_DEVICE_NAME_MAIN "enp5s0"
#define MAX_BUFFER_QUEUE_SIZE 5000
#define SERVER_DOMAIN "s.ipgw.top"
#define DEFAULT_DNS_SERVER "202.118.1.29"


typedef struct{
    u_int32_t data_len;
    u_char *data;
    sockaddr target;
    u_char session_key[16];
}packet;
/*现在通过ip层发送 系统通过arp自动获取网卡地址
static u_char SERVER_A_MAC_SUBNET[6]={0x02,0x42,0x51,0xda,0x83,0x3b};
static u_char SERVER_B_MAC_SUBNET[6]={0x02,0x42,0xac,0x11,0x00,0x02};
static u_char SERVER_A_MAC_OUT[6]={0x00,0xf1,0xf3,0x17,0xac,0xc5};
static u_char GATEWAY_MAC[6]={0x38,0x97,0xd6,0x51,0xa0,0xa2};
*/
#endif