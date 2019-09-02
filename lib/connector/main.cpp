#include <iostream>
#include "usernumberlist.hpp"
#include "flood.h"
#include "parse_option.h"

int main(int argc, char *argv[]) {
    user_opts userOpts = parse(argc, argv);
    bool login_index_is_set = (userOpts.login_index != -1);
    explain(userOpts);
    /* set flood_index and login_index.
    But, if login_index is not set, the programme will generate and tell temp login function login until connected
    however, if login_index is set, temp login function will only try until not union authentic. So there must be a bool on
     the top of the code to show whether the login index is -1 or other value. The explain function will set login index same with
     flood index if it is blank -1.*/
    if (userOpts.single_user_mode) {
        single_user_flood(list.at(static_cast<unsigned long>(userOpts.flood_index)), userOpts.times,
                          userOpts.show_process);
    } else {
        multi_user_flood(userOpts.flood_index, userOpts.times, list, userOpts.show_process);
    }
    // end of flood. If not specific login index, when flood is over, programme will login from random index.
    //alternate means one-by-one.
    alter_result alterResult = alternate_ipgw(userOpts.login_index, list, userOpts.ignore_index,
                                              (userOpts.login_index != -1),
                                              userOpts.show_process);
    if (login_index_is_set) {
        // Call the lower level function directly.
        temp_login_result result = responsible_ipgw(list.at(static_cast<unsigned long>(userOpts.login_index)), list.at(
                static_cast<unsigned long>(userOpts.login_index)), true,
                                                    true);
        if (result == NETWORK_CONNECTED) {
            cout << list.at(static_cast<unsigned long>(userOpts.login_index)) << " login successful." << endl;
        } else {
            cout << "login " << list.at(static_cast<unsigned long>(userOpts.login_index)) << " error, result code "
                 << result << endl;
        }
    } else {
        cout << "login " << list.at(static_cast<unsigned long>(alterResult.finally_login_success_index))
             << " successful. try times " << alterResult.try_times
             << endl;
        if (userOpts.show_process) {
            for (int i:alterResult.login_history) {
                cout << i;
            }
            cout << endl;
        }
    }
}