#include <common.h>
#include <env.h>
#include <i2c.h>
#include <i2c_eeprom.h>
#include <eeprom.h>
#include <eeprom_layout.h>
#include <stdint.h>
#include <string.h>

#include "conf_prom.h"
#include "encoding_helpers.h"
#include "hash_helpers.h"

/// Bus number our config PROM is on
#define BUS_NUM                         0
/// Length of serial number (bytes)
#define SERIAL_LEN                      16
/// Human readable serial number string (a base32-encoded 32-bit value)
#define SERIAL_STR_LEN                  10
/// Length of MAC address (bytes)
#define MAC_ADDR_LEN                    6

/// Flag indicating whether the config PROM has been read yet
static bool gPromRead = false;
/// Device serial number (human-readable string representation)
static char gDeviceSerial[SERIAL_STR_LEN];
/// Ethernet MAC address
static uint8_t gMacAddress[MAC_ADDR_LEN];

/**
 * @brief Takes the 16-byte serial number we've read in and converts it to a string.
 *
 * The serial number blob is run through a hash function, then base32 encoding to make it more
 * presentable to humans.
 */
static void FormatSerialNumber(void *snBytes, size_t numSnBytes) {
    // hash the serial number
    uint32_t value = do_hash_murmurhash3(snBytes, numSnBytes, 'PLSN');

    // base32 encode it
    memset(gDeviceSerial, '\0', SERIAL_STR_LEN);
    do_encode_base32(&value, sizeof(value), gDeviceSerial, SERIAL_STR_LEN);
}

/**
 * @brief Read the on-board configuration PROM
 *
 * Read out the data contained in the I2C configuration PROM (an AT24MAC402, containing both a
 * 128-bit serial number, and an EUI-48 MAC address) and apply it to the running system.
 *
 * This should be called during early board_init().
 *
 * Note that bus numbers are in ascending order: so I2C2 (our on-board bus) is bus 0, since I2C1 is
 * not used on the board.
 */
int pl_read_prom(void) {
    struct udevice *mux, *eepromSerial = NULL;
    int ret = 0;

    // prevent repeated reads
    if(gPromRead) {
        return 0;
    }

    // get handle to mux and activate the bus
    ret = i2c_get_chip_for_busnum(BUS_NUM, 0x70, 0, &mux);
    if(ret) {
        printf("%s (%s) failed!\n", "i2c_get_chip_for_busnum", "mux");
        return ret;
    }

    uint8_t muxOn = (1 << 1);
    ret = dm_i2c_write(mux, 0x70, &muxOn, 1);
    if(ret) {
        printf("%s failed!\n", "setting mux");
        return ret;
    }

    // get handle to EEPROM serial number area
    ret = i2c_get_chip_for_busnum(BUS_NUM, 0x58, 1, &eepromSerial);
    if(ret) {
        printf("%s (%s) failed!\n", "i2c_get_chip_for_busnum", "EEPROM");
        goto beach;
    }
    i2c_set_chip_offset_len(eepromSerial, 1);

    // read serial number and format for display
    uint8_t serialBytes[SERIAL_LEN];

    ret = dm_i2c_read(eepromSerial, 0x80, serialBytes, SERIAL_LEN);
    if(ret) {
        printf("%s failed!\n", "reading S/N");
        goto beach;
    }

    FormatSerialNumber(serialBytes, SERIAL_LEN);

    ret = env_set("serial#", gDeviceSerial);
    if(ret) {
        // TODO: check if the serial numbers differ
    }

    // read MAC address and save in environment
    ret = dm_i2c_read(eepromSerial, 0x9A, gMacAddress, MAC_ADDR_LEN);
    if(ret) {
        printf("%s failed!\n", "reading MAC");
        goto beach;
    }

    ret = eth_env_set_enetaddr("ethaddr", gMacAddress);
    (void) ret; // ignore errors here

    // we've read all data, so skip this routine next time
    gPromRead = true;

    // clean up
beach:;
    // reset mux state
    uint8_t muxOff = 0;
    ret = dm_i2c_write(mux, 0x70, &muxOff, 1);
    if(ret) {
        printf("%s failed!\n", "resetting mux");
    }

    return ret;
}

/**
 * @brief Print data read from config PROM
 *
 * Output the information that we read from the configuration PROM (that is, the serial number and
 * Ethernet MAC address) to the console.
 */
void pl_print_prom(void) {
    if(!gPromRead) {
        printf("!!! No IDPROM data available!\n");
        return;
    }

    printf("********** IDPROM data **********\n"
            "%14s: %pM\n"
            "%14s: %s\n\n",
            "Ethernet MAC", gMacAddress, "S/N", gDeviceSerial);
}
