// Hardcore file. This file use easy_socket module to write RAW http requests.
#include "http_socket_req.h"
#include "easy_socket/Socket.h"

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

void headonly_post(const string &url, const string &data) {
    Socket *sock = just_post(url, data);
    vector<Socket> reads(1);// new Vector reads contains type Socket length 1
    reads[0] = *sock;
    Socket::select(&reads, nullptr, nullptr, MAX_TIMEOUT_SECONDS);
    sock->close();
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
//Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/75.0.3770.142 Safari/537.36
//"POST /srun_portal_pc.php?ac_id=1& HTTP/1.1\r\naccept: */*\r\ncontent-length: 98\r\n\r\naction=login&ac_id=1&user_ip=&nas_ip=&user_mac=&URL=&username=20183602&password=20183602&save_me=0"