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

#define IPGW_IP_ADDR "202.118.1.87"
#define SERVER_A_IP_ADDR "192.168.1.102"
#define DEFAULT_UDP_PORT 1026
#define DEFAULT_DEVICE_NAME "wlp1s0"

u_char SERVER_A_MAC[6]={0xa0,0xc5,0x89,0x00,0x00,0x01};
u_char SERVER_B_MAC[6]={0xa0,0xc5,0x89,0x00,0x00,0x02};

/*************************************
 * 
 * 接收基于udp设计的udpop协议并且解包成ip-tcp包
 * 
 *************************************/

int recv_udpop_from_client_and_unpack_to_tcp(int sock_dgram_fd,u_char *ip_tcp_data,int *data_len){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    struct sockaddr_in client;
    socklen_t server_len=sizeof(struct sockaddr_in);
    int len = recvfrom(sock_dgram_fd,ip_tcp_data,1024,0,(struct sockaddr*)&client,&server_len);
    if(len<0){
        printf("recvfrom error\n");
        ret = -1;
        return ret;
    }
    *data_len = len;
    return ret;
}

/*************************************
 * 
 * 发送ip-tcp数据报
 * 
 *************************************/

int send_tcp_dgram(int sock_raw_fd,u_char *ip_tcp_data,int data_len,sockaddr_ll addr){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    socklen_t socklen=sizeof(sockaddr_ll);
    u_char buf[1024]={0};
    memcpy(buf,SERVER_B_MAC,6);
    memcpy(buf+6,SERVER_A_MAC,6);
    u_int16_t *data16 = (u_int16_t*)buf;
    data16[6]=ETH_P_IP;
    memcpy(buf+14,ip_tcp_data,data_len);
    int len=sendto(sock_raw_fd,buf,(size_t)(data_len+14),0,(sockaddr*)&addr,socklen);
    if(len<0){
        ret = -1;
        return ret;
    }
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
 * 接收发送主线程
 * 
 *************************************/

void* main_thread(void*){

    /*初始化接收udp数据报 */
    int sock_dgram_fd=socket(AF_INET,SOCK_DGRAM,0);
    if(sock_dgram_fd < 0){
        perror("dgram socket open error!");
    }
    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(DEFAULT_UDP_PORT);
    local.sin_addr.s_addr = inet_addr(SERVER_A_IP_ADDR);
    if(bind(sock_dgram_fd, (struct sockaddr*)&local,sizeof(sockaddr)) < 0){
        perror("bind udp error");
    }

    /*初始化发送raw数据 */
    int sock_raw_fd=socket(AF_INET,SOCK_RAW,htons(ETH_P_IP));
    if(sock_raw_fd < 0){
        perror("raw socket open error!");
    }
    struct sockaddr_ll local_raw;
    get_default_sockaddr_ll_send(sock_raw_fd,&local_raw,DEFAULT_DEVICE_NAME);

    u_int8_t buf[1024]={0};

    while(1){
        
        int ip_tcp_len = -1;
        int recv = recv_udpop_from_client_and_unpack_to_tcp(sock_dgram_fd,buf,&ip_tcp_len);
        if(recv < 0){
            perror("recv sock dgram error");
            continue;
        }
        int send = send_tcp_dgram(sock_raw_fd,buf,ip_tcp_len,local_raw);
        if(send < 0)
            perror("send sock raw error");
        
    }

    close(sock_dgram_fd);
    close(sock_raw_fd);

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