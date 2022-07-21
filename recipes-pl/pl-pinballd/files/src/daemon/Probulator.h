#ifndef PROBULATOR_H
#define PROBULATOR_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include <arpa/inet.h>

/**
 * @brief Front panel hardware prober
 *
 * Probulator serves as an interface to the I²C IDPROM on the front panel, reading it and decoding
 * its contents to determine what devices are present.
 *
 * The IDPROM contents have a small header (for validity checking) but all actual data consists of
 * a CBOR encoded map; the keys of which are 32-bit integers.
 */
class Probulator {
    private:
        /**
         * @brief IDPROM header
         *
         * All multibyte values are stored in network byte order.
         */
        struct IdpromHeader {
            /// Magic value; should be fixed
            uint32_t magic;
            /// Length of the payload (bytes)
            uint16_t payloadLength;

            /**
             * @brief Flags
             *
             * The following flag values are supported:
             *
             * - (1 << 0): Payload is compressed (using lzma/xz)
             */
            uint8_t flags;

            /// Reserved, set to 0
            uint8_t reserved{0};

            /// Expected magic value
            constexpr static const uint32_t kMagicValue{'BlaZ'};

            /**
             * @brief Swaps all multi-byte fields from the storage byte order
             */
            inline void swapFromEeprom() {
                this->magic = ntohl(this->magic);
                this->payloadLength = ntohs(this->payloadLength);
            }
            /**
             * @brief Swaps all multi-byte fields from host to storage byte order
             */
            inline void swapToEeprom() {
                this->magic = htonl(this->magic);
                this->payloadLength = htons(this->payloadLength);
            }
        } __attribute__((packed));

        /**
         * @brief Keys for the IDPROM payload
         *
         * These are 32-bit integers (specified as multicharacter constants) that define the keys
         * in the CBOR map in the payload.
         */
        enum class IdpromKey: uint32_t {
            /**
             * @brief Hardware revision
             *
             * String containing information about what revision front panel hardware is installed
             */
            HwRevision                  = 'HRev',
            /**
             * @brief Hardware description info string
             *
             * An user-readable description of what hardware we've got
             */
            HardwareDescription         = 'hwin',
            /**
             * @brief Manufacturer string
             */
            Manufacturer                = 'manu',

            /**
             * @brief Serial number string
             *
             * A string that contains the serial number of the assembly.
             *
             * @remark This option is mutually exclusive with SerialPointer.
             *
             * @seeAlso SerialPointer
             */
            SerialString                = 'snum',
            /**
             * @brief Serial number EEPROM pointer
             *
             * An array that contains three entries: an I²C bus address, a start address, and a
             * byte count. This is used to perform a read against a device on the bus to retrieve
             * the serial number instead of reading a string directly.
             *
             * This is useful for instances where the board uses an EEPROM that also contains a
             * preloaded unique serial number, such as an AT24CS32.
             *
             * @remark This option is mutually exclusive with SerialString.
             *
             * @seeAlso SerialString
             */
            SerialPointer               = 'Snpt',

            /**
             * @brief Required drivers
             *
             * Its value is a map, where each key in turn is a 16-byte UUID encoded as a byte
             * string. The value of this map is driver-specific and is not parsed; if the driver
             * does not need any bonus data, it can set the value to `null`.
             */
            RequiredDrivers             = 'Driv',
        };

    public:
        Probulator(const std::filesystem::path &i2cPath);
        ~Probulator();

        void probe();

    private:
        void parseIdpromPayload(std::span<const std::byte> payload);
        void parseAndReadSerialNumberPointer(struct cbor_item_t *);

        void readIdprom(const uint8_t deviceAddress, const uint16_t startAddress,
                std::span<std::byte> outBuffer);
        void writeIdprom(const uint8_t deviceAddress, const uint16_t startAddress,
                std::span<const std::byte> data);
        void writeIdpromPage(const uint8_t deviceAddress, const uint16_t base,
                std::span<const std::byte> data);

    private:
        // IDPROM page write size
        constexpr static const size_t kPageSize{32};

        /// file descriptor to the I2C bus the device is on
        int busFd{-1};

        /// Header read from the IDPROM
        IdpromHeader idpromHeader;
        /// Bus address of the IDPROM
        uint8_t idpromAddress{0};

        /// Currently attached hardware revision
        std::optional<std::string> hwRevision;
        /// Attached type of front panel
        std::optional<std::string> hwDesc;
        /// Serial number of the attached hardware
        std::optional<std::string> hwSerial;
};

#endif
