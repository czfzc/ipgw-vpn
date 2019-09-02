#include "req_res.h"

using namespace std;

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

void headonly_ipgw(const string &usernumber) {
    string post_data = string("action=login&ac_id=1&user_ip=&nas_ip=&user_mac=&URL=&username=").append(
            usernumber).append("&password=").append(usernumber + "&save_me=0");
    headonly_post(IPGW_POST_ADDRESS, post_data);
}