#ifndef _H_REQUEST_IPGW_H_
#define _H_REQUEST_IPGW_H_

#include <iostream>
#include "usernumberlist.hpp"
#include "flood.h"
#include "parse_option.h"

#define DEFAULT_FLOOD_TIMES 50

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