#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<cerrno>
#include<queue>
#include"nettools.h"
#include"ipgw.h"

using namespace std;

pthread_mutex_t pthread_mutex;
static int sock_udp_fd,sock_raw_fd;
static queue<packet*> data_queue;
static sockaddr_ll addr_ll,addr_ll_main;
static sockaddr_in client;



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
     
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(CLIENT_NAT_IP_ADDR);
    client.sin_port = htons(DEFAULT_UDP_PORT);
    /* int n =  connect(sock_udp_fd,(sockaddr*)&client,sizeof(sockaddr));
    */
    sockaddr_in serA;
    serA.sin_family = AF_INET;
    serA.sin_addr.s_addr = inet_addr(SERVER_A_IP_ADDR);
    serA.sin_port = htons(DEFAULT_UDP_PORT);
    int n = bind(sock_udp_fd,(sockaddr*)&serA,sizeof(sockaddr));
    if(n<0){
	perror("bind error\n");
    }

    /*初始化发送接收raw socket */
    sock_raw_fd = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_IP));
    if(sock_raw_fd<0){
        perror("sock raw open error");
        return -1;
    }
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll,DEFAULT_DEVICE_NAME);
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll_main,DEFAULT_DEVICE_NAME_MAIN);
    
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
    return 0;
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
        if(n == 0){
            continue;
	}else if(n > 0){
            if(FD_ISSET(sock_udp_fd,&read_set)){ /*udp有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
            //    int n = recv(sock_udp_fd,data->data,MAX_DATA_SIZE,0);
                int n = recv_udp_unpack_to_ip(sock_udp_fd,data->data,&(data->data_len),&client);
                printf("datalen: %d\n",data->data_len);
		        printf("udp:\n");
             //   print_data(data->data,data->data_len);
                if(n < 0){
                    printf("udp recv() error\n");
                    delete data->data;
                    delete data;
                    continue;
                }else{
                    if(*(data->data+9)==0x06){
                        printf("udp receved\n");
                        /*修改目的ip 并且重新计算校验和*/
                        u_int32_t *data32 = (u_int32_t*)data->data;
                        if(*(data32+4)==inet_addr(CLIENT_SUBNET_IP_ADDR)){
                            *(data32+4)=inet_addr(SERVER_B_SUBNET_IP_ADDR);
                        }

                        u_char bt = *(data->data);
                        bt = bt<<4;
                        u_int16_t ip_hdr_len = bt/4;
                            
                        u_int32_t src_ip = *(data32+3);
                        u_int32_t dest_ip = *(data32+4);
                        getsum_ip_packet(data->data);
                        getsum_tcp_packet(data->data+ip_hdr_len,data->data_len-ip_hdr_len,src_ip,dest_ip);

                        pthread_mutex_lock(&pthread_mutex);
                        data_queue.push(data);
                        pthread_mutex_unlock(&pthread_mutex);
                    }else{
                        printf("udp unreceved\n");
                        delete data->data;
                        delete data;
                        continue;
                    }
                }  
            }
            if(FD_ISSET(sock_raw_fd,&read_set)){ /*sock raw有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
                socklen_t socklen=sizeof(sockaddr_ll);
		        sockaddr_ll addr_recv;
                int n = recvfrom(sock_raw_fd,data->data,MAX_DATA_SIZE,0,(sockaddr*)&addr_recv,&socklen);
                if(addr_recv.sll_ifindex!=addr_ll.sll_ifindex){
                    delete data->data;
                    delete data;
		            continue;
                }
             
               // printf("raw:\n");
               // print_data(data->data,n);
                if(n < 0){
                    printf("raw socket recvfrom() error");
                    delete data->data;
                    delete data;
                }else{
                    data->data_len = n;
                    u_int32_t client_subnet = inet_addr(CLIENT_SUBNET_IP_ADDR);
                    if(memcmp(data->data+6,SERVER_B_MAC_SUBNET,6)==0&&*(data->data+23)==0x06&&*(data->data+47)!=0x12){ /*判断是从serverB来的tcp数据包 并且过滤raw socket发送的包*/
                        printf("raw receved\n");
                        u_char temp[MAX_DATA_SIZE];
                        memcpy(temp,data->data+14,data->data_len-14);
                        memcpy(data->data,temp,data->data_len-14);
                        data->data_len-=14;
                        u_char bt = *(data->data);
                        bt = bt<<4;
                        u_int16_t ip_hdr_len = bt/4;
                     //   printf("ip_hdr_len:%d\n",ip_hdr_len);
                        u_int32_t *data32 = (u_int32_t*)data->data; 
                        if(*(data->data+33)!=0x02){ //不是tcp握手包则伪装src ip发送至ipgw
                            *(data32+3) = inet_addr(CLIENT_NAT_IP_ADDR);
                        }else{                      //是tcp握手包则伪装为客户端内网ip通过udp打包
                            *(data32+3) = inet_addr(CLIENT_SUBNET_IP_ADDR);
                        }
                        u_int32_t src_ip = *(data32+3);
                        u_int32_t dest_ip = *(data32+4);
                        getsum_ip_packet(data->data);
                        getsum_tcp_packet(data->data+ip_hdr_len,data->data_len-ip_hdr_len,src_ip,dest_ip);
                        pthread_mutex_lock(&pthread_mutex);
                        data_queue.push(data);
                        pthread_mutex_unlock(&pthread_mutex);
                    }else{
              //          printf("raw unreceved\n");
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
            continue;
        }
        packet *data=data_queue.front();
        data_queue.pop();
        pthread_mutex_unlock(&pthread_mutex);
        u_int32_t *data32 = (u_int32_t*)(data->data);
        u_int32_t src_ip = *(data32+3);
        u_int32_t dest_ip = *(data32+4);
        if(src_ip == inet_addr(IPGW_IP_ADDR)){ /*来自于ipgw的包 */
            int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC_SUBNET,SERVER_B_MAC_SUBNET);
            printf("\nsended syn ack from ipgw\n");
        //    print_data(data->data,data->data_len); 
            if(err<0){
                printf("send_ip_ll to serb error!\n");
            }
        }else if(dest_ip==inet_addr(IPGW_IP_ADDR)){
            if(src_ip==inet_addr(CLIENT_SUBNET_IP_ADDR)&&
                    inet_addr(CLIENT_SUBNET_IP_ADDR)!=inet_addr(CLIENT_NAT_IP_ADDR)){    /*源为nat内网则发送到client */
                int n = sendto(sock_udp_fd,data->data,data->data_len,0,(sockaddr*)&client,sizeof(sockaddr));
		printf("sended udp\n");
                if(n<0){
                    perror("send tcp dgram error");
                }
            }else if(src_ip==inet_addr(CLIENT_NAT_IP_ADDR)){          /*否则发到网关 让网关处理 也就是伪装ip直接发送ipgw */
                printf("\nhhahasb\n");
		    int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll_main,SERVER_A_MAC_OUT,GATEWAY_MAC);
		if(err<0){
                    printf("send_ip_ll to ipgw error!\n");
                }
            }
        }
        delete data->data;
        delete data;
    }
}

void* print_thread(void*){
    while(true){
        usleep(500000);
        pthread_mutex_lock(&pthread_mutex);
        printf("queue size: %d\n",data_queue.size());
        pthread_mutex_unlock(&pthread_mutex);
    }
    
}

void* main_thread(void*){
    pthread_t recv,send,print;
    pthread_mutex_init(&pthread_mutex,NULL);
    int status=init();
    if(status<0){
        printf("program error!\n");
        _exit(1);
    }
    pthread_create(&recv,NULL,recv_thread,NULL);
    pthread_create(&send,NULL,send_thread,NULL);
    pthread_create(&print,NULL,print_thread,NULL);
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
