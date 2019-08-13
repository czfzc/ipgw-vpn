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
#define SERVER_A_IP_ADDR "58.154.192.121"
#define DEFAULT_UDP_PORT 1026
#define DEFAULT_DEVICE_NAME "enp2s0"
#define MAX_BUFFER_QUEUE_SIZE 5000
#define MAX_DATA_SIZE 1536

using namespace std;

u_char SERVER_A_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x02};
u_char SERVER_B_MAC[6]={0x02,0x42,0xac,0x11,0x00,0x03};


int sock_raw_fd;
int sock_dgram_fd;
struct sockaddr_in local;
struct sockaddr_ll local_raw;

pthread_t hRecv;
pthread_t hSend;

typedef struct{
    uint32_t data_len;
    u_char* data;
}packet;

static queue<packet*>PackBuff;
pthread_mutex_t mutex_queue;

pthread_mutex_t mutex_ctlsend;

void get_default_sockaddr_ll_send(int fd,sockaddr_ll *addr_ll,char *nic_name);

inline void print_data(u_char *data,int data_len){
    printf("\n");
    for(int i=0;i<data_len;i++){
        printf("%02x ",data[i]);
        if(i!=0&&i%16==0)
            printf("\n");
    }
    printf("\n");
}

void *recv_thread(void *)
{

    /*初始化接收udp数据报 */
    sock_dgram_fd=socket(AF_INET,SOCK_DGRAM,0);
    if(sock_dgram_fd < 0){
        perror("dgram socket open error!");
    }
    
    local.sin_family = AF_INET;
    local.sin_port = htons(DEFAULT_UDP_PORT);
    local.sin_addr.s_addr = inet_addr(SERVER_A_IP_ADDR);
    if(bind(sock_dgram_fd, (struct sockaddr*)&local,sizeof(sockaddr)) < 0){
        perror("bind udp error");
    }

    u_char buf[MAX_DATA_SIZE];
    packet *temp;
    struct sockaddr_in client;
    socklen_t server_len;
    while(1){
        temp=new packet;
        int len = recvfrom(sock_dgram_fd,buf,MAX_DATA_SIZE,0,(struct sockaddr*)&client,&server_len);
        if(len<0){
            printf("recvfrom error %d\n",errno);
            continue;    
        }        
        temp->data_len = len;
        temp->data = new unsigned char[len];
        memcpy(temp->data,buf,len);

        pthread_mutex_lock(&mutex_queue);
        PackBuff.push(temp);
        pthread_mutex_unlock(&mutex_queue);

        printf("%d \n",PackBuff.size());
    }
}

void *send_thread(void *)
{
    /*初始化发送raw数据 */
    sock_raw_fd=socket(PF_PACKET,SOCK_RAW,htons(ETH_P_IP));
    if(sock_raw_fd < 0){
        perror("raw socket open error!");
        return nullptr;
    }
    
    get_default_sockaddr_ll_send(sock_raw_fd,&local_raw,DEFAULT_DEVICE_NAME);

    u_char buf[MAX_DATA_SIZE];
    u_int16_t *data16 = (u_int16_t*)buf;
    memcpy(buf,SERVER_B_MAC,6);
    memcpy(buf+6,SERVER_A_MAC,6);
    data16[6]=htons(ETH_P_IP);
    packet *temp;
    socklen_t socklen=sizeof(sockaddr_ll);
    while(1)
    {
        pthread_mutex_lock(&mutex_queue);
        if (PackBuff.empty())
        {
            pthread_mutex_unlock(&mutex_queue);
            usleep(20);
            continue;
        }
        temp=PackBuff.front();
        PackBuff.pop();
        pthread_mutex_unlock(&mutex_queue);

        memcpy(buf+14,temp->data,temp->data_len);
        
        int len=sendto(sock_raw_fd,buf,(size_t)(temp->data_len+14),0,(sockaddr*)&local_raw,socklen);
        delete temp->data;
        delete temp;
    }
}

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

void get_default_sockaddr_ll_send(int fd,sockaddr_ll *addr_ll,char *nic_name)
{
    memset(addr_ll,0,sizeof(addr_ll));
    addr_ll->sll_ifindex=get_nic_index(fd,nic_name);
    addr_ll->sll_family=PF_PACKET;
    addr_ll->sll_halen=ETH_ALEN;
}

void server_start(){
    
    pthread_mutex_init(&mutex_queue,NULL);

    pthread_create(&hRecv,NULL,recv_thread,NULL);
    pthread_create(&hSend,NULL,send_thread,NULL);

}

int main(){
    server_start();
    while(1)
        usleep(100000);
    return 0;
}
