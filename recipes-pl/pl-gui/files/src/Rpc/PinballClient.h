#ifndef RPC_PINBALLCLIENT_H
#define RPC_PINBALLCLIENT_H

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

class EventLoop;

namespace Gui {
class Renderer;
}

namespace Rpc {
/**
 * @brief Types of pinballd broadcast messages
 *
 * Members may be ORed together for some functions.
 */
enum PinballBroadcastType: uintptr_t {
    None                                = 0,
    TouchEvent                          = (1 << 0),
    ButtonEvent                         = (1 << 1),
    EncoderEvent                        = (1 << 2),
};

/**
 * @brief Interface to pinballd (user interface hardware io deamon)
 */
class PinballClient {
    public:
        /// Available indicators on the front panel
        enum class Indicator {
            /// RGB status LED
            Status,
            /// Dual color trigger indicator
            Trigger,
            /// Single color overheat indicator
            Overheat,
            /// Single color overcurrent indicator
            Overcurrent,
            /// Single color error indicator
            Error,

            /// Single color mode button (CC)
            BtnModeCc,
            /// Single color mode button (CV)
            BtnModeCv,
            /// Single color mode button (CW)
            BtnModeCw,
            /// Single color mode button (bonus)
            BtnModeExt,
            /// Dual color "Load on" button
            BtnLoadOn,
            /// Menu button
            BtnMenu,
        };
        using IndicatorColor = std::tuple<double, double, double>;
        using IndicatorValue = std::variant<bool, double, IndicatorColor>;
        using IndicatorChange = std::pair<Indicator, IndicatorValue>;

    public:
        PinballClient(const std::shared_ptr<EventLoop> &ev,
                const std::filesystem::path &socketPath);
        ~PinballClient();

        /**
         * @brief Enable the secretion of UI events
         */
        inline void enableUiEvents(const std::shared_ptr<Gui::Renderer> &gui) {
            this->gui = gui;
            this->setDesiredBroadcasts(kUiBroadcastMask);
        }
        /**
         * @brief Disable UI event secretion
         */
        inline void disableUiEvents() {
            this->setDesiredBroadcasts(PinballBroadcastType::None);
            this->gui.reset();
        }

        void setDesiredBroadcasts(const PinballBroadcastType mask);

        /**
         * @brief Update the state of a single indicator
         *
         * This is a shortcut for the bulk indicator update method. You should prefer to use the
         * bulk updates if setting more than one indicator.
         */
        inline void setIndicatorState(const IndicatorChange &change) {
            std::array<IndicatorChange, 1> temp{{
                change
            }};
            return this->setIndicatorState(temp);
        }
        void setIndicatorState(std::span<const IndicatorChange> changes);

    private:
        int connectSocket();

        void bevRead(struct bufferevent *);
        void bevEvent(struct bufferevent *, const uintptr_t);

        void sendRaw(std::span<const std::byte> payload);
        uint8_t sendPacket(const uint8_t endpoint, std::span<const std::byte> payload);

        void processUiEvent(std::span<const std::byte>);
        void processUiTouchEvent(const struct cbor_item_t *);

        /// Emit a touch up event
        inline void emitTouchUp() {
            const auto [x, y] = this->lastTouchPos;
            this->emitTouchEvent(x, y, false);
        }
        /// Emit a touch down/movement event
        inline void emitTouchDown(const int16_t x, const int16_t y) {
            this->lastTouchPos = {x, y};
            this->emitTouchEvent(x, y, true);
        }
        void emitTouchEvent(const int16_t x, const int16_t y, const bool isDown);

    private:
        /// Should events be logged to the console?
        constexpr static const bool kLogEvents{false};

        /// Mask of all UI broadcast types we want
        constexpr static const PinballBroadcastType kUiBroadcastMask{
            PinballBroadcastType::TouchEvent | PinballBroadcastType::ButtonEvent |
                PinballBroadcastType::EncoderEvent
        };
        /// Value for the next outgoing packet tag
        uint8_t nextTag{0};

        /// The event loop that owns us
        std::weak_ptr<EventLoop> ev;
        /// GUI renderer process to receive events
        std::weak_ptr<Gui::Renderer> gui;

        /// Position of the last touch event
        std::pair<int16_t, int16_t> lastTouchPos{0, 0};

        /// Path of the RPC socket
        std::filesystem::path socketPath;
        /// File descriptor for the RPC socket
        int fd{-1};
        /// Buffer event wrapping the loadd socket
        struct bufferevent *bev{nullptr};

        /// Packet receive buffer
        std::vector<std::byte> rxBuf;
};
}

#endif
