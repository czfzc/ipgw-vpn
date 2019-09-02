#include "parse_option.h"
#include <cstdlib>

using namespace cxxopts;

Options build_options() {
    Options options("ipgw_flooder", "A powerful tool which connects to ipgw network via a flood leak.");
    options.add_options()
            ("s,show", "Show requests process, not shown by default.")
            ("m,flood-mode", "Flood mode includes single-user(su)/user-range(ur)",
             value<std::string>()->default_value("su"))
            ("f,flood-index",
             "Flood index. If flood mode is su, only index. If flood mode is ur, flood index++ by times. Default -1 means by random.",
             value<int>()->default_value("-1"))
            ("t,flood-times", "Flood times. After these times of flood, try to login.",
             value<int>()->default_value(DEFAULT_FLOOD_TIMES))
            ("g,ignore-index", "Won't login with specific index.", value<int>()->default_value("-1"))
            ("l,login-index",
             "Force login with specific index. Default stop at when first attempt success. When force login failed, programme will return an error.",
             value<int>()->default_value("-1"))
            ("v,version", "show programme version")
            ("h,help", "Show this help page.");
    options.custom_help("[options (and args)]");
    return options;
}

using namespace std;

user_opts parse(int argc, char **argv) {
    Options options = build_options();
//    cout << options.help();
    auto args = options.parse(argc, argv);
    user_opts userOpts{args["f"].as<int>(),
                       args["t"].as<int>(),
                       args["g"].as<int>(),
                       args["l"].as<int>(),
                       (args["m"].as<string>() == "su"),
                       (bool) args["s"].count(),
                       (bool) args["h"].count(),
                       (bool) args["v"].count()};
    return userOpts;
}

void print_help() {
    cout << build_options().help();
}

void explain(user_opts &userOpts) {// examine and explain user's exception.
    if (userOpts.help) {
        print_help();
        exit(0);
    }
    if (userOpts.version) {
        cout << "NEU ipgw-flooder V3.0 Official Test RAILGUN release\n";
        exit(0);
    }
    if (not userOpts.single_user_mode && userOpts.flood_index + userOpts.times > 170) {
        fprintf(stderr, "index add time out of range\n");
        exit(-1);
    }
    srand(time(nullptr));
    if (userOpts.flood_index == -1) {
        if (not userOpts.single_user_mode) {
            userOpts.flood_index = rand() * (long) (172 - userOpts.times) / RAND_MAX;
        } else {
            userOpts.flood_index = rand() * (long) 171 / RAND_MAX;
        }
    }
    if (userOpts.login_index == -1){
        userOpts.login_index = userOpts.flood_index; // I decide to make login index equals to flood index.
    }
}