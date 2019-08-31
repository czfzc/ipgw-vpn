#ifndef IPGW_FLOODER_FLOOD_H
#define IPGW_FLOODER_FLOOD_H

#include <array>
#include <vector>
#include "req_res.h"

using namespace std;

struct alter_result {
    int try_times;
    int finally_login_success_index;
    vector<int> login_history;
};

alter_result alternate_ipgw(int index, array<string, 171> &list, int ignore, bool until_connected, bool show_status);

void single_user_flood(const string &usernumber, int times, bool show_status);

void multi_user_flood(int from_index, int times, array<string, 171> &list, bool show_status);

#endif //IPGW_FLOODER_FLOOD_H
