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
#include<cerrno>

#include<queue>

#define IPGW_IP_ADDR "202.118.1.87"
#define IPGW_DEFAULT_PORT 80
#define SERVER_A_SUBNET_IP_ADDR "172.17.0.2"
#define SERVER_B_SUBNET_IP_ADDR "172.17.0.3"
#define SERVER_A_IP_ADDR "58.154.192.58"
#define CLIENT_NAT_IP_ADDR "58.154.192.75"
#define CLIENT_SUBNET_IP_ADDR "192.168.1.102"
#define DEFAULT_UDP_PORT 1026
#define DEFAULT_DEVICE_NAME "eth0"
#define MAX_BUFFER_QUEUE_SIZE 5000
#define MAX_DATA_SIZE 1536

using namespace std;

u_char SERVER_A_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x02};
u_char SERVER_B_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x03};
u_char GATEWAY_MAC[6]={0x38,0x97,0xd6,0x51,0xa0,0xa2};

typedef struct{
    uint32_t data_len;
    u_char *data;
}packet;

pthread_mutex_t pthread_mutex;
static int sock_udp_fd,sock_raw_fd;
static queue<packet*> data_queue;
static sockaddr_ll addr_ll;

/*************************************
 * 
 * 打印数据报
 * 
 *************************************/

void print_data(u_char *data,int data_len){
    printf("\n");
    for(int i=0;i<data_len;i++){
        printf("%02x ",data[i]);
        if(i!=0&&(i+1)%16==0)
            printf("\n");
    }
    printf("\n");
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
    //printf("====");
    //print_data(buf,data_len+14);
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
 * 初始化套接字和udp连接serverA
 * 
 *************************************/

int init(){
    int ret = 0;

    /*初始化互斥锁 */
    pthread_mutex_init(&pthread_mutex,NULL);
    /*初始化udp发送接收 1026-1026*/
    sock_udp_fd=socket(AF_INET,SOCK_DGRAM,0);
    if(sock_udp_fd<0){
        perror("udp socket open error");
        return -1;
    }
    sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(CLIENT_SUBNET_IP_ADDR);
    client.sin_port = htons(DEFAULT_UDP_PORT);
    connect(sock_udp_fd,(sockaddr*)&client,sizeof(sockaddr));
    sockaddr_in serA;
    serA.sin_family = AF_INET;
    serA.sin_addr.s_addr = inet_addr(SERVER_A_SUBNET_IP_ADDR);
    serA.sin_port = htons(DEFAULT_UDP_PORT);
    bind(sock_udp_fd,(sockaddr*)&serA,sizeof(sockaddr));

    /*初始化发送接收raw socket */
    int sock_raw_fd = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_IP));
    if(sock_raw_fd<0){
        perror("sock raw open error");
        return -1;
    }
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll,DEFAULT_DEVICE_NAME);
    
    /*初始化发送接收ip层raw socket */
    /*sock_raw_fd=socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
    if(sock_raw_fd < 0){
        perror("ip raw socket open error");
        return -1;
    }
    sockaddr_in ipgw;
    ipgw.sin_family = AF_INET;
    ipgw.sin_addr.s_addr = inet_addr(IPGW_IP_ADDR);
    ipgw.sin_port = htons(IPGW_DEFAULT_PORT);
    if(connect(sock_raw_fd,(sockaddr*)&ipgw,sizeof(ipgw))<0){
        perror("fail to connect ipgw");
    }*/
    
}

void* recv_thread(void*){
    fd_set read_set;
    timeval timeout={1,0};
    
    while(1){
        FD_ZERO(&read_set);
        FD_SET(sock_raw_fd,&read_set);
        FD_SET(sock_udp_fd,&read_set);
        int max_fd = sock_raw_fd > sock_udp_fd?sock_raw_fd:sock_udp_fd;
        int n = select(max_fd+1,&read_set,NULL,NULL,&timeout);
                if(n == 0)
            continue;
        else if(n > 0){
            if(FD_ISSET(sock_udp_fd,&read_set)){ /*udp有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
                int n = recv(sock_udp_fd,data->data,MAX_DATA_SIZE,0);
                if(n < 0){
                    printf("udp recv() error");
                    delete data;
                }else{
                    data->data_len = n;
                    pthread_mutex_lock(&pthread_mutex);
                    data_queue.push(data);
                    pthread_mutex_unlock(&pthread_mutex);
                }  
            }
            if(FD_ISSET(sock_raw_fd,&read_set)){ /*sock raw有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
                int n = recv(sock_raw_fd,data->data,MAX_DATA_SIZE,0);
                if(n < 0){
                    printf("ip raw socket recv() error");
                    delete data->data;
                    delete data;
                }else{
                    data->data_len = n;
                    if(memcmp(data->data+6,SERVER_B_MAC,6)==0){ /*判断数据包是从serverB来的 */
                        data->data+=14;
                        data->data_len-=14;
                        pthread_mutex_lock(&pthread_mutex);
                        data_queue.push(data);
                        pthread_mutex_unlock(&pthread_mutex);
                    }else{
                        delete data->data;
                        delete data; 
                    }
                }  
            }
        }else if(n < 0){
            perror("select error");
        }
    }
}

void* send_thread(void*){
    while(1){
        pthread_mutex_lock(&pthread_mutex);
        if(data_queue.size()==0){
            pthread_mutex_unlock(&pthread_mutex);
            usleep(1000000);
            continue;
        }
        packet *data=data_queue.front();
        data_queue.pop();
        pthread_mutex_unlock(&pthread_mutex);
        u_int32_t *data32 = (u_int32_t*)(data->data);
        u_int32_t src_ip = *(data32+3);
        u_int32_t dest_ip = *(data32+3);
        if(src_ip == inet_addr(IPGW_IP_ADDR)){ /*来自于ipgw的包 */
            int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC,SERVER_B_MAC);
            if(err<0){
                printf("send_ip_ll to ipgw error!\n");
            }
        }else if(src_ip == inet_addr(CLIENT_SUBNET_IP_ADDR)){
            if(dest_ip==inet_addr(IPGW_IP_ADDR)
                    &&(*data->data+32)==0x02){    /*若是发送到ipgw的握手包则转发到client */
                int n = send(sock_udp_fd,data->data,data->data_len,0);
                if(n<0){
                    perror("send tcp dgram error");
                }
            }else{          /*否则发到网关 让网关处理 也就是伪装ip直接发送ipgw */
                int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC,GATEWAY_MAC);
                if(err<0){
                    printf("send_ip_ll to ipgw error!\n");
                }
            }
        }
        delete data->data;
        delete data;
    }
}

void* main_thread(void*){
    pthread_t recv,send;
    pthread_mutex_init(&pthread_mutex,NULL);
    int status=init();
    if(status<0){
        printf("program error!\n");
        _exit(1);
    }
    pthread_create(&recv,NULL,recv_thread,NULL);
    pthread_create(&send,NULL,send_thread,NULL);
}


int main(){
    pthread_t main;
    int err = pthread_create(&main,NULL,main_thread,NULL);
    if(err != 0){
        perror("fail to create thread");
        return -1;
    }
    pthread_join(main,NULL);
    while(1){
        sleep(1000000);
    }
}