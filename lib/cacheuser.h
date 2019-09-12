#ifndef _HCACHEUSER_H_
#define _HCACHEUSER_H

#include<map>
#include<stack>
#include<set>
#include<sys/types.h>
#include<iostream>
#include<pthread.h>
#include"nettools.h"
#include"ipgw.h"
#pragma pack(1)
using namespace std;

class cacheuser{
    public:
        cacheuser(){
            this->init();
            pthread_mutex_init(&bind_mutex,NULL);
            pthread_mutex_init(&un_bind_mutex,NULL);
        }
        /*绑定client和serverB */
        bool bind(u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,const u_char* session_key,u_int32_t* sb_ip);
        /*解绑定client和serverB */
        bool un_bind(u_int32_t sb_ip,u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,const u_char* session_key);
        /*根据client查找serverB */
        int find_sb_by_client(const u_int64_t client,server_b_data* sb);
        /*根据serverB查找client */
        int find_client_by_sb(const u_int32_t sb_ip,client_data* client);
        /*查看serverB是否在使用 */
        bool check_sb_using(u_int32_t sb_ip);

    private:
        /*初始化缓存数据库 */
        int init();
        /*serverB主机到client转换哈希表 */
        map<u_int32_t,client_data*> sb_to_client;
        /*client到serverB主机转换哈希表 */
        map<u_int64_t,server_b_data*> client_to_sb;
        /*可用的serverB */
        stack<u_int32_t> sb_stk; 
        /*已用的serverB */
        set<u_int32_t> sb_using;
        /*互斥锁 */
        pthread_mutex_t bind_mutex,un_bind_mutex;

};

int cacheuser::init(){
    sb_stk.push(inet_addr("172.17.0.2"));
    return 0;
}

bool cacheuser::bind(u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,const u_char* session_key,u_int32_t* sb_ip_t){
    *sb_ip_t = sb_stk.top();
    sb_stk.pop();

    client_data *client = new client_data;
    client->src_port = c_uport;
    client->src_ip = c_ip;
    client->subnet_ip = c_subnet_ip;
    memcpy(client->session_key,session_key,16);
    sb_to_client.insert(pair<u_int32_t,client_data*>(*sb_ip_t,client));

    server_b_data *sb = new server_b_data;
    sb->sb_ip = *sb_ip_t;
    memcpy(sb->session_key,session_key,16);
    u_int64_t client_ip_port_subnet_ip;
    memcpy(((u_char*)&client_ip_port_subnet_ip),&c_ip,4);
    memcpy(((u_char*)&client_ip_port_subnet_ip)+4,&c_uport,2);
    memcpy(((u_char*)&client_ip_port_subnet_ip)+6,((u_char*)&c_subnet_ip)+2,2);
    print_data((u_char*)&client_ip_port_subnet_ip,8);
    client_to_sb.insert(pair<u_int64_t,server_b_data*>(client_ip_port_subnet_ip,sb));
    return true;
}

bool cacheuser::un_bind(u_int32_t sb_ip,u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,const u_char* session_key){
    /*暂未实现 */
    return true;
}

int cacheuser::find_client_by_sb(const u_int32_t sb_ip,client_data* client){
    if(client==NULL)
        return -1;
    map<u_int32_t,client_data*>::iterator sb_ite = sb_to_client.find(sb_ip);
    if(sb_ite == sb_to_client.end())
        return -1;
    else{
        memcpy(client,sb_ite->second,sizeof(client_data));
        return 0;
    }
}

int cacheuser::find_sb_by_client(const u_int64_t client,server_b_data* sb){
    if(sb==NULL) return -1;
    map<u_int64_t,server_b_data*>::iterator client_ite = client_to_sb.find(client);
    if(client_ite == client_to_sb.end())
        return -1;
    else{
        memcpy(sb,client_ite->second,sizeof(server_b_data));
        return 0;
    }
}

bool cacheuser::check_sb_using(u_int32_t sb_ip){
    return this->sb_using.count(sb_ip)==1;
}

#endif