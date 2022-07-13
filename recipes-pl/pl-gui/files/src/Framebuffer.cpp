#include <unistd.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <cerrno>
#include <cstring>
#include <sstream>
#include <system_error>

#include <event2/event.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libkms.h>

#include <fmt/format.h>
#include <plog/Log.h>

#include "EventLoop.h"
#include "Framebuffer.h"

struct type_name {
    unsigned int type;
    const char *name;
};

const std::unordered_map<uint32_t, std::string_view> Framebuffer::gConnectorNames{{
    { DRM_MODE_CONNECTOR_Unknown, "unknown" },
    { DRM_MODE_CONNECTOR_VGA, "VGA" },
    { DRM_MODE_CONNECTOR_DVII, "DVI-I" },
    { DRM_MODE_CONNECTOR_DVID, "DVI-D" },
    { DRM_MODE_CONNECTOR_DVIA, "DVI-A" },
    { DRM_MODE_CONNECTOR_Composite, "composite" },
    { DRM_MODE_CONNECTOR_SVIDEO, "s-video" },
    { DRM_MODE_CONNECTOR_LVDS, "LVDS" },
    { DRM_MODE_CONNECTOR_Component, "component" },
    { DRM_MODE_CONNECTOR_9PinDIN, "9-pin DIN" },
    { DRM_MODE_CONNECTOR_DisplayPort, "DP" },
    { DRM_MODE_CONNECTOR_HDMIA, "HDMI-A" },
    { DRM_MODE_CONNECTOR_HDMIB, "HDMI-B" },
    { DRM_MODE_CONNECTOR_TV, "TV" },
    { DRM_MODE_CONNECTOR_eDP, "eDP" },
    { DRM_MODE_CONNECTOR_VIRTUAL, "Virtual" },
    { DRM_MODE_CONNECTOR_DSI, "DSI" },
    { DRM_MODE_CONNECTOR_DPI, "DPI" },
}};

/**
 * @brief Allocate a framebuffer
 *
 * @param ev Event loop to attach event handlers to
 * @param path Filesystem path to the DRI device to open
 */
Framebuffer::Framebuffer(const std::shared_ptr<EventLoop> &ev, const std::string_view path) :
        ev(ev) {
    // open the card
    this->driFd = open(path.data(), O_RDWR);
    if(this->driFd == -1) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("open card '{}'", path));
    }

    // open the TTY and then disable it
    this->ttyFd = open(kTtyPath.data(), O_RDWR);
    if(this->ttyFd == -1) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("open tty '{}'", kTtyPath));
    }

    this->disableTty();

    // set up a dual buffered framebuffer
    this->getOutputDevice();
    this->initKms();
    this->initEventHandler();
}

/**
 * @brief Restore the original state of the framebuffer
 */
Framebuffer::~Framebuffer() {
    int err;

    // restore original mode
    err = drmModeSetCrtc(this->driFd, this->origCrtc->crtc_id, this->origCrtc->buffer_id,
            this->origCrtc->x, this->origCrtc->y, &this->connector->connector_id, 1,
            &this->origCrtc->mode);
    if(err) {
        PLOG_WARNING << "drmModeSetCrtc failed: " << err << "(" << errno << ")";
    }

    // kill the event
    event_free(this->drmEvent);

    // clean up resources
    if(this->connector) {
        drmModeFreeConnector(this->connector);
    }
    if(this->encoder) {
        drmModeFreeEncoder(this->encoder);
    }

    delete this->pageFlipEvent;

    // close fd
    close(this->driFd);

    // restore TTY
    this->enableTty();
    close(this->ttyFd);
}



/**
 * @brief Print all DRI resources
 */
void Framebuffer::dumpDriResources() {
    drmModeResPtr res;

    res = drmModeGetResources(this->driFd);
    if (!res) {
        throw std::system_error(errno, std::generic_category(), "drmModeGetResources");
    }

    printf("connectors: list");
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnectorPtr connector{nullptr};
        drmModeEncoderPtr encoder{nullptr};

        printf("\nNumber: %d ", res->connectors[i]);
        connector = drmModeGetConnectorCurrent(this->driFd, res->connectors[i]);

        if(!connector) {
            continue;
        }

        auto name = GetConnectorName(connector);
        printf("Name: %s\n", name.c_str());

        printf("Encoder: %d ", connector->encoder_id);

        encoder = drmModeGetEncoder(this->driFd, connector->encoder_id);
        if (!encoder)
            continue;

        printf("Crtc: %d", encoder->crtc_id);

        drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
    }

    printf("\nFramebuffers: ");
    for (int i = 0; i < res->count_fbs; i++) {
        printf("%d ", res->fbs[i]);
    }

    printf("\nCRTCs: ");
    for (int i = 0; i < res->count_crtcs; i++) {
        printf("%d ", res->crtcs[i]);
    }

    printf("\nencoders: ");
    for (int i = 0; i < res->count_encoders; i++) {
        printf("%d ", res->encoders[i]);
    }
    printf("\n");

    drmModeFreeResources(res);
}

/**
 * @brief Find a connector and encoder to use for output
 *
 * We'll select the first connector with an active display connection, and its encoder for use to
 * output.
 */
void Framebuffer::getOutputDevice() {
    auto res = drmModeGetResources(this->driFd);
    if (!res) {
        throw std::system_error(errno, std::generic_category(), "drmModeGetResources");
    }

    for(size_t i = 0; i < res->count_connectors; i++) {
        auto conn = drmModeGetConnector(this->driFd, res->connectors[i]);
        if(conn) {
            // check if it's connected and has display modes
            if(conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
                PLOG_DEBUG << "selected output: " << GetConnectorName(conn);

                this->connector = conn;
                break;
            }

            // the connector didn't match so release it
            drmModeFreeConnector(conn);
        } else {
            PLOG_WARNING << "connector " << i << " is null!";
        }
    }

    if(!this->connector) {
        throw std::runtime_error("no connectors with displays available");
    }

    // find the encoder corresponding to the connector
    for(size_t i = 0; i < res->count_encoders; i++) {
        auto enc = drmModeGetEncoder(this->driFd, res->encoders[i]);
        if(enc) {
            if(enc->encoder_id == connector->encoder_id) {
                this->encoder = enc;
                break;
            }

            // encoder didn't match so release it
            drmModeFreeEncoder(enc);
        } else {
            PLOG_WARNING << "encoder " << i << " is null!";
        }
    }

    if(!this->encoder) {
        throw std::runtime_error(fmt::format("failed to find encoder id {}",
                    connector->encoder_id));
    }

    // store a copy of the previous crtc mode
    this->origCrtc = drmModeGetCrtc(this->driFd, this->encoder->crtc_id);
    if(!this->origCrtc) {
        throw std::system_error(errno, std::generic_category(), "drmModeGetCrtc");
    }
}

/**
 * @brief Initialize kernel modesetting
 *
 * Create some kernel modesetting resources, including a pair of framebuffers which we can flip
 * between when drawing.
 */
void Framebuffer::initKms() {
    int err;

    // get preferred display mode for display
    auto &mode = this->connector->modes[0];
    PLOG_INFO << "display mode: " << fmt::format("{}x{}", mode.hdisplay, mode.vdisplay);

    // create KMS driver and get info
    err = kms_create(this->driFd, &this->kmsDriver);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "kms_create");
    }

    // create the first buffer object and framebuffer
    this->kmsBuffers[0] = this->createBo(mode);

    err = drmModeAddFB(this->driFd, mode.hdisplay, mode.vdisplay, 24, 32,
            this->kmsBuffers[0].stride, this->kmsBuffers[0].handle, &this->fbIds[0]);
    if(err) {
        throw std::system_error(errno, std::generic_category(), fmt::format("drmModeAddFB1 ({}x{})",
                    mode.hdisplay, mode.vdisplay));
    }

    // set the mode
    err = drmModeSetCrtc(this->driFd, this->encoder->crtc_id, this->fbIds[0],
            // x, y
            0, 0,
            // connector array and number of entries
            &this->connector->connector_id, 1, &mode);
    if(err) {
        throw std::system_error(errno, std::generic_category(), fmt::format("drmModeSetCrtc ({}x{})",
                    mode.hdisplay, mode.vdisplay));
    }

    // create another buffer object and framebuffer as the backbuffer
    this->kmsBuffers[1] = this->createBo(mode);

    err = drmModeAddFB(this->driFd, mode.hdisplay, mode.vdisplay, 24, 32,
            this->kmsBuffers[1].stride, this->kmsBuffers[1].handle, &this->fbIds[1]);
    if(err) {
        throw std::system_error(errno, std::generic_category(), fmt::format("drmModeAddFB2 ({}x{})",
                    mode.hdisplay, mode.vdisplay));
    }

    // schedule page flip for next vblank
    this->requestFbFlip(1);

    // set up page flip handler
    this->pageFlipEvent = new drmEventContext;
    memset(this->pageFlipEvent, 0, sizeof(*this->pageFlipEvent));

    this->pageFlipEvent->version = DRM_EVENT_CONTEXT_VERSION;
    this->pageFlipEvent->page_flip_handler = Framebuffer::PageFlipHandler;
}

/**
 * @brief Initialize the DRM event handler
 *
 * This creates a libevent object that observes the DRI file descriptor. When it becomes readable,
 * we'll request to process events.
 */
void Framebuffer::initEventHandler() {
    auto evbase = this->ev.lock()->getEvBase();

    this->drmEvent = event_new(evbase, this->driFd, (EV_READ | EV_PERSIST),
            [](auto fd, auto what, auto ctx) {
        auto fb = reinterpret_cast<Framebuffer *>(ctx);
        try {
            fb->handleEvents();
        } catch(const std::exception &e) {
            PLOG_ERROR << "failed to handle DRM event: " << e.what();
        }
    }, this);
    if(!this->drmEvent) {
        throw std::runtime_error("failed to allocate DRM event");
    }

    event_add(this->drmEvent, nullptr);
}

/**
 * @brief Create a new buffer object
 *
 * Allocate a buffer object with the given mode info.
 *
 * @param mode Display mode to use
 */
Framebuffer::Buffer Framebuffer::createBo(const drmModeModeInfo &mode) {
    Buffer buf;
    int err;
    unsigned int pitch;

    // create the bo
    unsigned boAttribs[] = {
        KMS_WIDTH,      mode.hdisplay,
        KMS_HEIGHT,     mode.vdisplay,
        KMS_BO_TYPE,    KMS_BO_TYPE_SCANOUT_X8R8G8B8,
        KMS_TERMINATE_PROP_LIST
    };

    err = kms_bo_create(this->kmsDriver, boAttribs, &buf.bo);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "kms_bo_create");
    }

    // get buffer stride
    err = kms_bo_get_prop(buf.bo, KMS_PITCH, &pitch);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "kms_bo_get_prop(KMS_PITCH)");
    }
    buf.stride = pitch;
    buf.pixelSize = {mode.hdisplay, mode.vdisplay};

    // then get the handle
    err = kms_bo_get_prop(buf.bo, KMS_HANDLE, &buf.handle);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "kms_bo_get_prop(KMS_HANDLE)");
    }

    // map buffer to userspace
    err = kms_bo_map(buf.bo, &buf.fb);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "kms_bo_map");
    }

    return buf;
}

/**
 * @brief Handle DRM events
 */
void Framebuffer::handleEvents() {
    auto err = drmHandleEvent(this->driFd, this->pageFlipEvent);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "drmHandleEvent");
    }
}

/**
 * @brief Request framebuffer flip
 *
 * Enqueue a flip to the framebuffer with the specified index at the next vertical blanking
 * interval of the display.
 *
 * @param index Framebuffer index ([0, 1]) to display
 */
void Framebuffer::requestFbFlip(const size_t index) {
    int err = drmModePageFlip(this->driFd, this->encoder->crtc_id, this->fbIds[index],
            DRM_MODE_PAGE_FLIP_EVENT, this);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "drmModePageFlip");
    }

    this->currentFb = index;
}

/**
 * @brief Page flip callback
 *
 * Invoked from the event loop by the framebuffer event handler when the page has flipped.
 *
 * @param fd DRI file descriptor
 * @param ctx User-specified context ptr (always `this` of the instance)
 */
void Framebuffer::PageFlipHandler(int fd, unsigned int seq, unsigned int tv_sec,
        unsigned int tv_usec, void *ctx) {
    auto fb = reinterpret_cast<Framebuffer *>(ctx);
    const auto nextFb = fb->currentFb ? 0 : 1;

    // invoke callbacks (TODO: validate buffer offset is right)
    for(const auto &[token, cb] : fb->swapCallbacks) {
        cb(nextFb);
    }

    // flip to the other buffer
    fb->requestFbFlip(nextFb);
}



/**
 * @brief Get a string name for the given connector
 *
 * This name consists of the type string, and then the connector type id, separated by a dash.
 *
 * @param connector DRM connector to get the name for
 *
 * @return A string containing the DRM name
 */
std::string Framebuffer::GetConnectorName(drmModeConnectorPtr connector) {
    std::stringstream st;

    // get the name of the connector type
    if(gConnectorNames.contains(connector->connector_type)) {
        st << gConnectorNames.at(connector->connector_type);
    } else {
        st << "???";
    }

    // append the type id
    st << "-";
    st << connector->connector_type_id;

    return st.str();
}



/**
 * @brief Install a buffer swap callback
 *
 * Add a callback that's invoked during vertical blanking, after we've switched to output a
 * framebuffer for drawing. The alternate buffer is used as a back buffer to draw to with no
 * performance penalty or visual artifacts.
 *
 * @param cb Callback function to install
 *
 * @return A token that can be used to remove the callback later
 */
uint32_t Framebuffer::addSwapCallback(const SwapCallback &cb) {
    uint32_t token{0};

    do {
        token = ++this->swapCallbackToken;
    } while(!token);

    this->swapCallbacks.emplace(token, cb);

    return token;
}

/**
 * @brief Remove a previously installed buffer swap callback
 *
 * @param token A swap callback token as returned by addSwapCallback
 */
void Framebuffer::removeSwapCallback(const uint32_t token) {
    this->swapCallbacks.erase(token);
}


/**
 * @brief Disable the framebuffer console
 */
void Framebuffer::disableTty() {
    PLOG_VERBOSE << "disable tty";

    if(ioctl(this->ttyFd, KDSETMODE, KD_GRAPHICS) == -1) {
        throw std::system_error(errno, std::generic_category(), "KDSETMODE");
    }
}

/**
 * @brief Enable the framebuffer console
 */
void Framebuffer::enableTty() {
    PLOG_VERBOSE << "enable tty";

    if(ioctl(this->ttyFd, KDSETMODE, KD_TEXT) == -1) {
        throw std::system_error(errno, std::generic_category(), "KDSETMODE");
    }
}
