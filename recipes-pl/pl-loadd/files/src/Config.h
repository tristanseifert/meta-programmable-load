#ifndef CONFIG_H
#define CONFIG_H

#include <string>

/**
 * @brief Server configuration
 */
class Config {
    public:
        /**
         * @brief Get the file path for the RPC listening socket
         *
         * @todo Actually read this from a configuration
         */
        static const std::string &GetRpcSocketPath() {
            return gSocketPath;
        }

    private:
        static std::string gSocketPath;
};

#endif
