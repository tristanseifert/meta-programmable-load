/**
 * @file
 *
 * @brief Boot splash remote control command line utility
 *
 * This allows setting the boot progress, version strings, and progress string. They are
 * communicated to the splash via the RPC interface. Use it as an example on how to use the remote
 * control library, which should be embedded in your own application for more granular control.
 */
#include <remote.h>

#include <cstdlib>
#include <iostream>
#include <getopt.h>

/**
 * @brief Entry point
 *
 * Entry point of the utility. One or more of the following flags should be specified:
 *
 * --progress: Integer boot progress, between 0 and 100.
 * --message: Boot progress string
 * --version: An integer, comma, then the version string for one of the version slots.
 */
int main(const int argc, char * const *argv) {
    int err;

    // open connection
    err = splash_connect();
    if(err) {
        std::cerr << "failed to connect: " << err << std::endl;
        return -1;
    }

    // parse options
    int c;

    while(1) {
        int index{0};

        const static struct option options[] = {
            {"progress",        required_argument, 0, 0},
            {"message",         required_argument, 0, 0},
            {"version",         required_argument, 0, 0},
            {"quit",            no_argument, 0, 0},
            {nullptr,           0, 0, 0},
        };

        c = getopt_long(argc, argv, "", options, &index);
        if(c == -1) {
            break;
        }

        // long options (based on index)
        if(c == 0) {
            // update progress percentage
            if(index == 0) {
                const auto progress = strtoul(optarg, nullptr, 10);
                if(progress > 100) {
                    std::cerr << "invalid progress value (must be [0, 100]): " << progress
                        << std::endl;
                    return -1;
                }

                err = splash_update_progress(static_cast<double>(progress) / 100.);
            }
            // set message
            else if(index == 1) {
                err = splash_update_message(optarg);
            }
            // request termination of splash daemon
            else if(index == 3) {
                err = splash_request_exit();
            }
            // TODO: others
            else {
                printf("option '%s', value '%s'\n", options[index].name, optarg ? optarg : "(null)");
            }

            // handle error from command
            if(err) {
                // try the next argument, maybe that won't fail
                std::cerr << "failed to handle command '" << options[index].name << "': "
                    << err << std::endl;
            }
        }
        // non-long options
        else {
            switch(c) {
                case '?':
                    break;

                default:
                    std::cerr << "Unexpected return from getopt_long: " << std::hex << c
                        << std::endl;
                    break;
            }
        }
    }

    // clean up
    splash_disconnect();
    return 0;
}
