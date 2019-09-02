#ifndef IPGW_FLOODER_PARSE_OPTION_H
#define IPGW_FLOODER_PARSE_OPTION_H

#include "cxxopts.h" // The library helps me parse arguments. So nice!
#define DEFAULT_FLOOD_TIMES "50"
struct user_opts {
    int flood_index;// flood index
    int times;// flood times
    int ignore_index;// ignore index
    int login_index;
    bool single_user_mode;
    bool show_process;
    bool help;
    bool version;
};

user_opts parse(int argc, char **argv);

void print_help();

void explain(user_opts &userOpts);

#endif //IPGW_FLOODER_PARSE_OPTION_H
