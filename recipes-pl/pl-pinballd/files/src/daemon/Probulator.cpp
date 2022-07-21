#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <system_error>
#include <vector>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>

#include "Probulator.h"

/**
 * @brief Initialize the probulator
 *
 * Open the I²C bus and figure out where the IDPROM is chilling. We expect that it's an AT24C32
 * (or similar) type, which has its data area at address 0b1010XXX.
 */
Probulator::Probulator(const std::filesystem::path &i2cPath) {
    int err;

    // open the bus
    this->busFd = open(i2cPath.native().c_str(), O_RDWR);
    if(this->busFd == -1) {
        throw std::system_error(errno, std::generic_category(), 
                fmt::format("failed to open i2c bus ('{}')", i2cPath.native()));
    }

    // figure out where the EEPROM is chilling
    for(size_t i = 0; i < 8; i++) {
        const uint8_t address = 0b1010'000 + (i & 7);
        PLOG_DEBUG << fmt::format("Testing for IDPROM at ${:02x}", address);

        /*
         * Check if there's an EEPROM here by trying to read the header from the EEPROM; that is,
         * the first few bytes. We'll set up the read by writing a 16-bit address.
         */
        IdpromHeader header;

        std::array<struct i2c_msg, 2> msgs;
        for(auto &msg : msgs) {
            memset(&msg, 0, sizeof(msg));

            msg.addr = address;
        }

        std::array<uint8_t, 2> readAddr{{0, 0}};
        msgs[0].len = readAddr.size();
        msgs[0].buf = readAddr.data();

        msgs[1].flags = I2C_M_RD;
        msgs[1].len = sizeof(header);
        msgs[1].buf = reinterpret_cast<uint8_t *>(&header);

        struct i2c_rdwr_ioctl_data txns;
        txns.msgs = msgs.data();
        txns.nmsgs = msgs.size();

        err = ioctl(this->busFd, I2C_RDWR, &txns);
        if(err < 0) {
            // this means no device is there
            if(errno == ENXIO) {
                PLOG_VERBOSE << fmt::format("No device at ${:02x}", address);
                continue;
            }

            // other type of error
            PLOG_WARNING << fmt::format("failed to read IDPROM at ${:02x}: {} ({})", address,
                    errno, strerror(errno));
            continue;
        }

        // now verify the header
        header.swapFromEeprom();

        if(header.magic != IdpromHeader::kMagicValue) {
            PLOG_WARNING << fmt::format("IDPROM@${:02x} invalid magic (${:08x}, expected ${:08x})",
                    address, header.magic, IdpromHeader::kMagicValue);
            continue;
        }

        // neat, it's probably our desired IDPROM
        PLOG_INFO << fmt::format("Located IDPROM at ${:02x}", address);

        this->idpromAddress = address;
        this->idpromHeader = header;
        break;
    }
}

/**
 * @brief Close all allocated resources
 */
Probulator::~Probulator() {
    close(this->busFd);
}

/**
 * @brief Read the entirety of the IDPROM and parse its contents
 *
 * This determines all of the devices that are present on the front panel.
 */
void Probulator::probe() {
    int err;
    std::vector<std::byte> payload;

    if(!this->idpromAddress) {
        throw std::runtime_error("couldn't locate IDPROM");
    }

    // read the payload of the IDPROM
    payload.resize(this->idpromHeader.payloadLength);

    std::array<struct i2c_msg, 2> msgs;
    for(auto &msg : msgs) {
        memset(&msg, 0, sizeof(msg));

        msg.addr = this->idpromAddress;
    }

    std::array<uint8_t, 2> readAddr{{0, sizeof(IdpromHeader)}};
    msgs[0].len = readAddr.size();
    msgs[0].buf = readAddr.data();

    msgs[1].flags = I2C_M_RD;
    msgs[1].len = payload.size();
    msgs[1].buf = reinterpret_cast<uint8_t *>(payload.data());

    struct i2c_rdwr_ioctl_data txns;
    txns.msgs = msgs.data();
    txns.nmsgs = msgs.size();

    err = ioctl(this->busFd, I2C_RDWR, &txns);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "read IDPROM payload");
    }

    // parse the payload
    this->parseIdpromPayload(payload);
}

/**
 * @brief Parse the contents of the IDPROM
 *
 * Set up a CBOR decoder over the IDPROM contents and iterate over it to find all required keys.
 *
 * @param payload Payload buffer to decode
 */
void Probulator::parseIdpromPayload(std::span<const std::byte> payload) {
    PLOG_VERBOSE << fmt::format("Parsing IDPROM payload: {} bytes", payload.size());
}
