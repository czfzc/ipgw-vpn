#ifndef _HCACHEUSER_H_
#define _HCACHEUSER_H

#include<map>
#include<stack>
#include<sys/types.h>
#include<iostream>
#include<pthread.h>
#include"nettools.h"
using namespace std;

class cacheuser{
    public:
        cacheuser(){
            this->init();
            pthread_mutex_init(&bind_mutex,NULL);
            pthread_mutex_init(&un_bind_mutex,NULL);
        }
        /*绑定client和serverB */
        bool bind(u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,u_char* session_key,u_int32_t* sb_ip);
        /*解绑定client和serverB */
        bool un_bind(u_int32_t sb_ip,u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,u_char* session_key);
        /*根据client查找serverB */
        int find_sb_by_client(u_int64_t client_udp_src_ip_and_port,u_char* udp_src_ip_and_port_and_subnet_ip_and_session);
        /*根据serverB查找client */
        int find_client_by_sb(u_int32_t sb_ip,u_char* sb_ip_and_session);

    private:
        /*初始化缓存数据库 */
        int init();
        /*serverB主机到client转换哈希表 */
        map<u_int32_t,u_char*> sb_to_client;
        /*client到serverB主机转换哈希表 */
        map<u_int64_t,u_char*> client_to_sb;
        /*可用的serverB */
        stack<u_int32_t> sb_stk; 
        /*互斥锁 */
        pthread_mutex_t bind_mutex,un_bind_mutex;

};

int cacheuser::init(){
    sb_stk.push(inet_addr("172.17.0.2"));
}

bool cacheuser::bind(u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,u_char* session_key,u_int32_t* sb_ip_t){
    pthread_mutex_lock(&bind_mutex);
    u_char *udp_src_ip_and_port_and_subnet_and_session = new u_char[26];
    memcpy(udp_src_ip_and_port_and_subnet_and_session,&c_ip,4);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+4,&c_uport,2);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+6,&c_subnet_ip,4);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+10,session_key,16);
    u_int32_t sb_ip = this->sb_stk.top();
    this->sb_stk.pop();
    sb_to_client.insert(pair<u_int32_t,u_char*>(sb_ip,udp_src_ip_and_port_and_subnet_and_session));
    u_char *sb_ip_and_session = new u_char[20];
    u_int64_t udp_src_ip_and_subnet = 0;
    memcpy(&udp_src_ip_and_subnet,&c_ip,4);
    memcpy(&udp_src_ip_and_subnet+4,&c_subnet_ip,4);
    memcpy(sb_ip_and_session,&sb_ip,4);
    memcpy(sb_ip_and_session+4,session_key,16);
    *sb_ip_t = sb_ip;
    client_to_sb.insert(pair<u_int64_t,u_char*>(udp_src_ip_and_subnet,sb_ip_and_session));
    pthread_mutex_unlock(&bind_mutex);
    return true;
}

bool cacheuser::un_bind(u_int32_t sb_ip,u_int16_t c_uport,u_int32_t c_ip,u_int32_t c_subnet_ip,u_char* session_key){
    pthread_mutex_lock(&un_bind_mutex);
    u_char *udp_src_ip_and_port_and_subnet_and_session = new u_char[26];
    memcpy(udp_src_ip_and_port_and_subnet_and_session,&c_ip,4);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+4,&c_uport,2);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+6,&c_subnet_ip,4);
    memcpy(udp_src_ip_and_port_and_subnet_and_session+10,session_key,16);
    map<u_int32_t,u_char*>::iterator sb_to_client_ite = sb_to_client.find(sb_ip);
    if(sb_to_client_ite == sb_to_client.end()){
        cout<<"invalid sb_ip"<<endl;
        return false;
    }else{
        if(memcmp(sb_to_client_ite->second,udp_src_ip_and_port_and_subnet_and_session,22)==0)
            sb_to_client.erase(sb_to_client_ite);
        else{
            cout<<"invalid client"<<endl;
            return false;
        }
    }
    u_char *sb_ip_and_session = new u_char[24];
    u_int64_t udp_src_and_port_and_subnet = 0;
    memcpy(&udp_src_and_port_and_subnet,&c_ip,4);
    memcpy(&udp_src_and_port_and_subnet+4,&c_uport,2);
    memcpy(sb_ip_and_session,&sb_ip,4);
    memcpy(sb_ip_and_session+4,session_key,16);
    memcpy(sb_ip_and_session+20,&c_subnet_ip,24);
    map<u_int64_t,u_char*>::iterator client_to_sb_ite = client_to_sb.find(udp_src_and_port_and_subnet);
    if(client_to_sb_ite == client_to_sb.end()){
        cout<<"invalid client"<<endl;
        return false;
    }else{
        if(memcmp(client_to_sb_ite->second,sb_ip_and_session,20)==0)
            client_to_sb.erase(client_to_sb_ite);
        else{
            cout<<"invalid sb_ip"<<endl;
            return false;
        }
    }
    pthread_mutex_unlock(&un_bind_mutex);
    return true;
}

int cacheuser::find_client_by_sb(u_int32_t sb_ip,u_char* client_udp_src_ip_and_port_and_subnet_ip_and_session){
    if(client_udp_src_ip_and_port_and_subnet_ip_and_session==NULL) return -1;
    map<u_int32_t,u_char*>::iterator sb_ite = sb_to_client.find(sb_ip);
    if(sb_ite == sb_to_client.end())
        return -1;
    else{
        memcpy(client_udp_src_ip_and_port_and_subnet_ip_and_session,sb_ite->second,26);
        return 0;
    }
}

int cacheuser::find_sb_by_client(u_int64_t client_udp_src_ip_and_subnet_ip,u_char* sb_ip_and_session){
    if(sb_ip_and_session==NULL) return -1;
    map<u_int64_t,u_char*>::iterator client_ite = client_to_sb.find(client_udp_src_ip_and_subnet_ip);
    if(client_ite == client_to_sb.end())
        return -1;
    else{
        memcpy(sb_ip_and_session,client_ite->second,20);
        return 0;
    }
}

#endif