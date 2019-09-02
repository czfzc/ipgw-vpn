#ifndef IPGW_FLOODER_REQ_RES_H
#define IPGW_FLOODER_REQ_RES_H

#include "http_socket_req.h"

#define NOT_UNION_AUTH (-2)
#define NETWORK_NOT_CONNECTED (-1)
#define NETWORK_CONNECTED 0
#define FORCE_UNION_AUTH 1
#define WAIT_FIVE_MINITES 2
#define PASSWORD_ERROR 3
#define UNKNOWN_RESPONSE 4

#define IPGW_POST_ADDRESS "ipgw.neu.edu.cn"
//#define IPGW_POST_ADDRESS "localhost"
typedef short temp_login_result;

temp_login_result responsible_ipgw(const string &usernumber, const string &password, bool check_connected, bool detailed_response);

void headonly_ipgw(const string &usernumber);

#endif //IPGW_FLOODER_REQ_RES_H
