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
#include"../lib/checksum.h"
#include"../lib/ipgw.h"
#include"../lib/cacheuser.h"

#define SERVER_DOMAIN "s.ipgw.top"

pthread_mutex_t pthread_mutex;
static int sock_udp_fd,sock_raw_fd,sock_ip_fd;
static queue<packet*> data_queue;
static sockaddr_ll addr_ll,addr_ll_main;
/*static sockaddr_in client;*/
cacheuser cache;

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
    
    /*client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(CLIENT_NAT_IP_ADDR);
    client.sin_port = htons(DEFAULT_UDP_PORT);
     int n =  connect(sock_udp_fd,(sockaddr*)&client,sizeof(sockaddr));
    */
    sockaddr_in serA;
    serA.sin_family = AF_INET;
    serA.sin_addr.s_addr = htons(INADDR_ANY);
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
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll,(char*)DEFAULT_DEVICE_NAME);
    get_default_sockaddr_ll_send(sock_raw_fd,&addr_ll_main,(char*)DEFAULT_DEVICE_NAME_MAIN);
    
    /*初始化发送ip层raw socket */
    sock_ip_fd=socket(AF_INET,SOCK_RAW,IPPROTO_TCP);
    if(sock_ip_fd < 0){
        perror("ip raw socket open error");
        return -1;
    }

    int  one = 1;
	const int *val = &one;
	if (setsockopt(sock_ip_fd, IPPROTO_IP, IP_HDRINCL, val, sizeof(int)))
	{
		perror("setsockopt() error");
		_exit(-1);
	}
    /*sockaddr_in ipgw;
    ipgw.sin_family = AF_INET;
    ipgw.sin_addr.s_addr = inet_addr(IPGW_IP_ADDR);
    ipgw.sin_port = htons(IPGW_DEFAULT_PORT);
    if(connect(sock_raw_fd,(sockaddr*)&ipgw,sizeof(ipgw))<0){
        perror("fail to connect ipgw");
    }*/
    return 0;
}

/*-1代表验证失败 0代表验证成功 */
int indetify_user_by_user_name_and_src_ip(const u_char* user_name,u_int16_t user_name_len,
        u_int32_t client_src_ip,u_char* error_mes,u_int16_t *error_mes_len,u_char* session_key,
        u_int32_t* client_subnet_ip){
    u_int32_t server_ip = inet_addr("58.154.193.182");
   // socket_resolver(SERVER_DOMAIN,&server_ip);

    int sockfd;
    u_char buf[MAX_DATA_SIZE];
    struct sockaddr_in server;
    while((sockfd = socket (AF_INET,SOCK_STREAM,0))==-1);
    server.sin_family = AF_INET;
    server.sin_port = htons(DEFAULT_SERVER_PORT);
    server.sin_addr.s_addr = server_ip;
    bzero(&(server.sin_zero),8);

    while(connect(sockfd,(struct sockaddr*)&server,sizeof(struct sockaddr))==-1);

    printf("connected to %s\n",inet_ntoa(server.sin_addr));

    u_int16_t src_ip_len=4;
    u_int16_t datalen = user_name_len+4+src_ip_len;
    u_char data[datalen];
    memcpy(data,&user_name_len,2);
    memcpy(data+2,user_name,user_name_len);
   printf("datalen:%d\n",datalen);
    memcpy(data+2+user_name_len,&src_ip_len,2);
    memcpy(data+4+user_name_len,&client_src_ip,src_ip_len);
    
    u_char send_data[datalen+3];
    send_data[0] = 11;
    memcpy(send_data+1,&datalen,2);
    memcpy(send_data+3,data,datalen);
    int n = send(sockfd,send_data,datalen+3,0);
    printf("sended op 11\n");
    if(n<0){
    	printf("error to send tcp\n");
    }
    printf("send len %d\n",n);
   // printf("sended:\n");
   // print_data(data,datalen);
    n = recv(sockfd,buf,MAX_DATA_SIZE,0);
    if(n<0){
	    printf("error to recv udp\n");
    }
    printf("recvdata:\n");
    print_data(buf,n);
    if(buf[0] == 0){
        u_int16_t error_len = 0;
        memcpy(&error_len,buf+1,2);
        memcpy(error_mes,buf+3,error_len);
        *error_mes_len = error_len;
        printf("server error!");
        return -1;
    }else{
        memcpy(session_key,buf+3,16);
        memcpy(client_subnet_ip,buf+3+16,4);
        return 0;
    }
    close(sockfd);
}


/*********************************
 * 
 * 向指定serverB发送联网指令 返回0代表成功 -1代表失败 -2代表目标返回数据包格式错误
 * 
 ********************************/

int send_ipgw_flood_command(u_int32_t sb_ip,u_char* mes,u_int16_t* mes_len){
    in_addr ad;
    ad.s_addr = sb_ip;
    printf("goto send flood function %s\n",inet_ntoa(ad));
    int sock_udp_to_sb_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(sock_udp_to_sb_fd < 0){
        printf("open udp socket to sb error!\n");
        return -1;
    }
    sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_addr.s_addr = sb_ip;
    addr_in.sin_port = htons(DEFAULT_SERVER_B_UDP_PORT);
    bzero(&addr_in.sin_zero,8);
    if(connect(sock_udp_to_sb_fd,(sockaddr*)&addr_in,sizeof(sockaddr_in))<0){
        printf("connect to sb error!\n");
        return -1;
    }
    u_char data[]={0x0d,0x00,0x05,0x01,0x02,0x03,0x04,0x05};
    if(sendto(sock_udp_to_sb_fd,data,8,0,(sockaddr*)&addr_in,sizeof(sockaddr_in))<0){
        printf("send udp to sb error!\n");
        return -1;
    }
    socklen_t socklen = sizeof(sockaddr_in);
    u_char buf[MAX_DATA_SIZE];
    int len = recvfrom(sock_udp_to_sb_fd,buf,MAX_DATA_SIZE,0,(sockaddr*)&addr_in,&socklen);
    if(len<0){
        printf("recv udp from sb error\n");
        return -1;
    }
    print_data(buf,len);
    if(buf[0]==14){     /*连接成功 */
        u_int16_t m_len = 0;
        memcpy(&m_len,buf+1,2);
        m_len = ntohs(m_len);
        memcpy(mes,buf+3,m_len);
        *mes_len = m_len;
        return 0;
    }else if(buf[0]==0){    /*连接失败 */
        u_int16_t m_len = 0;
        memcpy(&m_len,buf+1,2);
        m_len = ntohs(m_len);
        memcpy(mes,buf+3,m_len);
        *mes_len = m_len;
        return -1;
    }else{                  /*数据包异常 */
        printf("error udp dgram!\n");
        return -2;
    } 
}

void* indetify_thread(void* args){
    sockaddr_in *from_client = new sockaddr_in;
    memcpy(from_client,((void**)args)[0],sizeof(sockaddr_in));
    u_int16_t* user_name_len = (u_int16_t*)((void**)args)[1];
    u_char* user_name = (u_char*)((void**)args)[2];
    u_char error_mes[256];
    u_char session_key[16];
    u_int32_t c_subnet_ip = 0;
    u_int16_t error_mes_len = 0;
    print_data(user_name,5);
    printf("identifying...  %s\n",inet_ntoa(from_client->sin_addr));
    int status = indetify_user_by_user_name_and_src_ip(user_name,*user_name_len,from_client->sin_addr.s_addr,
            error_mes,&error_mes_len,session_key,&c_subnet_ip);
    printf("identify result %d\n",status);
    if(status==-1){     /*验证不通过 直接向client返回 */
        u_char data[error_mes_len+2];
        memcpy(data,&error_mes_len,2);
        memcpy(data+2,error_mes,error_mes_len);
        int n = sendto(sock_udp_fd,data,error_mes_len+2,0,(sockaddr*)from_client,sizeof(sockaddr));
        if(n!=-1){
            return NULL;
        }else{
            printf("send udp to client to return error fail");
        }
    }else{              /*验证通过 开始分配空余主机做serverB并且给serverB下达联网指令 */
        /*获取本机ip */
       /* u_int32_t this_serverA_ip;
        get_eth_IP(DEFAULT_DEVICE_NAME_MAIN,(u_char*)&this_serverA_ip);*/
        u_int32_t sb_ip = 0;
        bool status = cache.bind(from_client->sin_port,from_client->sin_addr.s_addr,c_subnet_ip,session_key,&sb_ip);
        if(status){     /*绑定成功 */
            printf("bind success!\n");
            /*开始下达联网指令 */
            u_char mes[MAX_DATA_SIZE];
            u_int16_t mes_len = 0;
            int status = send_ipgw_flood_command(sb_ip,mes,&mes_len);
            if(status == 0 ){
                printf("successful to connect ipgw!\n");
                char mes[]="successful to connect ipgw!";
                u_int16_t mes_len = strlen(mes);
                u_char data[mes_len+3];
                data[0] = 15;
                memcpy(data+1,&mes_len,2);
                memcpy(data+3,mes,mes_len);
                int n = sendto(sock_udp_fd,data,mes_len+3,0,(sockaddr*)from_client,sizeof(sockaddr_in));
                printf("from client port %d\n",ntohs(from_client->sin_port));
                if(n<0){
                    printf("send error to client failed\n");
                }
            }else if(status == -1 ||status == -2){
                char mes[]="error to connect ipgw";
                u_int16_t mes_len = strlen(mes);
                u_char data[mes_len+3];
                data[0] = 0;
                memcpy(data+1,&mes_len,2);
                memcpy(data+3,mes,mes_len);
                
                int n = sendto(sock_udp_fd,data,mes_len+3,0,(sockaddr*)from_client,sizeof(sockaddr_in));
                if(n<0){
                    printf("send error to client failed\n");
                }
            }
        }else{      /*绑定失败 返回用户失败信息 */
            printf("bind error!\n");
            char mes[]="error to bind user to server B";
            u_int16_t mes_len = strlen(mes);
            u_char data[mes_len+2];
            memcpy(data,&mes_len,2);
            memcpy(data,mes,mes_len);
            int n = sendto(sock_udp_fd,data,mes_len+2,0,(sockaddr*)from_client,sizeof(sockaddr));
            if(n<0){
                printf("send error to client failed\n");
            }
        }
    }
    return NULL;
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
                sockaddr_in from_client;
                int n = recv_udp_unpack_to_ip(sock_udp_fd,data->data,&(data->data_len),&from_client);
		        printf("udp:\n");
             //   print_data(data->data,data->data_len);
                if(n < 0){
                    printf("udp recv() error\n");
                    delete data->data;
                    delete data;
                    continue;
                }else{
                    if(n>9&&*(data->data+9)==0x06){ /*收到的来自client用udp打包的从ipgw来的ip包 应发给对应serverB */
                        printf("udp receved\n");
                        /*修改目的ip 并且重新计算校验和*/
                        u_int32_t *data32 = (u_int32_t*)data->data;

                        /* 
                        if(*(data32+4)==inet_addr(CLIENT_SUBNET_IP_ADDR)){
                            *(data32+4)=inet_addr(SERVER_B_SUBNET_IP_ADDR);
                        }*/
                        /*根据缓存转换IP数据包的目标ip从client内网ip变为serverB的ip */
                        u_int32_t c_subnet_ip = *(data32+4);
                        u_int32_t c_ip = from_client.sin_addr.s_addr;
                        u_int16_t c_udp_port = from_client.sin_port;
                        u_int64_t udp_src_ip_port_subnet_ip;
                        memcpy(&udp_src_ip_port_subnet_ip,&c_ip,4);
                        memcpy(&udp_src_ip_port_subnet_ip+4,&c_udp_port,2);
                        memcpy(&udp_src_ip_port_subnet_ip+6,&c_subnet_ip+2,2);
                        server_b_data* sb = new server_b_data;
                    
                        int status = cache.find_sb_by_client(udp_src_ip_port_subnet_ip,sb);

                        u_char session_key[16];
                        memcpy(session_key,sb->session_key,16);
                        u_int32_t sb_ip = sb->sb_ip;

                        if(status == -1){
                            printf("ip client to server b transform fail!");
                            continue;
                        }else{
                            *(data32+4)=sb_ip;
                            sockaddr_in target_sb_in;
                            target_sb_in.sin_family = AF_INET;
                            target_sb_in.sin_addr.s_addr = sb_ip;
                            bzero(&target_sb_in.sin_zero,8);
                            memcpy(&(data->target),&target_sb_in,sizeof(struct sockaddr)); 
                            /*将数据包结构体的目的地设置为对应的serverB */

                            memcpy(data->client.session_key,sb->session_key,16); /*将客户端信息拷贝进去 */
                            data->client.src_ip = c_ip;
                            data->client.subnet_ip = c_subnet_ip;
                            data->client.src_port = c_udp_port;
                        }
                        
                        
                        /*转换结束 */

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
                        u_char version= *(data->data);
                        version >>= 4;
                        if(version&0x0f == 4){  /*如果是ip封包的udp但非tcp封包 */
                            printf("udp unreceved\n");
                            delete data->data;
                            delete data;
                            continue;
                        }else{          /*否则这是client向serverA发送的联网握手包 开启线程进行验证 */
                            u_int16_t len = 0;
                            memcpy(&len,data->data,2);
                            u_char* user_name = new u_char[len];
                            memcpy(user_name,data->data+2,len);
                            printf("hello from %s\n",user_name);
                            void* args[]={&from_client,&len,user_name};
                            pthread_t indetify;
                            printf("from port %d\n",from_client.sin_port);
                            pthread_create(&indetify,NULL,indetify_thread,args);
                        }
                        
                    }
                }  
            }
            if(FD_ISSET(sock_raw_fd,&read_set)){ /*sock raw有数据 */
                packet *data = new packet;
                data->data = new u_char[MAX_DATA_SIZE];
              /*   socklen_t socklen=sizeof(sockaddr_ll);
		        sockaddr_ll addr_recv;
                 int n = recvfrom(sock_raw_fd,data->data,MAX_DATA_SIZE,0,(sockaddr*)&addr_recv,&socklen);
                if(addr_recv.sll_ifindex!=addr_ll.sll_ifindex){ //过滤掉不属于对应网卡的数据包 
                    delete data->data;
                    delete data;
		            continue;
                }
             */
               // printf("raw:\n");
               // print_data(data->data,n);
                sockaddr_in from;
                socklen_t socklen=sizeof(sockaddr_in);
                int n = recvfrom(sock_ip_fd,data->data,MAX_DATA_SIZE,0,(sockaddr*)&from,&socklen);
                data->data_len = n;
                if(n < 0){
                    printf("raw socket recvfrom() error");
                    delete data->data;
                    delete data;
                }else{
                    data->data_len = n;
                    if(cache.check_sb_using(from.sin_addr.s_addr)&&*(data->data+33)!=0x12){ 
                        /*判断是从serverB来的tcp数据包 并且过滤raw socket发送的包*/
                        printf("sb raw receved\n");
                        u_char temp[MAX_DATA_SIZE];
                        memcpy(temp,data->data+14,data->data_len-14);
                        memcpy(data->data,temp,data->data_len-14);
                        data->data_len-=14;
                        u_char bt = *(data->data);
                        bt = bt<<4;
                        u_int16_t ip_hdr_len = bt/4;
                        u_int32_t *data32 = (u_int32_t*)data->data; 

                        /*
                        if(*(data->data+33)!=0x02){ //不是tcp握手包则伪装src ip发送至ipgw
                            *(data32+3) = inet_addr(CLIENT_NAT_IP_ADDR);
                        }else{                      //是tcp握手包则伪装为客户端内网ip通过udp打包
                            *(data32+3) = inet_addr(CLIENT_SUBNET_IP_ADDR);
                        }
                        */
                       /*此处开始源地址转换 */
                       if(*(data->data+33)!=0x02){ //不是tcp握手包则伪装src ip发送至ipgw
                            u_int32_t sb_ip = *(data32+3);
                            client_data* client = new client_data;
                            cache.find_client_by_sb(sb_ip,client);
                            memcpy(&data->client,client,sizeof(client_data));     //为此数据包设置归属的用户
                            u_char session_key[16];
                            memcpy(session_key,client->session_key,16);
                            *(data32+3) = client->src_ip;
                            sockaddr_in ipgw_in;
                            ipgw_in.sin_family = AF_INET;
                            ipgw_in.sin_addr.s_addr = inet_addr(IPGW_IP_ADDR);
                            ipgw_in.sin_port = htons(IPGW_DEFAULT_PORT);
                            bzero(&ipgw_in.sin_zero,8);
                            memcpy(&data->target,&ipgw_in,sizeof(struct sockaddr_in)); /*设置数据包目的地址为ipgw */
                        }else{                      //是tcp握手包则伪装为客户端内网ip通过udp打包
                            u_int32_t sb_ip = *(data32+3);
                            client_data* client = new client_data;
                            cache.find_client_by_sb(sb_ip,client);
                            u_char session_key[16];
                            memcpy(session_key,client->session_key,16);
                            *(data32+3) = client->subnet_ip;
                            memcpy(&data->client,client,sizeof(client_data));   //为此数据包设置归属的用户
                            sockaddr_in target_client;
                            target_client.sin_port = client->src_port;
                            target_client.sin_addr.s_addr = client->src_ip;
                            target_client.sin_family = AF_INET;
                            bzero(&target_client.sin_zero,8);
                            memcpy(&(data->target),&target_client,sizeof(struct sockaddr_in));//拷贝udp目的地址到数据包结构体
                        }
                        /*转换结束 */
                        u_int32_t src_ip = *(data32+3);
                        u_int32_t dest_ip = *(data32+4);
                        getsum_ip_packet(data->data);
                        getsum_tcp_packet(data->data+ip_hdr_len,data->data_len-ip_hdr_len,src_ip,dest_ip);
                        pthread_mutex_lock(&pthread_mutex);
                        data_queue.push(data);
                        pthread_mutex_unlock(&pthread_mutex);
                    }else{
                        printf("sb raw unreceved\n");
                        delete data->data;
                        delete data; 
                    }
                }  
            }
        }else if(n < 0){
            perror("select error\n");
        }
    }
    return NULL;
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
        if(src_ip == inet_addr(IPGW_IP_ADDR)){ /*来自client打包的源于ipgw的包 应发给client对应的serverB*/
            int n = sendto(sock_ip_fd,data->data,data->data_len,0,&data->target,sizeof(sockaddr_in));
          /*  int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll,SERVER_A_MAC_SUBNET,SERVER_B_MAC_SUBNET);*/
            printf("\nsended syn ack from ipgw\n");
        //    print_data(data->data,data->data_len); 
            if(n<0){
                printf("send ip to serverB to serb error!\n");
            }
        }else if(dest_ip==inet_addr(IPGW_IP_ADDR)){
            if(src_ip==data->client.subnet_ip&&
                    data->client.subnet_ip!=data->client.src_ip){    /*源为client内网ip则打包udp发送到client */
                u_char* session_key = data->client.session_key; //需要作为对称加密秘钥的session
                int n = sendto(sock_udp_fd,data->data,data->data_len,0,(sockaddr*)&(data->target),sizeof(sockaddr));
		        printf("sended udp\n");
                if(n<0){
                    perror("send udp to client error\n");
                }
            }else if(src_ip==data->client.src_ip){          /*否则发到网关 让网关处理 也就是伪装ip直接发送ipgw */
                printf("\nsending ip to ipgw\n");
		        /* int err = send_ip_ll(sock_raw_fd,data->data,data->data_len,addr_ll_main,SERVER_A_MAC_OUT,GATEWAY_MAC);*/
                int n = sendto(sock_ip_fd,data->data,data->data_len,0,&data->target,sizeof(sockaddr_in));
		        if(n<0){
                    printf("send ip to ipgw error!\n");
                }
            }
        }
        delete data->data;
        delete data;
    }
    return NULL;
}

void* print_thread(void*){
    while(true){
        usleep(500000);
        pthread_mutex_lock(&pthread_mutex);
//        printf("queue size: %d\n",data_queue.size());
        pthread_mutex_unlock(&pthread_mutex);
    }
    return NULL;
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
    return NULL;
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
    /*
    u_int16_t cport = htons(1029);
    u_int32_t c_ip = inet_addr("58.154.192.112");
    u_int32_t subnet_ip = inet_addr("192.168.1.123");
    u_int32_t sb_ip = 0;
    char session[16] = "123123123";
    if(cache.bind(cport,c_ip,subnet_ip,(u_char*)session,&sb_ip)<0){
        printf("cache bind error");
        exit(0);
    }
    //client_data cli;
    cli.src_ip = c_ip;
    cli.src_port = cport;
    cli.subnet_ip = subnet_ip;
    memcpy(cli.session_key,session,16);
    server_b_data sb;
    u_int64_t client_ip_port_subnet_ip;
    memcpy(((u_char*)&client_ip_port_subnet_ip),&c_ip,4);
    memcpy(((u_char*)&client_ip_port_subnet_ip)+4,&cport,2);
    memcpy(((u_char*)&client_ip_port_subnet_ip)+6,((u_char*)&subnet_ip)+2,2);
    printf("start\n");
    print_data((u_char*)&client_ip_port_subnet_ip,8);
    if(cache.find_sb_by_client(client_ip_port_subnet_ip,&sb)<0){
        printf("find no result\n");
    }else{
        in_addr in;
        in.s_addr = sb.sb_ip;
        printf("%s\n",inet_ntoa(in));
    }*/
}
