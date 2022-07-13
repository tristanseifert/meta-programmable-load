#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <span>
#include <string>
#include <string_view>

class EventLoop;

/**
 * @brief DRM framebuffer driver
 *
 * This class exposes a double buffered framebuffer, with draw callbacks on VBlank, with a rather
 * convenient interface.
 */
class Framebuffer {
    public:
        Framebuffer(const std::shared_ptr<EventLoop> &ev, const std::string_view path);
        ~Framebuffer();

        /**
         * @brief Get the stride of a framebuffer
         *
         * The stride is the number of bytes per line, also known as pitch.
         *
         * @return Framebuffer stride, in bytes
         */
        inline const size_t getStride(const size_t idx) {
            return this->kmsBuffers.at(idx).stride;
        }

        /**
         * @brief Get the memory of a framebuffer
         *
         * @param idx Framebuffer index ([0, 1])
         *
         * @return Range of virtual memory space containing this framebuffer's data
         */
        inline std::span<std::byte> getData(const size_t idx) {
            auto &buf = this->kmsBuffers.at(idx);

            return {reinterpret_cast<std::byte *>(buf.fb),
                buf.stride * static_cast<size_t>(buf.pixelSize.second)};
        }

        /**
         * @brief Get the pixel dimensions of a framebuffer
         *
         * @return A pair containing (width, height) of the framebuffer object
         */
        inline auto getSize(const size_t idx) {
            return this->kmsBuffers.at(idx).pixelSize;
        }

    private:
        /**
         * @brief Framebuffer wrapper
         *
         * This is a thin wrapper around a kms_bo and drm framebuffer resources.
         */
        struct Buffer {
            /// Kernel buffer object
            struct kms_bo *bo{nullptr};
            /// Handle for this BO
            unsigned int handle{0};

            /// Address in memory the buffer's draw area is mapped
            void *fb{nullptr};
            /// Framebuffer stride
            size_t stride{0};
            /// Dimensions (pixels)
            std::pair<uint16_t, uint16_t> pixelSize;
        };

        /// Connector type mapping to names
        static const std::unordered_map<uint32_t, std::string_view> gConnectorNames;

        /// Console path to open (for disabling)
        constexpr static const std::string_view kTtyPath{"/dev/tty0"};


        void dumpDriResources();

        void getOutputDevice();
        void initKms();
        void initEventHandler();

        Buffer createBo(const struct _drmModeModeInfo &mode);
        void handleEvents();
        void requestFbFlip(const size_t newFbIndex);
        static void PageFlipHandler(int, unsigned int, unsigned int, unsigned int, void *);

        static std::string GetConnectorName(struct _drmModeConnector *);

        void disableTty();
        void enableTty();

    private:
        /// File descriptor for the card
        int driFd{-1};

        /// Original CRTC mode
        struct _drmModeCrtc *origCrtc{nullptr};
        /// Connector to use for output
        struct _drmModeConnector *connector{nullptr};
        /// Display encoder in use
        struct _drmModeEncoder *encoder{nullptr};
        /// Page flip handler
        struct _drmEventContext *pageFlipEvent{nullptr};

        /// Kernel modesetting driver
        struct kms_driver *kmsDriver{nullptr};
        /// Buffer objects corresponding to our framebuffers
        std::array<Buffer, 2> kmsBuffers;
        /// IDs of framebuffers
        std::array<uint32_t, 2> fbIds;
        /// Currently activated framebuffer (index)
        size_t currentFb{0};

        /// File descriptor to the tty which is attached to the fbdev
        int ttyFd{-1};

        /// Main event loop
        std::weak_ptr<EventLoop> ev;
        /// Event for handling drm messages
        struct event *drmEvent{nullptr};
};

#endif
