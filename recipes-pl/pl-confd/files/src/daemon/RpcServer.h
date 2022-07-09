#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <sys/signal.h>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>

class DataStore;

/**
 * @brief Remote access interface
 *
 * This class opens the listening socket for the RPC interface and handles requests there. It
 * hides the management and interfacing with clients behind a single run method, which takes
 * advantage of system facilities to wait on multiple file descriptors at once.
 */
class RpcServer {
    public:
        /**
         * @brief Initialize RPC server
         */
        RpcServer(const std::shared_ptr<DataStore> &store) : store(store) {
            this->initSocket();
            this->initEventLoop();
        }

        ~RpcServer();

        void run();

    private:
        /**
         * @brief Information for a single connected client
         *
         * This struct encapsulates all information about a connected client, including the events
         * used to wait for activity on the connection.
         */
        struct Client {
            /// Underlying client file descriptor
            int socket{-1};
            /// Socket buffer event (used for data ready to read + events)
            struct bufferevent *event{nullptr};
            /// message receive buffer
            std::vector<std::byte> receiveBuf;
            /// message transmit buffer
            std::vector<std::byte> transmitBuf;

            Client(RpcServer *, const int);
            ~Client();

            void replyTo(const struct rpc_header &, std::span<const std::byte>);
            void send(std::span<const std::byte>);
        };

        /**
         * @brief Flags for a request
         *
         * These flags may be combined by bitwise-OR, and are passed to the various processing
         * functions to alter their behavior.
         */
        enum Flags: uintptr_t {
            None                                = 0,
            /**
             * @brief Output floating point values with 32-bit precision
             *
             * When set, all floating point values (internally stored as doubles) will be converted
             * to 32-bit floating point (float) types on output.
             */
            SinglePrecisionFloat                = (1 << 0),

            /**
             * @brief Format message as a response to a "set" request
             *
             * This renames the `found` field to `updated` within the body of a key response, and
             * does not send the key value again.
             */
            IsSetRequest                        = (1 << 1),

            /**
             * @brief Exclude the property key value from response
             */
            ExcludeValue                        = (1 << 2),
        };

    private:
        void initSocket();

        void initEventLoop();
        void initWatchdogEvent();
        void initSignalEvents();
        void initSocketEvent();

        void acceptClient();
        void handleClientRead(struct bufferevent *);
        void handleClientEvent(struct bufferevent *, const size_t);
        void abortClient(struct bufferevent *);

        void handleTermination();

        void doCfgQuery(std::span<const std::byte>, struct cbor_item_t *,
                const std::shared_ptr<Client> &);
        void getCfgQueryFlags(struct cbor_item_t *, Flags &);
        void sendKeyValue(const struct rpc_header *, const std::shared_ptr<Client> &,
                const std::string &, const PropertyValue &, const Flags = Flags::None);

        void doCfgUpdate(std::span<const std::byte>, struct cbor_item_t *,
                std::shared_ptr<Client> &);

        static std::string ExtractKeyName(struct cbor_item_t *);

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};

        /// Main RPC listening socket
        int listenSock{-1};
        /// event for listening socket receiving a client
        struct event *listenEvent{nullptr};

        /// signals to intercept
        constexpr static const std::array<int, 3> kEvents{{SIGINT, SIGTERM, SIGHUP}};
        /// termination signal events
        std::array<struct event *, 3> signalEvents;

        /// watchdog kicking timer event (if watchdog is active)
        struct event *watchdogEvent{nullptr};

        /// libevent main loop
        struct event_base *evbase{nullptr};

        /// connected clients
        std::unordered_map<struct bufferevent *, std::shared_ptr<Client>> clients;

        /// configuration data storage
        std::shared_ptr<DataStore> store;
};

#endif
