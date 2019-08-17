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

#include"checksum.cpp"

#include<map>
#include<queue>

#define IPGW_IP_ADDR "202.118.1.87"
#define SERVER_A_IP_ADDR "192.168.1.102"
#define SERVER_B_IP_ADDR "192.168.1.104"
#define CLIENT_NAT_IP_ADDR "58.154.192.75"
#define DEFAULT_UDP_PORT 1026
#define DEFAULT_DEVICE_NAME "wlp1s0"
#define MAX_BUFFER_QUEUE_SIZE 5000
#define MAX_DATA_SIZE 1536

#define LINK_LAYER 0x00 /*数据链路层 */
#define IP_LAYER 0x01   /*IP层 */
#define TRANS_LAYER 0x02    /*传输层 */

using namespace std;

u_char SERVER_A_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x02};
u_char SERVER_B_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x03};
u_char GATEWAY_MAC[6]={0x38,0x97,0xd6,0x51,0xa0,0xa2};

typedef struct{
    uint32_t data_len;
    u_char *data;
    u_char packet_type;
}packet;

pthread_mutex_t pthread_mutex;
static queue<packet*> data_queue;
static int sock_ipgw_ip_fd,sock_raw_fd,sock_udp_fd;
static sockaddr_ll addr_ll; /*链路层发送物理地址的结构体 */
static vector<int> seq_numbers;
static map<int,int> seq_port_map; 
/*
    定义map<((serverA syn seq)-(client syn sql)),(serverA src port)<<4+(client src port)> 
    也就是同步码偏移和两源端口的对应关系
*/
static map<long,short> client_seq_map;
/*
    定义map<(client syn seq)<<8+(ipgw syn ack seq),(src port)> 
    也就是客户端打洞的src port和client与ipgw主机syn seq码的关系
*/

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
 * 检测tcp ack seq-number是否属于确认一个syn ack
 * 
 *************************************/

bool check_ack_seq(int seq_number){
    vector<int>::iterator ite;
    for(ite = seq_numbers.begin();ite!=seq_numbers.end();ite++){
        if(seq_number==*ite+1){
            seq_numbers.erase(ite);
            return true;
        }
    }
    return false;
}

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
    int len = recvfrom(sock_dgram_fd,ip_tcp_data,MAX_DATA_SIZE,0,(struct sockaddr*)&client,&server_len);
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
 * 生成tcp syn ack的ip报文
 * 
 *************************************/

void generate_syn_ack_ip_packet(packet *data,int src_ip,int dest_ip,
        u_int16_t src_port,u_int16_t dest_port,u_int32_t seq,u_int32_t ack){
    u_char arr[]={
        0x45,0x00,0x00,0x3c,0xf2,0x92,0x40,0x00,
        0x40,0x06,0x81,0x87,0x3a,0x9a,0xc0,0x3a,
        0xca,0x76,0x01,0x57,0xab,0xa2,0x00,0x50,
        0xb5,0x34,0xf2,0xdb,0x00,0x00,0x00,0x00,
        0xa0,0x02,0xfa,0xf0,0x90,0x8a,0x00,0x00,
        0x02,0x04,0x05,0xb4,0x04,0x02,0x08,0x0a,
        0x2b,0xcc,0x76,0x13,0x00,0x00,0x00,0x00,
        0x01,0x03,0x03,0x07
    };
    data->data_len=60;
    u_char *ptr=arr;
    u_int16_t *ptr16=(u_int16_t*)ptr;
    u_int32_t *ptr32=(u_int32_t*)ptr;
    *(ptr16+10)=src_port;
    *(ptr16+11)=dest_port;
    *(ptr32+6)=seq;
    *(ptr32+7)=ack;
    *(ptr32+3)=src_ip;
    *(ptr32+4)=dest_ip;
    getsum_tcp_packet(arr+20,data->data_len-20,src_ip,dest_ip);
    getsum_ip_packet(arr);
    /*计算ip和tcp校验和 */
    memcpy(data->data,arr,data->data_len);
    data->packet_type = IP_LAYER;
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
 * 接收所有tcp数据报（ip层raw socket）
 * 
 *************************************/

int recv_tcp_dgram(int sock_ip_fd,u_char *ip_tcp_data,int *data_len){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
        return ret;
    }
    sockaddr_in addr;
    socklen_t socklen = sizeof(sockaddr_in);
    int len=recvfrom(sock_ip_fd,ip_tcp_data,1024,0,(sockaddr*)&addr,&socklen);
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
 * 发送tcp数据报(ip层) 未完成
 * 
 *************************************/

int send_tcp_dgram(int sock_ip_fd,u_char *ip_tcp_data,int data_len){
    int ret = 0;
    if(ip_tcp_data == NULL){
        ret = -1;
	printf("null data");
        return ret;
    }
    socklen_t socklen=sizeof(sockaddr_in);
    int len=send(sock_ip_fd,ip_tcp_data,(size_t)data_len,0);
    if(len<0){
	printf("error sendto,errno:%d\n",errno);
        ret = -1;
        return ret;
    }
    return ret;
}

/*************************************
 * 
 * 初始化socket线程
 * 
 *************************************/

int init(){
    int ret = 0;
        /*初始化给ipgw的socket （ip层raw socket） */
    /*sock_ipgw_ip_fd=socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
    if(sock_ipgw_ip_fd < 0){
        perror("create ipgw send sock error");
        ret = -1;
        return ret;
    }
    sockaddr_in ipgw_addr;
    ipgw_addr.sin_family = AF_INET;
    ipgw_addr.sin_addr.s_addr = inet_addr(IPGW_IP_ADDR);
    connect(sock_ipgw_ip_fd,(sockaddr*)&ipgw_addr,sizeof(sockaddr));
    int one=1;
    if(setsockopt(sock_ipgw_ip_fd,IPPROTO_TP,IP_HDRINCL,&one,sizeof(int))<0){
        perror("setsockopt error");
        ret = -1;
        return ret;
    }*/

    /*初始化发送接收serverB raw socket（链路层） */
    sock_raw_fd = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_IP));
    if(sock_raw_fd < 0){
        perror("create serb raw sock error");
        ret = -1;
        return ret;
    }
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll,DEFAULT_DEVICE_NAME);
    
    /*初始化发送接收client udp */
    sock_udp_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(sock_udp_fd < 0){
        perror("sreate client udp sock error");
        ret = -1;
        return ret;
    }
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr(CLIENT_NAT_IP_ADDR);
    bind(sock_udp_fd,(sockaddr*)&client_addr,sizeof(sockaddr));
    connect(sock_udp_fd,(sockaddr*)&client_addr,sizeof(sockaddr));
    return ret;
}

/*************************************
 * 
 * raw socket发送线程
 * 
 *************************************/

void* sendpkt_thread(void*){
    while(1){
        pthread_mutex_lock(&pthread_mutex);
        if(data_queue.size()==0){
            pthread_mutex_unlock(&pthread_mutex);
            usleep(1000000);
            continue;
        }
        packet *data = data_queue.front();
        data_queue.pop();
        pthread_mutex_unlock(&pthread_mutex);
        u_char *data_ptr=data->data;
        u_int16_t *data_ptr16 = (u_int16_t*)data_ptr;
        u_int32_t *data_ptr32 = (u_int32_t*)data_ptr;
        /*不同层数据包的不同层处理方式 */
        if(data->packet_type == IP_LAYER){    /*IP层 */
            u_char protrcol = *(data_ptr+9);
            u_int16_t hd_len=(*data_ptr<<4)/4;
            u_int32_t seq_number = *(data_ptr32+hd_len/4+1);
            u_int32_t ack_number = *(data_ptr32+hd_len/4+2);
            u_int32_t src_ip=*(data_ptr32+3);
            u_int32_t dst_ip=*(data_ptr32+4);
            u_int16_t src_port=*(data_ptr16);
            u_int16_t dst_port=*(data_ptr16+1);

            if(protrcol == IPPROTO_TCP){
                seq_number = *(data_ptr32+1);
                if(/*(*(data_ptr16+16))&(0x0fff)==0x0002&&*/
                        dst_ip==inet_addr(IPGW_IP_ADDR)){   /*判断ipgw tcp syn数据报 */

                    seq_numbers.push_back(seq_number);
                    /*此处向serverB发送ipgw syn ack */
                    packet tcp_data;
                    tcp_data.data = new u_char[MAX_DATA_SIZE];
                    int test_ipgw_seq=0x12345678;
                    generate_syn_ack_ip_packet(&tcp_data,dst_ip,src_ip,dst_port,src_port,test_ipgw_seq,seq_number+1);
                    send_ip_ll(sock_raw_fd,tcp_data.data,tcp_data.data_len,addr_ll,SERVER_A_MAC,SERVER_B_MAC);
                }else if((*(data_ptr16+6))&(0x0fff)==0x0010
                        &&check_ack_seq(seq_number)&&dst_ip==inet_addr(IPGW_IP_ADDR)){   /*判断tcp三次握手之第三次ipgw ack数据报 */
                    /*此处什么也不发 */
                }else{      /*其他包 */
                    if(dst_ip==inet_addr(SERVER_B_IP_ADDR)){
                        send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC,SERVER_B_MAC);
                    }else{
                        send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC,GATEWAY_MAC);
                    }
                }
            }
        }



        
        

    }
}

/*************************************
 * 
 * raw socket接收tcp数据报线程
 * 
 *************************************/

void* recvpkt_thread(void*){
    while(1){
        pthread_mutex_lock(&pthread_mutex);
        if(data_queue.size()>MAX_BUFFER_QUEUE_SIZE){
            pthread_mutex_unlock(&pthread_mutex);
            usleep(20);
            continue;
        }
        pthread_mutex_unlock(&pthread_mutex);
        socklen_t socklen = sizeof(sockaddr_ll);
        packet *data = new packet;
        data->data=new u_char[MAX_DATA_SIZE];
        int n = recvfrom(sock_raw_fd,data->data,MAX_DATA_SIZE,0,(sockaddr*)&addr_ll,&socklen);
        print_data(data->data,n);
        if(n < 0){
            printf("raw socket recvfrom() error");
        }
        data->data+=14;
        data->data_len = n-14;
        data->packet_type = IP_LAYER;
        
        if(memcmp((data->data)+6,SERVER_B_MAC,6)==0){ /*判断数据包若来自serverB则处理 */
            pthread_mutex_lock(&pthread_mutex);
            data_queue.push(data);
            pthread_mutex_unlock(&pthread_mutex);
        }
        
    }
}

/*************************************
 * 
 * 用于接收client打包成udp的ipgw ip数据报
 * 
 *************************************/

void* udp_thread(void*){
    while(1){
        u_char buf[MAX_DATA_SIZE];
        int n = recv(sock_udp_fd,buf,1024,0);
        if(n<0){
            printf("recv udp error");
        }
        packet *data=new packet;
        memcpy(data->data,buf,n);
        data->data_len=n;
        data->packet_type=IP_LAYER;

        pthread_mutex_lock(&pthread_mutex);
        data_queue.push(data);
        pthread_mutex_lock(&pthread_mutex);
        
    }
}

void* main_thread(void*){
    pthread_t recvpkt,sendpkt,udp;
    pthread_mutex_init(&pthread_mutex,NULL);
    int status=init();
    if(status<0){
        printf("program error!\n");
    }
    pthread_create(&recvpkt,NULL,recvpkt_thread,NULL);
    pthread_create(&sendpkt,NULL,sendpkt_thread,NULL);
    pthread_create(&udp,NULL,udp_thread,NULL);
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