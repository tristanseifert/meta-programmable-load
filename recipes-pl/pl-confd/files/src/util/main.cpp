#include <getopt.h>

#include <iostream>
#include <string>

#include <confd.h>

/**
 * @brief Utility entry point
 *
 * The program is driven on the command line by the following switches:
 *
 * - socket: Specify the path to the UNIX domain socket used to communicate with confd
 * - read: Name of a key to read
 * - write: Name of a key to write (TODO: implement)
 *
 * Note that you must always specify one of --read or --write.
 */
int main(const int argc, char * const *argv) {
    int err;
    std::string socketPath{"/var/log/confd.sock"};
    std::string keyName;
    bool read{false};

    // parse command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // path to the socket
            {"socket",                  required_argument, 0, 0},
            // name of key to read
            {"read",                    required_argument, 0, 0},
            // TODO: add value type flag
            {nullptr,                   0, 0, 0},
        };

        c = getopt_long(argc, argv, "", options, &index);

        // end of options
        if(c == -1) {
            break;
        }
        // long option (based on index)
        else if(!c) {
            if(index == 0) {
                socketPath = optarg;
            }
            else if(index == 1) {
                keyName = optarg;
                read = true;
            }
        }
    }

    if(keyName.empty()) {
        std::cerr << "key name is required" << std::endl;
        return -1;
    }

    // establish connection
    err = confd_open(socketPath.c_str());
    if(err) {
        std::cerr << "failed to connect to confd: " << err << std::endl;
        return -1;
    }

    // perform request
    if(read) {
        char outStr[512];
        err = confd_get_string(keyName.c_str(), outStr, sizeof(outStr));

        if(err) {
            std::cerr << "failed to read key: " << err << std::endl;
            return -1;
        }
    }

    // clean up
    confd_close();
}
