#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <gpiod.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <system_error>

#include <fmt/format.h>
#include <plog/Log.h>

#include "drivers/gpio/GpioChip.h"
#include "Nt35510.h"

using namespace Drivers::Lcd;

const std::array<Nt35510::PanelData, Nt35510::kNumPanels> Nt35510::gPanelData{
    {
        "ER-TFT040-1",
        {
            // some sort of magic (35510h) from example code
            {0xf000, 0x55},
            {0xf001, 0xaa},
            {0xf002, 0x52},
            {0xf003, 0x08},
            {0xf004, 0x01},

            // set AVDD to 5.2V
            {0xb000, 0x0d},
            {0xb001, 0x0d},
            {0xb002, 0x0d},
            // AVDD ratio
            {0xb600, 0x34},
            {0xb601, 0x34},
            {0xb602, 0x34},
            // AVEE = -5.2V
            {0xb100, 0x0d},
            {0xb101, 0x0d},
            {0xb102, 0x0d},
            // AVEE ratio
            {0xb700, 0x34},
            {0xb701, 0x34},
            {0xb702, 0x34},
            // VCL = -2.5V
            {0xb200, 0x00},
            {0xb201, 0x00},
            {0xb202, 0x00},
            // VCL ratio
            {0xb800, 0x24},
            {0xb801, 0x24},
            {0xb802, 0x24},
            // VGH = 15V
            {0xbf00, 0x01},
            {0xb300, 0x0f},
            {0xb301, 0x0f},
            {0xb302, 0x0f},
            // VGH ratio
            {0xb900, 0x34},
            {0xb901, 0x34},
            {0xb902, 0x34},
            // VGL_REG = -10V
            {0xb500, 0x08},
            {0xb500, 0x08},
            {0xb501, 0x08},
            {0xc200, 0x03},
            // VGLX ratio
            {0xba00, 0x24},
            {0xba01, 0x24},
            {0xba02, 0x24},
            // VGMN/VGSN -4.5V/0V
            {0xbd00, 0x00},
            {0xbd01, 0x78},
            {0xbd02, 0x00},
            // VCOM = -1.325V
            {0xbe00, 0x00},
            {0xbe01, 0x89}, // 69 (?)
            // gamma setting
{0xD100, 0x00},
{0xD101, 0x2D},
{0xD102, 0x00},
{0xD103, 0x2E},
{0xD104, 0x00}, 
{0xD105, 0x32},
{0xD106, 0x00},
{0xD107, 0x44},
{0xD108, 0x00},
{0xD109, 0x53},
{0xD10A, 0x00},
{0xD10B, 0x88},
{0xD10C, 0x00},
{0xD10D, 0xB6},
{0xD10E, 0x00},
{0xD10F, 0xF3},	//
{0xD110, 0x01},
{0xD111, 0x22},
{0xD112, 0x01},
{0xD113, 0x64},
{0xD114, 0x01},
{0xD115, 0x92},
{0xD116, 0x01},
{0xD117, 0xD4},
{0xD118, 0x02},	
{0xD119, 0x07},
{0xD11A, 0x02},
{0xD11B, 0x08},
{0xD11C, 0x02},
{0xD11D, 0x34},
{0xD11E, 0x02},
{0xD11F, 0x5F}, //
{0xD120, 0x02},
{0xD121, 0x78},
{0xD122, 0x02},
{0xD123, 0x94},
{0xD124, 0x02},
{0xD125, 0xA6},
{0xD126, 0x02},
{0xD127, 0xBB},
{0xD128, 0x02}, 
{0xD129, 0xCA},
{0xD12A, 0x02},
{0xD12B, 0xDB},
{0xD12C, 0x02},
{0xD12D, 0xE8},
{0xD12E, 0x02},
{0xD12F, 0xF9}, //
{0xD130, 0x03}, 
{0xD131, 0x1F},
{0xD132, 0x03},
{0xD133, 0x7F},
			 
{0xD200, 0x00},
{0xD201, 0x2D},
{0xD202, 0x00},
{0xD203, 0x2E},
{0xD204, 0x00}, 
{0xD205, 0x32},
{0xD206, 0x00},
{0xD207, 0x44},
{0xD208, 0x00},
{0xD209, 0x53},
{0xD20A, 0x00},
{0xD20B, 0x88},
{0xD20C, 0x00},
{0xD20D, 0xB6},
{0xD20E, 0x00},
{0xD20F, 0xF3},	//
{0xD210, 0x01},
{0xD211, 0x22},
{0xD212, 0x01},
{0xD213, 0x64},
{0xD214, 0x01},
{0xD215, 0x92},
{0xD216, 0x01},
{0xD217, 0xD4},
{0xD218, 0x02},	
{0xD219, 0x07},
{0xD21A, 0x02},
{0xD21B, 0x08},
{0xD21C, 0x02},
{0xD21D, 0x34},
{0xD21E, 0x02},
{0xD21F, 0x5F}, //
{0xD220, 0x02},
{0xD221, 0x78},
{0xD222, 0x02},
{0xD223, 0x94},
{0xD224, 0x02},
{0xD225, 0xA6},
{0xD226, 0x02},
{0xD227, 0xBB},
{0xD228, 0x02}, 
{0xD229, 0xCA},
{0xD22A, 0x02},
{0xD22B, 0xDB},
{0xD22C, 0x02},
{0xD22D, 0xE8},
{0xD22E, 0x02},
{0xD22F, 0xF9}, //
{0xD230, 0x03}, 
{0xD231, 0x1F},
{0xD232, 0x03},
{0xD233, 0x7F},
		 
{0xD300, 0x00},
{0xD301, 0x2D},
{0xD302, 0x00},
{0xD303, 0x2E},
{0xD304, 0x00}, 
{0xD305, 0x32},
{0xD306, 0x00},
{0xD307, 0x44},
{0xD308, 0x00},
{0xD309, 0x53},
{0xD30A, 0x00},
{0xD30B, 0x88},
{0xD30C, 0x00},
{0xD30D, 0xB6},
{0xD30E, 0x00},
{0xD30F, 0xF3},	//
{0xD310, 0x01},
{0xD311, 0x22},
{0xD312, 0x01},
{0xD313, 0x64},
{0xD314, 0x01},
{0xD315, 0x92},
{0xD316, 0x01},
{0xD317, 0xD4},
{0xD318, 0x02},	
{0xD319, 0x07},
{0xD31A, 0x02},
{0xD31B, 0x08},
{0xD31C, 0x02},
{0xD31D, 0x34},
{0xD31E, 0x02},
{0xD31F, 0x5F}, //
{0xD320, 0x02},
{0xD321, 0x78},
{0xD322, 0x02},
{0xD323, 0x94},
{0xD324, 0x02},
{0xD325, 0xA6},
{0xD326, 0x02},
{0xD327, 0xBB},
{0xD328, 0x02}, 
{0xD329, 0xCA},
{0xD32A, 0x02},
{0xD32B, 0xDB},
{0xD32C, 0x02},
{0xD32D, 0xE8},
{0xD32E, 0x02},
{0xD32F, 0xF9}, //
{0xD330, 0x03}, 
{0xD331, 0x1F},
{0xD332, 0x03},
{0xD333, 0x7F},
		 
{0xD400, 0x00},
{0xD401, 0x2D},
{0xD402, 0x00},
{0xD403, 0x2E},
{0xD404, 0x00}, 
{0xD405, 0x32},
{0xD406, 0x00},
{0xD407, 0x44},
{0xD408, 0x00},
{0xD409, 0x53},
{0xD40A, 0x00},
{0xD40B, 0x88},
{0xD40C, 0x00},
{0xD40D, 0xB6},
{0xD40E, 0x00},
{0xD40F, 0xF3},	//
{0xD410, 0x01},
{0xD411, 0x22},
{0xD412, 0x01},
{0xD413, 0x64},
{0xD414, 0x01},
{0xD415, 0x92},
{0xD416, 0x01},
{0xD417, 0xD4},
{0xD418, 0x02},	
{0xD419, 0x07},
{0xD41A, 0x02},
{0xD41B, 0x08},
{0xD41C, 0x02},
{0xD41D, 0x34},
{0xD41E, 0x02},
{0xD41F, 0x5F}, //
{0xD420, 0x02},
{0xD421, 0x78},
{0xD422, 0x02},
{0xD423, 0x94},
{0xD424, 0x02},
{0xD425, 0xA6},
{0xD426, 0x02},
{0xD427, 0xBB},
{0xD428, 0x02}, 
{0xD429, 0xCA},
{0xD42A, 0x02},
{0xD42B, 0xDB},
{0xD42C, 0x02},
{0xD42D, 0xE8},
{0xD42E, 0x02},
{0xD42F, 0xF9}, //
{0xD430, 0x03}, 
{0xD431, 0x1F},
{0xD432, 0x03},
{0xD433, 0x7F},
		 
{0xD500, 0x00},
{0xD501, 0x2D},
{0xD502, 0x00},
{0xD503, 0x2E},
{0xD504, 0x00}, 
{0xD505, 0x32},
{0xD506, 0x00},
{0xD507, 0x44},
{0xD508, 0x00},
{0xD509, 0x53},
{0xD50A, 0x00},
{0xD50B, 0x88},
{0xD50C, 0x00},
{0xD50D, 0xB6},
{0xD50E, 0x00},
{0xD50F, 0xF3},	//
{0xD510, 0x01},
{0xD511, 0x22},
{0xD512, 0x01},
{0xD513, 0x64},
{0xD514, 0x01},
{0xD515, 0x92},
{0xD516, 0x01},
{0xD517, 0xD4},
{0xD518, 0x02},	
{0xD519, 0x07},
{0xD51A, 0x02},
{0xD51B, 0x08},
{0xD51C, 0x02},
{0xD51D, 0x34},
{0xD51E, 0x02},
{0xD51F, 0x5F}, //
{0xD520, 0x02},
{0xD521, 0x78},
{0xD522, 0x02},
{0xD523, 0x94},
{0xD524, 0x02},
{0xD525, 0xA6},
{0xD526, 0x02},
{0xD527, 0xBB},
{0xD528, 0x02}, 
{0xD529, 0xCA},
{0xD52A, 0x02},
{0xD52B, 0xDB},
{0xD52C, 0x02},
{0xD52D, 0xE8},
{0xD52E, 0x02},
{0xD52F, 0xF9}, //
{0xD530, 0x03}, 
{0xD531, 0x1F},
{0xD532, 0x03},
{0xD533, 0x7F},
		 
{0xD600, 0x00},
{0xD601, 0x2D},
{0xD602, 0x00},
{0xD603, 0x2E},
{0xD604, 0x00}, 
{0xD605, 0x32},
{0xD606, 0x00},
{0xD607, 0x44},
{0xD608, 0x00},
{0xD609, 0x53},
{0xD60A, 0x00},
{0xD60B, 0x88},
{0xD60C, 0x00},
{0xD60D, 0xB6},
{0xD60E, 0x00},
{0xD60F, 0xF3},	//
{0xD610, 0x01},
{0xD611, 0x22},
{0xD612, 0x01},
{0xD613, 0x64},
{0xD614, 0x01},
{0xD615, 0x92},
{0xD616, 0x01},
{0xD617, 0xD4},
{0xD618, 0x02},	
{0xD619, 0x07},
{0xD61A, 0x02},
{0xD61B, 0x08},
{0xD61C, 0x02},
{0xD61D, 0x34},
{0xD61E, 0x02},
{0xD61F, 0x5F}, //
{0xD620, 0x02},
{0xD621, 0x78},
{0xD622, 0x02},
{0xD623, 0x94},
{0xD624, 0x02},
{0xD625, 0xA6},
{0xD626, 0x02},
{0xD627, 0xBB},
{0xD628, 0x02}, 
{0xD629, 0xCA},
{0xD62A, 0x02},
{0xD62B, 0xDB},
{0xD62C, 0x02},
{0xD62D, 0xE8},
{0xD62E, 0x02},
{0xD62F, 0xF9}, //
{0xD630, 0x03}, 
{0xD631, 0x1F},
{0xD632, 0x03},
{0xD633, 0x7F},

            // LV2 page 0 enable
            {0xf000, 0x55},
            {0xf001, 0xaa},
            {0xf002, 0x52},
            {0xf003, 0x08},
            {0xf004, 0x00},
            // display control
            {0xb100, 0xcc},
            {0xb101, 0x00},
            // select IPS (0x6b) vs TN (0x50)
            {0xb500, 0x50},
            // source hold time
            {0xb600, 0x05},
            // gate eq
            {0xb700, 0x70},
            {0xb701, 0x70},
            // source eq control (mode 2)
            {0xb800, 0x01},
            {0xb801, 0x03},
            {0xb802, 0x03},
            {0xb803, 0x03},
            // inversion mode
            {0xbc00, 0x02},
            {0xbc01, 0x00},
            {0xbc02, 0x00},
            // timing control
            {0xc900, 0xd0},
            {0xc901, 0x02},
            {0xc902, 0x50},
            {0xc903, 0x50},
            {0xc904, 0x50},

            // generate internal clock
            {0xb300, 0x01},
            // use pixel clock
            //{0xb300, 0x00},

            // tearing effect only vblank
            {0x3500, 0x00},
            // data format: 24 bits
            {0x3a00, 0x77},
            // memory data access control
            {0x3600, 0x00},
            // SRAM data input via PCLK/RGB bus
            //{0x4a00, 0x00},
            {0x4a00, 0x01},
        },
    },
};


/**
 * @brief Initialize the display controller
 *
 * Determines the type of display, then applies the appropriate configuration sequence to the
 * controller to enable its output.
 */
Nt35510::Nt35510(const std::filesystem::path &spidevPath,
        const std::shared_ptr<Drivers::Gpio::GpioChip> &gpioChip, const size_t gpioLine) :
    devPath(spidevPath), gpioChip(gpioChip), gpioLine(gpioLine) {
    int err;

    /*
     * Determine the panel type
     *
     * Currently, we only have a single supported panel, so that's the type we always select in
     * here. In the future, we could get that information passed in (as a string or UUID) from
     * the constructor.
     */

    // test opening the SPI device
    PLOG_DEBUG << "Opening display at " << spidevPath.native();
    this->openDevice();

    //auto chip = gpiod_chip_open_by_name("gpiochip0");
    auto chip = gpiod_chip_open_by_name("gpiochip2");
    if(!chip) {
        throw std::runtime_error("failed to open gpiochip");
    }

    //this->devCs = gpiod_chip_get_line(chip, 15);
    this->devCs = gpiod_chip_get_line(chip, 8);
    if(!this->devCs) {
        throw std::runtime_error("failed to get gpio for /cs");
    }

    err = gpiod_line_request_output(this->devCs, "balls", 1);
    if(err) {
        PLOG_FATAL << "failed to set gpio line as output";
    }

    uint8_t balls;
    this->regRead(0x0c00, balls);
    PLOG_INFO << "pixel format pre reset: " << fmt::format("{:02x}", balls);

    // configure the reset line and assert it
    using PinMode = Drivers::Gpio::GpioChip;
    this->gpioChip->configurePin(this->gpioLine, PinMode::OutputPushPull);

    this->toggleReset();

    this->regRead(0x0c00, balls);
    PLOG_INFO << "pixel format post reset: " << fmt::format("{:02x}", balls);

    // read display id
    std::array<uint8_t, 3> displayId{};

    this->regRead(0x0400, displayId[0]);
    this->regRead(0x0401, displayId[1]);
    this->regRead(0x0402, displayId[2]);
    PLOG_INFO << "Display id 1: " << fmt::format("{:02x} {:02x} {:02x}", displayId[0], displayId[1],
            displayId[2]);

    this->regRead(0xDA00, displayId[0]);
    this->regRead(0xDB00, displayId[1]);
    this->regRead(0xDC00, displayId[2]);
    PLOG_INFO << "Display id 2: " << fmt::format("{:02x} {:02x} {:02x}", displayId[0], displayId[1],
            displayId[2]);

    // apply initialization sequence and enable display
    this->runInitSequence(gPanelData[0]);
    this->enableDisplay();

    // read display signal mode
    this->regRead(0x0a00, displayId[0]);
    PLOG_INFO << "power mode: " << fmt::format("{:02x}", displayId[0]);

    this->regRead(0x0b00, displayId[0]);
    PLOG_INFO << "DMADCTL: " << fmt::format("{:02x}", displayId[0]);

    this->regRead(0x0c00, displayId[0]);
    PLOG_INFO << "pixel format: " << fmt::format("{:02x}", displayId[0]);

    this->regRead(0x0d00, displayId[0]);
    PLOG_INFO << "display mode: " << fmt::format("{:02x}", displayId[0]);

    this->regRead(0x0e00, displayId[0]);
    PLOG_INFO << "signal mode: " << fmt::format("{:02x}", displayId[0]);

    this->regRead(0x0f00, displayId[0]);
    PLOG_INFO << "diagnostic state: " << fmt::format("{:02x}", displayId[0]);
}

/**
 * @brief Open the SPI device
 */
void Nt35510::openDevice() {
    int err;

    this->dev = open(this->devPath.native().c_str(), O_RDWR);
    if(this->dev == -1) {
        throw std::system_error(errno, std::generic_category(), "Nt35510: open spidev");
    }

#ifdef __linux__
    err = ioctl(this->dev, SPI_IOC_WR_MAX_SPEED_HZ, &this->busSpeed);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "Nt35510: set spidev speed");
    }

    // display is configured: rising trigger, clock starts low
    uint8_t mode{SPI_MODE_0};
    err = ioctl(this->dev, SPI_IOC_WR_MODE, &mode);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "Nt35510: set spidev mode");
    }

    // 8 bits per word
    uint8_t bpw{0};
    err = ioctl(this->dev, SPI_IOC_WR_BITS_PER_WORD, &bpw);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "Nt35510: set spidev bits per word");
    }
#endif
}

void Nt35510::closeDevice() {
    if(this->dev != -1) {
        close(this->dev);
        this->dev = -1;
    }
}

/**
 * @brief Shut down the display controller
 *
 * Place the display controller in idle mode (which reduces its refresh rate and color fidelity)
 * and then release all resources.
 */
Nt35510::~Nt35510() {
    this->closeDevice();
}

/**
 * @brief Run the initialization sequence for the specified panel
 *
 * This executes all register writes specified in the initialization sequence for this panel.
 */
void Nt35510::runInitSequence(const PanelData &data) {
    for(const auto &reg : data.initRegs) {
        this->regWrite(reg.first, reg.second);
    }
}

/**
 * @brief Enable the display
 */
void Nt35510::enableDisplay() {
    // start up (turn off sleep mode)
    this->regWrite(0x1100);
    usleep(125 * 1000);

    // display on
    this->regWrite(0x2900);
}


/**
 * @brief Toggle the display's reset line
 *
 * This is an active low IO attached to the GPIO specified during the construction of the object;
 * we'll pulse it low for ~500ms to give the controller time to complete its internal
 * initialization.
 */
void Nt35510::toggleReset() {
    // de-assert reset (1)
    this->gpioChip->setPinState(this->gpioLine, true);
    usleep(150 * 1000);

    // assert reset (0)
    this->gpioChip->setPinState(this->gpioLine, false);
    usleep(150 * 1000);

    // de-assert reset (1)
    this->gpioChip->setPinState(this->gpioLine, true);
    usleep(150 * 1000);
}

/**
 * @brief Write to device
 *
 * This writes a command (or register address) as the first 16-bit quantity, then an optional
 * byte of data.
 */
void Nt35510::regWrite(const uint16_t address, std::optional<const uint8_t> value) {
    if(value) {
        PLOG_DEBUG << "<< " << fmt::format("reg {:04x} = {:02x}", address, *value);
    } else {
        PLOG_DEBUG << "<< " << fmt::format("cmd {:04x}", address);
    }

    // write the register address
    this->writeWord(false, false, true, ((address & 0xFF00) >> 8));
    // PLOG_VERBOSE << "write0: " << fmt::format("{:02x} {:02x}", txBuffers[0][0], txBuffers[0][1]);

    this->writeWord(false, false, false, (address & 0x00FF));
    // PLOG_VERBOSE << "write1: " << fmt::format("{:02x} {:02x}", txBuffers[1][0], txBuffers[1][1]);

    // write data
    if(value) {
        this->writeWord(false, true, false, *value);
    }
}


/**
 * @brief Reads a register from the device
 *
 * @param address Register address to read from
 * @param outValue Variable to receive the byte content of the register
 */
void Nt35510::regRead(const uint16_t address, uint8_t &outValue) {
    PLOG_DEBUG << ">> " << fmt::format("reg {:04x}", address);

    // write the register address
    this->writeWord(false, false, true, ((address & 0xFF00) >> 8));
    // PLOG_VERBOSE << "write0: " << fmt::format("{:02x} {:02x}", txBuffers[0][0], txBuffers[0][1]);

    this->writeWord(false, false, false, (address & 0x00FF));
    // PLOG_VERBOSE << "write1: " << fmt::format("{:02x} {:02x}", txBuffers[1][0], txBuffers[1][1]);

    // read out the byte
    const auto temp = this->writeWord(true, true, false);
    outValue = temp;
}

/**
 * @brief Write a word to the display controller
 *
 * This performs a two byte SPI transaction (manging the chip selects manually) to the display.
 *
 * @return Data received during the second byte phase
 */
uint8_t Nt35510::writeWord(const bool rw, const bool dc, const bool upper, const uint8_t payload) {
    int err;
    std::array<uint8_t, 2> rxBuffer, txBuffer;

    std::fill(rxBuffer.begin(), rxBuffer.end(), 0);

    // prepare tx buffer
    txBuffer[0] = (rw ? (1 << 7) : 0) | (dc ? (1 << 6) : 0) | (upper ? (1 << 5) : 0);
    txBuffer[1] = payload;

    /*
    // prepare spi transaction
    struct spi_ioc_transfer txn;
    memset(&txn, 0, sizeof(txn));

    txn.len = 2;
    txn.tx_buf = reinterpret_cast<uintptr_t>(txBuffer.data());
    txn.rx_buf = reinterpret_cast<uintptr_t>(rxBuffer.data());
*/

    // assert CS
    gpiod_line_set_value(this->devCs, 0);

    // do txn
    //err = ioctl(this->dev, SPI_IOC_MESSAGE(1), &txn);
    // write two bytes
    if(!rw) {
        err = write(this->dev, txBuffer.data(), 2);
        if(err == -1) {
            throw std::system_error(errno, std::generic_category(),
                fmt::format("Nt35510: tx word ({} {} {} {:02x})", rw ? "write" : "read",
                    dc ? "data" : "command", upper ? "upper" : "lower", payload));
        }
    } else {
        // write byte (command)
        err = write(this->dev, txBuffer.data(), 1);
        if(err == -1) {
            throw std::system_error(errno, std::generic_category(),
                fmt::format("Nt35510: tx word 1/1 ({} {} {} {:02x})", rw ? "write" : "read",
                    dc ? "data" : "command", upper ? "upper" : "lower", payload));
        }

        // read byte
        err = read(this->dev, rxBuffer.data(), 1);
        if(err == -1) {
            throw std::system_error(errno, std::generic_category(),
                fmt::format("Nt35510: tx word 2/2 ({} {} {} {:02x})", rw ? "write" : "read",
                    dc ? "data" : "command", upper ? "upper" : "lower", payload));
        }
    }


    // deassert CS, and handle error of transaction
    gpiod_line_set_value(this->devCs, 1);

    if(err == -1) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("Nt35510: tx word ({} {} {} {:02x})", rw ? "write" : "read",
                    dc ? "data" : "command", upper ? "upper" : "lower", payload));
    }

    // return the received byte
    return rxBuffer[0];
}


