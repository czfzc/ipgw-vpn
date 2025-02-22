#ifndef _HCHECKSUM_H_
#define _HCHECKSUM_H_

#include<sys/types.h>
#include<netinet/in.h>
#include<string.h>
#include<stdio.h>
#include"nettools.h"

u_int16_t checksum(u_int16_t * data, int len){
    long sum = 0;
/*
计算所有数据的16bit对之和
*/
    while( len > 1  )  {
        /*  This is the inner loop */
        sum += *(u_int16_t*)data++;

        len -= 2;
    } 

/* 如果数据长度为奇数，在该字节之后补一个字节(0),
   然后将其转换为16bit整数，加到上面计算的校验和
　　中。
 */
    if( len > 0 ) { 
        char left_over[2] = {0};
        left_over[0] = *data;
        sum += * (u_int16_t*) left_over;
    }   

/*  将32bit数据压缩成16bit数据，即将进位加大校验和
　　的低字节上，直到没有进位为止。
 */
    while (sum>>16)
        sum = (sum & 0xffff) + (sum >> 16);
/*返回校验和的反码*/
   return ~sum;
}

/*************************************
 * 
 * IP包求校验和
 * 
 *************************************/

void getsum_ip_packet(unsigned char *buffer)
{
    u_char bt = *(buffer);
    bt = bt<<4;
    u_int16_t ip_hdr_len = bt/4;
    u_int16_t *data16 = (u_int16_t*)buffer;
    *(data16+5)=0;
    u_int16_t sum = checksum(data16,ip_hdr_len);  
    *(data16+5)=sum;
}

/*************************************
 * 
 * TCP包求校验和
 * 
 *************************************/

void getsum_tcp_packet(u_char *buffer,u_int16_t data_len,u_int32_t src_ip,u_int32_t dest_ip)
{
    u_int16_t *buf16=(u_int16_t*)buffer;
    *(buf16+8)=0x0000; /*原校验和置0 */ 
    
    src_ip=src_ip;
    dest_ip=dest_ip;
    u_char fake_header[12];
    memcpy(fake_header,&src_ip,4);
    memcpy(fake_header+4,&dest_ip,4);
    bzero(fake_header+8,1);
    fake_header[9]=0x06;
    u_int16_t len = htons(data_len);
    memcpy(fake_header+10,&len,2);

    u_char data[MAX_DATA_SIZE]={0};
    memcpy(data,fake_header,12);
    memcpy(data+12,buffer,data_len);
    u_int16_t *data16 = (u_int16_t*)data;
    printf("checksum:\n");
    print_data(data,data_len+12);
    printf("lenlen:%d\n",data_len);
    *(buf16+8)=checksum(data16,data_len+12);
}

#endif