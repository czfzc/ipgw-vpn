#ifndef IPGW_FLOODER_HTTP_SOCKET_REQ_H
#define IPGW_FLOODER_HTTP_SOCKET_REQ_H

#include <string>
#define MAX_TIMEOUT_SECONDS 10
#define MAX_READ_LENGTH 10000
using namespace std;

string responsible_post(const string &url, const string &data);

void headonly_post(const string &url, const string &data);

#endif //IPGW_FLOODER_HTTP_SOCKET_REQ_H
