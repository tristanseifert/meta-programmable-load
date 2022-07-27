#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

#include <cbor.h>
#include <fmt/format.h>
#include <plog/Log.h>
#include <uuid.h>

#include "Utils/Base32.h"
#include "Utils/Cbor.h"

#include "LedManager.h"
#include "Probulator.h"
#include "drivers/DriverList.h"

/**
 * @brief Initialize the probulator
 *
 * Open the I²C bus and figure out where the IDPROM is chilling. We expect that it's an AT24C32
 * (or similar) type, which has its data area at address 0b1010XXX.
 */
Probulator::Probulator(const std::filesystem::path &i2cPath) {
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

        try {
            this->readIdprom(address, 0, {reinterpret_cast<std::byte *>(&header), sizeof(header)});
        } catch(const std::exception &e) {
            PLOG_WARNING << fmt::format("failed to read IDPROM at ${:02x}: {}", address, e.what());
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

    // initialize stuff
    this->led = std::make_shared<LedManager>();
}

/**
 * @brief Close all allocated resources
 */
Probulator::~Probulator() {
    this->led.reset();

    // shut down drivers
    this->drivers.clear();

    // close hardware resources
    close(this->busFd);
}

/**
 * @brief Read the entirety of the IDPROM and parse its contents
 *
 * This determines all of the devices that are present on the front panel.
 */
void Probulator::probe() {
    std::vector<std::byte> payload;

    if(!this->idpromAddress) {
        throw std::runtime_error("couldn't locate IDPROM");
    }

    // read the payload of the IDPROM
    payload.resize(this->idpromHeader.payloadLength);
    this->readIdprom(this->idpromAddress, sizeof(IdpromHeader), payload);

    // parse the payload
    this->parseIdpromPayload(payload);
    PLOG_INFO << "Hardware: " << this->hwDesc.value_or("unknown") << " rev "
        << this->hwRevision.value_or("(unknown)");
    PLOG_INFO << "Hardware s/n: " << this->hwSerial.value_or("(unknown)");
}

#include <sstream>
static void DumpPacket(const std::string_view &what, std::span<const std::byte> packet) {
    std::stringstream str;
    for(size_t i = 0; i < packet.size(); i++) {
        str << fmt::format("{:02x} ", packet[i]);
        if((i % 16) == 15) {
            str << std::endl;
        }
    }

    PLOG_DEBUG << what << ":" << std::endl << str.str();
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

    // set up CBOR decoder
    struct cbor_load_result result{};

    auto item = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
            &result);
    if(result.error.code != CBOR_ERR_NONE) {
        throw std::runtime_error(fmt::format("cbor_load failed: {} (at ${:x})", result.error.code,
                    result.error.position));
    }

    // validate the input
    if(!cbor_isa_map(item)) {
        throw std::runtime_error("invalid CBOR payload (expected map)");
    }

    auto keys = cbor_map_handle(item);
    const auto numKeys = cbor_map_size(item);

    /*
     * Get general information about the hardware.
     *
     * This consists of meta information, hardware revision information, and the actual serial
     * number of the board.
     */
    for(size_t i = 0; i < numKeys; i++) {
        auto &pair = keys[i];
        if(!cbor_isa_uint(pair.key)) {
            throw std::runtime_error(fmt::format("invalid CBOR key (expected uint, got {})",
                        cbor_typeof(pair.key)));
        }

        const auto key = cbor_get_uint32(pair.key);

        switch(key) {
            // ignore manufacturer string
            case static_cast<uint32_t>(IdpromKey::Manufacturer):
                break;
            // hw revision string
            case static_cast<uint32_t>(IdpromKey::HwRevision):
                if(!cbor_isa_string(pair.value) || !cbor_string_is_definite(pair.value)) {
                    throw std::runtime_error("invalid hw revision value (expected definite string)");
                }
                this->hwRevision = reinterpret_cast<const char *>(cbor_string_handle(pair.value));
                break;
            // hw description
            case static_cast<uint32_t>(IdpromKey::HardwareDescription):
                if(!cbor_isa_string(pair.value) || !cbor_string_is_definite(pair.value)) {
                    throw std::runtime_error("invalid hw desc value (expected definite string)");
                }
                this->hwDesc = reinterpret_cast<const char *>(cbor_string_handle(pair.value));
                break;
            // serial number pointer
            case static_cast<uint32_t>(IdpromKey::SerialPointer):
                if(!cbor_isa_array(pair.value)) {
                    throw std::runtime_error("invalid serial number ptr (expected array)");
                } else if(this->hwSerial) {
                    throw std::logic_error("encountered serial ptr, but already read serial!");
                }

                this->parseAndReadSerialNumberPointer(pair.value);
                break;
            // serial number string
            case static_cast<uint32_t>(IdpromKey::SerialString):
                if(!cbor_isa_string(pair.value) || !cbor_string_is_definite(pair.value)) {
                    throw std::runtime_error("invalid serial string (expected definite string)");
                }
                this->hwSerial = reinterpret_cast<const char *>(cbor_string_handle(pair.value));
                break;

            // drivers (processed later on)
            case static_cast<uint32_t>(IdpromKey::RequiredDrivers):
                break;

            default:
                PLOG_WARNING << fmt::format("unknown key ${:08x}", key);
        }
    }

    /*
     * Now we can go and figure out all the drivers. This is because the drivers might query what
     * revision board we're running on.
     *
     * Drivers are stored in a map, where the key of the map is a 16-byte uuid bytestring, and the
     * value is simply passed as-is to the driver initializer to make sense of.
     */
    for(size_t i = 0; i < numKeys; i++) {
        auto &pair = keys[i];
        const auto key = cbor_get_uint32(pair.key);

        if(key != static_cast<uint32_t>(IdpromKey::RequiredDrivers)) {
            continue;
        }

        // we should have a map as the value of this key
        if(!cbor_isa_map(pair.value)) {
            throw std::runtime_error("invalid driver list (expected map)");
        }

        // iterate through the map
        auto drivers = cbor_map_handle(pair.value);
        const auto numDrivers = cbor_map_size(pair.value);

        for(size_t j = 0; j < numDrivers; j++) {
            auto &driverPair = drivers[j];

            // validate that the key is a bytestring of the appropriate length and get the uuid
            if(!cbor_isa_bytestring(driverPair.key) ||
                    !cbor_bytestring_is_definite(driverPair.key)) {
                throw std::runtime_error("invalid driver key (expected bytestring)");
            }
            else if(cbor_bytestring_length(driverPair.key) != 16) {
                throw std::runtime_error(fmt::format("invalid driver uuid (got {} bytes)",
                            cbor_bytestring_length(driverPair.key)));
            }

            const auto ptr = cbor_bytestring_handle(driverPair.key);

            uuids::uuid driverId(ptr, ptr+16);

            // try to instantiate the associated driver
            auto driverIt = std::find_if(gSupportedDrivers.begin(), gSupportedDrivers.end(),
                    [&driverId](const auto &info) {
                return (driverId == info.id);
            });
            if(driverIt == gSupportedDrivers.end()) {
                throw std::runtime_error(fmt::format("unsupported driver (id {})",
                            uuids::to_string(driverId)));
            }

            const auto &driverInfo = *driverIt;
            PLOG_DEBUG << "Found driver " << uuids::to_string(driverId) << ": " << driverInfo.name;

            try {
                driverInfo.constructor(this, driverId, driverPair.value);
            } catch(const std::exception &e) {
                PLOG_ERROR << "failed to init driver " << driverInfo.name << ": " << e.what();
                throw std::runtime_error("driver initialization failed");
            }
        }
    }

    // clean up
    cbor_decref(&item);
}

/**
 * @brief Decode a serial number pointer and follow it
 *
 * Read the serial number pointer array (an array containing an I²C device address, a read address
 * and number of bytes to read) to read out the serial number of the hardware.
 *
 * Once the binary serial number has been read, we'll run it through a base32 encoding pass to
 * stringify it.
 *
 * @param array A CBOR array containing the required data
 */
void Probulator::parseAndReadSerialNumberPointer(struct cbor_item_t *array) {
    uint8_t deviceAddress{0};
    uint16_t readAddress{0}, readNumBytes{0};

    cbor_item_t *temp;

    // validate the input
    if(cbor_array_is_indefinite(array)) {
        throw std::runtime_error("indefinite arrays not supported");
    }
    else if(cbor_array_size(array) < 3) {
        throw std::runtime_error(fmt::format("invalid serial number ptr (have {} items)",
                    cbor_array_size(array)));
    }

    // get the device address
    temp = cbor_array_get(array, 0);
    if(!temp) {
        throw std::logic_error("failed to decode sn ptr device address");
    } else if(!cbor_isa_uint(temp)) {
        throw std::runtime_error("invalid sn ptr device address (expected uint)");
    }
    deviceAddress = Util::CborReadUint(temp);

    // get starting address
    temp = cbor_array_get(array, 1);
    if(!temp) {
        throw std::logic_error("failed to decode sn ptr read address");
    } else if(!cbor_isa_uint(temp)) {
        throw std::runtime_error("invalid sn ptr read address (expected uint)");
    }
    readAddress = Util::CborReadUint(temp);

    // get total number of bytes
    temp = cbor_array_get(array, 2);
    if(!temp) {
        throw std::logic_error("failed to decode sn ptr read length");
    } else if(!cbor_isa_uint(temp)) {
        throw std::runtime_error("invalid sn ptr read length (expected uint)");
    }
    readNumBytes = Util::CborReadUint(temp);

    PLOG_VERBOSE << fmt::format("reading sn from device ${:02x}, offset ${:04x} ({} bytes)",
            deviceAddress, readAddress, readNumBytes);

    // follow the sn pointer
    std::vector<std::byte> snBytes;
    snBytes.resize(readNumBytes);

    this->readIdprom(deviceAddress, readAddress, snBytes);

    std::vector<char> snChars;
    snChars.resize(snBytes.size());

    auto totalBytes = Util::Base32::Encode(snBytes, snChars);
    if(totalBytes < 0) {
        throw std::runtime_error("Base32::Encode failed");
    }
    snChars.resize(totalBytes);

    this->hwSerial = std::string(snChars.data(), snChars.size());
}

/**
 * @brief Register a driver instance
 *
 * Store a reference to the driver so we can cleanly shut it down later.
 */
void Probulator::registerDriver(const std::shared_ptr<DriverBase> &driver) {
    this->drivers.emplace_back(driver);

    driver->driverDidRegister(this);
}



/**
 * @brief Read data from the IDPROM
 *
 * @param startAddress Physical address into the IDPROM to read data from
 * @param outBuffer Buffer to receive the read data
 */
void Probulator::readIdprom(const uint8_t deviceAddress, const uint16_t startAddress,
        std::span<std::byte> outBuffer) {
    std::array<struct i2c_msg, 2> msgs;
    for(auto &msg : msgs) {
        memset(&msg, 0, sizeof(msg));
        msg.addr = deviceAddress;
    }

    std::array<uint8_t, 2> readAddr{{static_cast<uint8_t>(startAddress >> 8),
        static_cast<uint8_t>(startAddress & 0xff)}};
    msgs[0].len = readAddr.size();
    msgs[0].buf = readAddr.data();

    msgs[1].flags = I2C_M_RD;
    msgs[1].len = outBuffer.size();
    msgs[1].buf = reinterpret_cast<uint8_t *>(outBuffer.data());

    struct i2c_rdwr_ioctl_data txns;
    txns.msgs = msgs.data();
    txns.nmsgs = msgs.size();

    int err = ioctl(this->busFd, I2C_RDWR, &txns);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "read IDPROM");
    }
}

/**
 * @brief Write data to the IDPROM
 *
 * This is a helper routine that allows writing to an IDPROM, assuming it's not write-protected in
 * hardware.
 *
 * @param base Address to write the data at in the EEPROM array
 * @param data Byte string to write to the EEPROM array
 */
void Probulator::writeIdprom(const uint8_t deviceAddress, const uint16_t base,
        std::span<const std::byte> data) {
    size_t bytesWritten{0};

    // handle a partial page at the start
    if(base % kPageSize) {
        this->writeIdpromPage(deviceAddress, base,
                data.subspan(0, kPageSize - (base % kPageSize)));
        bytesWritten += (base % kPageSize);
    }

    // write full pages
    const size_t totalNumPages = (data.size() - bytesWritten) / kPageSize;

    for(size_t i = 0; i < totalNumPages; i++) {
        this->writeIdpromPage(deviceAddress, base + bytesWritten,
                data.subspan(bytesWritten, kPageSize));
        bytesWritten += kPageSize;
    }

    // write any partial ending page
    if(totalNumPages < data.size()) {
        this->writeIdpromPage(deviceAddress, base + bytesWritten, data.subspan(bytesWritten));
    }
}

/**
 * @brief Write a page of IDPROM data
 *
 * @seeAlso kPageSize
 */
void Probulator::writeIdpromPage(const uint8_t deviceAddress, const uint16_t base,
        std::span<const std::byte> data) {
    int err;

    using namespace std::chrono_literals;
    PLOG_VERBOSE << "IDPROM write: " << fmt::format("{} bytes to ${:04x}", data.size(), base);

    // validate args
    if(data.size() > kPageSize) {
        throw std::invalid_argument("data must not be larger than a page");
    }
    else if((base % kPageSize) + data.size() > kPageSize) {
        throw std::invalid_argument(fmt::format("page write would wrap ({} bytes at ${:04x})",
                    data.size(), base));
    }

    // do the write
    std::vector<std::byte> msg;
    msg.resize(data.size() + 2);

    msg[0] = std::byte{static_cast<uint8_t>(base >> 8)};
    msg[1] = std::byte{static_cast<uint8_t>(base & 0xff)};

    memcpy(msg.data() + 2, data.data(), data.size());
    DumpPacket("write txn", msg);

    err = ioctl(this->busFd, I2C_SLAVE, deviceAddress);
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "write IDPROM payload");
    }

    err = write(this->busFd, msg.data(), msg.size());
    if(err < 0) {
        throw std::system_error(errno, std::generic_category(), "write IDPROM payload");
    }

    // wait for write to complete
    std::this_thread::sleep_for(10ms);
}
