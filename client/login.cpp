#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include"../lib/checksum.h"
#include"../lib/nettools.h"
#include"../lib/ipgw.h"

#define SUBNET_IP "192.168.1.102"
#undef SERVER_DOMAIN
#define SERVER_DOMAIN "localhost"

#pragma pack(1)

u_char user_name[]={'c','z','f','z','c'};
u_char password[]={'c','a','o','r','i','c','e'};
u_int16_t user_name_len = 5;
u_int16_t password_len = 7;

struct dgram_data{
    u_char op_code = 0;
    u_int16_t data_len = 0;
    u_char data[MAX_DATA_SIZE] = {0};
};

int sock_tcp_fd,sock_udp_fd;

int init(){
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
    printf("%d hahahh\n",sock_udp_fd);

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
}
/* 
int get_content_from_dgram(u_char* content,u_int16_t* content_data_len,const u_char* dgram){
    if(content==NULL||content_data_len==NULL){
        printf("null pointer from get_content_from_dgram()");
        return -1;
    }
    memcpy(content_data_len,dgram+1,2);
    memcpy(content,dgram+3,*content_data_len);
}

int set_dgram_from_content(u_char* dgram,u_int16_t* dgram_data_len,const u_char op_code,
        const u_int16_t content_len,const u_char* content){
    if(dgram==NULL||dgram_data_len==NULL||content==NULL){
        printf("null pointer from set_dgram_from_content()");
    }
    dgram[0] = op_code;
    u_int16_t c_len = content_len;
    memcpy(dgram+1,&c_len,2);
    memcpy(dgram+3,content,content_len);
    *dgram_data_len = c_len+3;
}*/

int request_data(int sock_fd,const struct dgram_data* send_data,struct dgram_data* recv_data){
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
    if(n<0){
        printf("error to send udp %d\n",errno);
        return -1;
    }
    u_char buf[MAX_DATA_SIZE];
    sockaddr_in from;
    socklen_t from_len = sizeof(sockaddr_in);
    n = recvfrom(sock_udp_fd,buf,MAX_DATA_SIZE,0,(sockaddr*)&from,&from_len);
    if(n<0){
        printf("error to recv udp %d\n",errno);
        return -1;
    }
    u_char mes[MAX_DATA_SIZE];
    u_int16_t mes_len = 0;
    memcpy(&mes_len,mes,2);
    memcpy(mes,buf+2,mes_len);
    mes[mes_len] = '\0';
    printf("result from serverA: %s\n",mes);
 /*    if(buf[0]==14){
        printf("success!\n");
    }else if(buf[0]==0){
        buf[n]='\0';
        printf("err: %s\n",buf);
        return -1;
    }*/
    return 0;
}

void *main_thread(void*){
    u_int32_t subnet_ip = inet_addr(SUBNET_IP);
    u_int32_t sa_ip = 0;
    u_char session_key[16];
    if(init()<0)
        return NULL;
    if(step_request_to_login(session_key,user_name,user_name_len,password,password_len)<0)
        return NULL;
    if(step_1_connect_to_ipgw(session_key,&subnet_ip,&sa_ip)<0)
        return NULL;
    in_addr sa;
    sa.s_addr = sa_ip;
    printf("server A ip is %s\n",inet_ntoa(sa));
     
    if(step_2_connect_to_ipgw(sa_ip,user_name,user_name_len)<0)
        return NULL;
    
}

int main(){
    pthread_t thread_main;
    pthread_create(&thread_main,NULL,main_thread,NULL);
    pthread_join(thread_main,NULL);
}