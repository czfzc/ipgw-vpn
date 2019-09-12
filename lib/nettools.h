#ifndef _HNETTOOLS_H_
#define _HNETTOOLS_H_
#define MAX_DATA_SIZE 1536

#include<stdio.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<net/if.h>
#include<sys/ioctl.h>
#include<linux/if_ether.h>
#include<linux/if_packet.h>
#include<sys/socket.h>
#include<string.h>
#include<cerrno>
#include<netdb.h>
#include"checksum.h"
#include"md5.c"
#include"ipgw.h"

/*************************************
 * 
 * 接收udp并且解包成ip包
 * 
 *************************************/

int recv_udp_unpack_to_ip(int sock_dgram_fd,u_char *ip_tcp_data,u_int32_t *data_len,sockaddr_in * from_client){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    socklen_t server_len=sizeof(struct sockaddr_in);
    int len = recvfrom(sock_dgram_fd,ip_tcp_data,MAX_DATA_SIZE,0,(struct sockaddr*)from_client,&server_len);
    if(len<0){
        printf("recvfrom error\n");
        ret = -1;
        return ret;
    }
    printf("len %d\n",len);
    *data_len = len;
    return ret;
}

/*************************************
 * 
 * 根据网卡设备名获取网卡序列号
 * 
 *************************************/

int get_nic_index(int fd,const char* nic_name)
{
    struct ifreq ifr;
    if (nic_name== NULL)
        return -1;
    memset(&ifr, 0,sizeof(ifr));
    strncpy(ifr.ifr_name, nic_name, IFNAMSIZ);
    if (ioctl(fd,SIOCGIFINDEX,&ifr) == -1){
        printf("SIOCGIFINDEX ioctl error");
        return -1;
    }
    return ifr.ifr_ifindex;
}

/*************************************
 * 
 * 将IP报文包裹以太网帧头部并发送链路层数据报(链路层raw socket)
 * 
 *************************************/

int send_ip_ll(int sock_raw_fd,u_char *ip_tcp_data,int data_len,sockaddr_ll addr,
                        u_char *src_mac,u_char *dest_mac){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
	    printf("null data\n");
        return ret;
    }
    socklen_t socklen=sizeof(sockaddr_ll);
    u_char buf[MAX_DATA_SIZE]={0};
    memcpy(buf,dest_mac,6);
    memcpy(buf+6,src_mac,6);
    u_int16_t *data16 = (u_int16_t*)buf;
    data16[6]=htons(ETH_P_IP);
    memcpy(buf+14,ip_tcp_data,data_len);
    printf("====\n");
  //  print_data(buf,data_len+14);
    printf("\n");
    int len=sendto(sock_raw_fd,buf,(size_t)(data_len+14),0,(sockaddr*)&addr,socklen);
    if(len<0){
	printf("error sendto,errno:%d\n",errno);
        ret = -1;
        return ret;
    }
    return ret;
}

/*************************************
 * 
 * 获取默认发送sockaddr_ll
 * 
 *************************************/

void get_default_sockaddr_ll_send(int fd,sockaddr_ll *addr_ll,char *nic_name)
{
    memset(addr_ll,0,sizeof(addr_ll));
    addr_ll->sll_ifindex=get_nic_index(fd,nic_name);
    addr_ll->sll_family=PF_PACKET;
    addr_ll->sll_halen=ETH_ALEN;
}

/*************************************
 * 
 * 打印数据报
 * 
 *************************************/

void print_data(const u_char *data,const int data_len){
    printf("\n");
    for(int i=0;i<data_len;i++){
        printf("%02x ",data[i]);
        if(i!=0&&(i+1)%16==0)
            printf("\n");
    }
    printf("\n");
}

/*根据域名获取ip */
int socket_resolver(const char *domain, u_int32_t *ipaddr)
{
    if (!domain || !ipaddr) return -1;

    struct hostent* host=gethostbyname(domain);
    if (!host)
    {
        return -1;
    }

    if(host->h_addrtype == AF_INET){
        memcpy(ipaddr,host->h_addr,4);
    }else return -1;

    return 0;
}

/***********************
 * 
 * 计算16b md5
 * 
 ***********************/
int md5_16(u_char* result,const u_char* content,int content_len){
    if(result==NULL||content==NULL){
        printf("error to printf md5!\n");
        return -1;
    }
    md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)content, content_len);
	md5_finish(&state, digest);
    memcpy(result,&digest,16);
}

int get_local_ip_using_create_socket(u_int32_t *subnet_ip) 
{
    int status = -1;
    int af = AF_INET;
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in remote_addr;
    struct sockaddr_in local_addr;
    char *local_ip = NULL;
    socklen_t len = 0;

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(53);
    remote_addr.sin_addr.s_addr = inet_addr(DEFAULT_DNS_SERVER);

    len =  sizeof(struct sockaddr_in);
    status = connect(sock_fd, (struct sockaddr*)&remote_addr, len);
    if(status != 0 ){
        printf("connect err \n");
    }

    len =  sizeof(struct sockaddr_in);
    getsockname(sock_fd, (struct sockaddr*)&local_addr, &len);

    if(local_addr.sin_addr.s_addr!=0)
    {
        *subnet_ip = local_addr.sin_addr.s_addr;
        status = 0;
    }
    return status;
}

#endif