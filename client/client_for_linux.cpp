#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<queue>
#include"../lib/checksum.h"
#include"../lib/nettools.h"
#include"../lib/ipgw.h"

#undef SERVER_DOMAIN
#define SERVER_DOMAIN "localhost"

#pragma pack(1)

using namespace std;

u_char user_name[]={'c','z','f','z','c'};
u_char password[]={'c','a','o','r','i','c','e'};
u_int16_t user_name_len = 5;
u_int16_t password_len = 7;

struct dgram_data{
    u_char op_code = 0;
    u_int16_t data_len = 0;
    u_char data[MAX_DATA_SIZE] = {0};
};

pthread_mutex_t pthread_mutex;

static queue<packet*> data_queue;

int sock_tcp_fd,sock_udp_fd,sock_ip_fd;

static u_int32_t client_subnet_ip = 0;

int init(){

    if(get_local_ip_using_create_socket(&client_subnet_ip)<0){
        printf("fail to get local ip addr\n");
        return -1;
    }else{
        in_addr ad;
        ad.s_addr = client_subnet_ip;
        printf("local subnet ip is %s\n",inet_ntoa(ad));
    }
    /*初始化给server发送tcp */
    sock_tcp_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_tcp_fd<0){
        printf("error to create tcp socket\n");
        return -1;
    }
    sockaddr_in server;
    hostent* server_host = new hostent;
    server_host = gethostbyname(SERVER_DOMAIN);
    if(server_host->h_length != 4||server_host->h_addrtype != AF_INET){
        printf("this is not ipv4 address\n");
        return -1;
    }
    memcpy(&server.sin_addr.s_addr,server_host->h_addr_list[0],server_host->h_length);
    server.sin_family = server_host->h_addrtype;
    server.sin_port = htons(DEFAULT_SERVER_PORT);
    bzero(&server.sin_zero,8);
    if(connect(sock_tcp_fd,(sockaddr*)&server,sizeof(sockaddr_in))<0){
        printf("connect error!\n");
        return -1;
    }

    /*初始化发送udp */
    sock_udp_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(sock_udp_fd<0){
        printf("error to create udp socket\n");
        return -1;
    }
    sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = client_subnet_ip;
    client.sin_port = htons(DEFAULT_UDP_PORT);
    if(bind(sock_udp_fd,(sockaddr*)&client,sizeof(sockaddr))<0){
        printf("bind udp error\n");
    }

    /*初始化发送接收ipgw的ip层raw socket */
    sock_ip_fd=socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
    if(sock_ip_fd < 0){
        perror("ip raw socket open error");
        return -1;
    }
    /*设置IP_HDRINCL字段 手动构建ip包 */
    int  one = 1;
	const int *val = &one;
	if (setsockopt(sock_ip_fd, IPPROTO_IP, IP_HDRINCL, val, sizeof(int)))
	{
		perror("setsockopt() error");
		_exit(-1);
	}
    /*设置ip包发至ipgw */
    sockaddr_in ipgw;
    ipgw.sin_family = AF_INET;
    ipgw.sin_addr.s_addr = inet_addr(IPGW_IP_ADDR);
    ipgw.sin_port = htons(IPGW_DEFAULT_PORT);
    if(connect(sock_ip_fd,(sockaddr*)&ipgw,sizeof(ipgw))<0){
        perror("fail to connect ipgw");
    }
    return 0;
}

int print_dgram_data(const struct dgram_data* dgram_data){
    if(dgram_data == NULL)
        return -1;
    if(dgram_data->op_code == 0){
        u_char mes[MAX_DATA_SIZE];
        memcpy(mes,dgram_data->data,dgram_data->data_len);
        mes[dgram_data->data_len] == '\0';
        printf("recv error: %s\nexiting ...",(char*)mes);
        exit(0);
        return -1;
    }else{
        printf("recv data:");
        print_data(dgram_data->data,dgram_data->data_len);
    }
    return 0;
}


/*************************************
 * 
 * 发送线程 发送raw packet至ipgw或打包后的udp dgram至serverA
 * 
 *************************************/

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
        if(src_ip == inet_addr(IPGW_IP_ADDR)){ /*ipgw反射来的 应打包发送给serverA */
            int n = send(sock_udp_fd,data->data,data->data_len,0);
            if(n<0){
                perror("send udp error");
            }
            delete data->data;
            delete data;
        }else if(src_ip == client_subnet_ip){
            int n = send(sock_ip_fd,data->data,data->data_len,0);
            if(n<0){
                perror("send tcp dgram error");
            }
            delete data->data;
            delete data;
        }
    }
    return NULL;
}

/*************************************
 * 
 * 接收线程 接收ipgw的raw packet或接收serverA的udp dgram
 * 
 *************************************/

void* recv_thread(void*){
    fd_set read_set;
    timeval timeout = {1,0};
    while(1){
        FD_ZERO(&read_set);
        FD_SET(sock_udp_fd,&read_set);
        FD_SET(sock_ip_fd,&read_set);
        int max_fd = sock_udp_fd > sock_ip_fd?sock_udp_fd:sock_ip_fd;
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
                    delete data->data;
                    delete data;
                }else{
                    data->data_len = n;
                    pthread_mutex_lock(&pthread_mutex);
                    data_queue.push(data);
                    pthread_mutex_unlock(&pthread_mutex);
                }  
            }
            if(FD_ISSET(sock_ip_fd,&read_set)){ /*sock raw ip有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
                int n = recv(sock_ip_fd,data->data,MAX_DATA_SIZE,0);
                if(n < 0){
                    printf("ip raw socket recv() error");
                    delete data->data;
                    delete data;
                }else{
                    data->data_len = n;
                    pthread_mutex_lock(&pthread_mutex);
                    data_queue.push(data);
                    pthread_mutex_unlock(&pthread_mutex);
                }  
            }
        }else if(n < 0){
            perror("select error");
        }
    }
    return NULL;
}

static int request_data(int sock_fd,const struct dgram_data* send_data,struct dgram_data* recv_data){
    if(write(sock_fd,send_data,send_data->data_len+3)==-1){
        printf("send error!\n");
        return -1;
    }
    printf("sended:\n");
    print_dgram_data(send_data);
    if(read(sock_fd,recv_data,MAX_DATA_SIZE+3)==-1){
        printf("recv error!\n");
        return -1;
    }
    printf("received:\n");
    print_dgram_data(recv_data);
    return 0;
}

/*登录步骤 */
int step_request_to_login(u_char* session_key,const u_char* user_name,const u_int16_t user_name_len,
        const u_char* password,const u_int16_t password_len){
    if(user_name==NULL||session_key==NULL){
        printf("null pointer param!\n");
        return -1;
    }
    dgram_data recv_data,send_data;
    send_data.op_code = 1;
    send_data.data_len = user_name_len;
    memcpy(send_data.data,user_name,user_name_len);
    /*发送op1 接收op2 */
    request_data(sock_tcp_fd,&send_data,&recv_data);
    if(recv_data.op_code != 2) return -1;

    u_char* salt = recv_data.data;
    u_char content[password_len+16];
    u_char salted_password_md5[16];
    memcpy(content,password,password_len);
    memcpy(content+password_len,salt,16);
    md5_16(send_data.data,content,password_len+16);
    send_data.op_code = 3;
    send_data.data_len = 16;
    /*发送op3 接收op4 */
    request_data(sock_tcp_fd,&send_data,&recv_data);
    if(recv_data.op_code != 4) return -1;

    memcpy(session_key,recv_data.data,16);
    return 0;
}

/*联网第一步骤 向server发送申请*/
int step_1_connect_to_ipgw(const u_char* session_key,const u_int32_t* subnet_ip,u_int32_t* serverA_ip){
    if(session_key==NULL||subnet_ip==NULL){
        printf("null pointer param!\n");
        return -1;
    }
    dgram_data recv_data,send_data;
    send_data.op_code = 7;
    send_data.data_len = 20;
    memcpy(send_data.data,session_key,16);
    memcpy(send_data.data+16,subnet_ip,4);
    /*发送op7 接收op8 */
    request_data(sock_tcp_fd,&send_data,&recv_data);

    memcpy(serverA_ip,recv_data.data,4);
    return 0;
}

/*联网第二步骤 发送向serverA申请连接的信号(udp)*/
int step_2_connect_to_ipgw(u_int32_t serverA_ip,const u_char* user_name,const u_int32_t user_name_len){

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = serverA_ip;
    sa.sin_port = htons(1026);
    
    bzero(&sa.sin_zero,8);

    u_char data[user_name_len+2];
    memcpy(data,&user_name_len,2);
    memcpy(data+2,user_name,user_name_len);

    int n = sendto(sock_udp_fd,data,user_name_len+2,0,(sockaddr*)&sa,sizeof(sockaddr_in));
    printf("sended user_name udp\n");
    print_data(data,user_name_len+2);
    if(n<0){
        printf("error to send udp %d\n",errno);
        return -1;
    }
    u_char buf[MAX_DATA_SIZE];
    sockaddr_in from;
    socklen_t from_len = sizeof(sockaddr_in);
    n = recv(sock_udp_fd,buf,MAX_DATA_SIZE,0);
    if(n<0){
        printf("error to recv udp %d\n",errno);
        return -1;
    }
    printf("recv udp:\n");
    print_data(buf,n);
    u_char mes[MAX_DATA_SIZE];
    u_int16_t mes_len = 0;
    memcpy(&mes_len,buf+1,2);
    memcpy(mes,buf+3,mes_len);
    mes[mes_len] = '\0';
    printf("result from serverA: %s\n",mes);
     if(buf[0]==15){
        printf("success!\n");
    }else if(buf[0]==0){
        printf("error!\n");
        return -1;
    }
    return 0;
}



int step_open_dgram_recv_and_pack(u_int32_t sa_ip){
    /*绑定udp连接serverA */
    sockaddr_in sa;
    sa.sin_addr.s_addr = sa_ip;
    sa.sin_family = AF_INET;
    sa.sin_port = htons(DEFAULT_UDP_PORT);
    bzero(&sa.sin_zero,8);
    if(connect(sock_udp_fd,(sockaddr*)&sa,sizeof(sockaddr))<0){
        printf("step connect to sa error\n");
        return -1;
    }
    /*发送握手包通知client已上线 */
    send(sock_udp_fd,"hello",5,0);

    pthread_t recv,send;
    pthread_mutex_init(&pthread_mutex,NULL);
    pthread_create(&recv,NULL,recv_thread,NULL);
    pthread_create(&send,NULL,send_thread,NULL);
    while(true){
        usleep(10000000);
    }
}

void *main_thread(void*){
    u_int32_t sa_ip = 0;
    u_char session_key[16];
    if(init()<0)
        return NULL;
    if(step_request_to_login(session_key,user_name,user_name_len,password,password_len)<0)
        return NULL;
    if(step_1_connect_to_ipgw(session_key,&client_subnet_ip,&sa_ip)<0)
        return NULL;
    in_addr sa;
    sa.s_addr = sa_ip;
    printf("server A ip is %s\n",inet_ntoa(sa));
     
    if(step_2_connect_to_ipgw(sa_ip,user_name,user_name_len)<0)
        return NULL;

    if(step_open_dgram_recv_and_pack(sa_ip)<0)
        return NULL;
    
}

int main(){
    pthread_t thread_main;
    pthread_create(&thread_main,NULL,main_thread,NULL);
    pthread_join(thread_main,NULL);
}