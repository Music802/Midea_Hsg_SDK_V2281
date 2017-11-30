#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "ite/ite_usbex.h"
#include "bootloader.h"
#include "config.h"
#include "ite/itp.h"
#include "ite/itu.h"
#include "itu_private.h"

static bool blLcdOn = false;
static bool blLcdConsoleOn = false;
static bool stop_led = false;

#if defined (CFG_ENABLE_UART_CLI)
static bool boot = false;
static bool bootstate = false;
char tftppara[128] = "tftp://192.168.1.20/doorbell_indoor2.bin";

#if defined (CFG_UART0_ENABLE) && defined(CFG_DBG_UART0)
#define UART_PORT ":uart0"
#elif defined(CFG_UART1_ENABLE) && defined(CFG_DBG_UART1)
#define UART_PORT ":uart1"
#elif defined(CFG_UART2_ENABLE) && defined(CFG_DBG_UART2)
#define UART_PORT ":uart2"
#elif defined(CFG_UART3_ENABLE) && defined(CFG_DBG_UART3)
#define UART_PORT ":uart3"
#endif

#endif //end of #if defined (CFG_ENABLE_UART_CLI)

#ifdef CFG_MSC_ENABLE
static bool usbInited = false;
#endif

#define CLK_PORT               CFG_LCD_SPI_CLK_GPIO
#define CS_PORT                 CFG_LCD_SPI_CS_GPIO
#define DATA_PORT              CFG_LCD_SPI_DATA_GPIO
#define LCM_RST                 CFG_LCD_LCM_RESET
#define LCD_POWER_PORT          CFG_LCD_LCM_POWER
#define BACKLIGHT_ENABLE_PORT   CFG_LCD_LCM_BACKLIGHT_EN

#define CLK_SET()       ithGpioSet(CLK_PORT)
#define CLK_CLR()       ithGpioClear(CLK_PORT)
#define CS_SET()        ithGpioSet(CS_PORT)
#define CS_CLR()        ithGpioClear(CS_PORT)
#define TXD_SET()       ithGpioSet(DATA_PORT)
#define TXD_CLR()       ithGpioClear(DATA_PORT)
#define RXD_GET()       ithGpioGet(DATA_PORT)

volatile int delay_cnt = 0;

void delay(int count)
{
    int i;
    ithEnterCritical();
    for (i = 0; i < count; i++)
        i++;
    ithExitCritical();
}

void delay_ms(int cout)
{ 
    usleep(350);
}

void spi_gpio_init(void)
{
    ithGpioSet(CLK_PORT);
    ithGpioSetMode(CLK_PORT, ITH_GPIO_MODE0);
    ithGpioSetOut(CLK_PORT);

    ithGpioSet(CS_PORT);
    ithGpioSetMode(CS_PORT, ITH_GPIO_MODE0);
    ithGpioSetOut(CS_PORT);

    ithGpioSet(DATA_PORT);
    ithGpioSetMode(DATA_PORT, ITH_GPIO_MODE0);
    ithGpioSetOut(DATA_PORT);

    ithGpioSet(LCD_POWER_PORT);
    ithGpioSetMode(LCD_POWER_PORT, ITH_GPIO_MODE0);
    ithGpioSetOut(LCD_POWER_PORT);

    usleep(5 * 1000);

    ithGpioSet(LCM_RST);
    ithGpioSetMode(LCM_RST, ITH_GPIO_MODE0);
    ithGpioSetOut(LCM_RST);

//#ifdef CFG_BOARD_1954_B
    ithGpioSetMode(BACKLIGHT_ENABLE_PORT, ITH_GPIO_MODE0);
    ithGpioSetOut(BACKLIGHT_ENABLE_PORT);
    ithGpioClear(BACKLIGHT_ENABLE_PORT);
//#endif
    
    CLK_CLR();
    CS_SET();
}
#ifdef CFG_SPI_LCD_800X480_RM68180
static void spi_send_byte(uint8_t raw, bool rw, bool dc, bool hl)
{
	unsigned int i;
	uint16_t data = 0;
	if (rw)
		data |= 0x8000;
	if (dc)
		data |= 0x4000;
	if (hl)
		data |= 0x2000;
	data |= raw;

	for (i = 0; i < 16; i++)
	{
		CLK_CLR();
		delay(10);
		if (data & 0x8000) {
			TXD_SET();
		}
		else {
			TXD_CLR();
		}
		delay(10);
		CLK_SET();
		delay(10);
		data <<= 1;
	}
	CLK_CLR();
}

static uint8_t spi_read_byte()
{
	unsigned int i;
	uint8_t head = 0xC0;
	uint8_t data = 0;
	for (i = 0; i < 8; i++)
	{
		CLK_CLR();
		delay(10);
		if (head & 0x80) {
			TXD_SET();
		}
		else {
			TXD_CLR();
		}
		delay(10);
		CLK_SET();
		delay(10);
		head <<= 1;
	}
	ithGpioClear(DATA_PORT);
	ithGpioSetIn(DATA_PORT);
	delay(5);
	for (i = 0; i < 8; i++)
	{
		CLK_CLR();
		delay(20);
		CLK_SET();
		delay(10);
		if (RXD_GET()) {
			data |= 0x01;
		}
		data <<= 1;
	}
	ithGpioSetOut(DATA_PORT);
	CLK_CLR();
	return data;
}

static void spi_send_cmd(uint16_t cmd)
{
	uint8_t raw = cmd >> 8;
	spi_send_byte(raw, false, false, true);
	raw = cmd & 0xff;
	spi_send_byte(raw, false, false, false);
}

static void spi_send_para(uint8_t para)
{
	spi_send_byte(para, false, true, false);
}

static void spi_write_reg(uint16_t add, uint8_t val)
{
	CS_CLR();
	delay(10);
	spi_send_cmd(add);
	spi_send_para(val);
	CS_SET();
	delay(2);
}

static void spi_write_single(uint16_t cmd)
{
	CS_CLR();
	delay(10);
	spi_send_cmd(cmd);
	CS_SET();
	delay(2);
}

volatile uint8_t id;
void test_lcm_rm68180()
{
	spi_gpio_init();
	//Select Command Page '1'
	spi_write_reg(0xF000, 0x55);
	spi_write_reg(0xF001, 0xAA);
	spi_write_reg(0xF002, 0x52);
	spi_write_reg(0xF003, 0x08);
	spi_write_reg(0xF004, 0x01);

	// Setting AVDD Voltage
	spi_write_reg(0xB000, 0x05);   //AVDD=6V
	spi_write_reg(0xB001, 0x05);
	spi_write_reg(0xB002, 0x05);

	// Setting AVEE Voltage
	spi_write_reg(0xB100, 0x05);  //AVEE=-6V
	spi_write_reg(0xB101, 0x05);
	spi_write_reg(0xB102, 0x05);

	// Setting VCL Voltage
	spi_write_reg(0xB200, 0x03);
	spi_write_reg(0xB201, 0x03);
	spi_write_reg(0xB202, 0x03);

	// Setting VGH Voltag
	spi_write_reg(0xB300, 0x0A);
	spi_write_reg(0xB301, 0x0A);
	spi_write_reg(0xB302, 0x0A);

	// Setting VRGH Voltag
	spi_write_reg(0xB400, 0x2D);
	spi_write_reg(0xB401, 0x2D);
	spi_write_reg(0xB402, 0x2D);

	// Setting VGL_REG Voltag
	spi_write_reg(0xB500, 0x08);
	spi_write_reg(0xB501, 0x08);
	spi_write_reg(0xB502, 0x08);

	// AVDD Ratio setting
	spi_write_reg(0xB600, 0x34);
	spi_write_reg(0xB601, 0x34);
	spi_write_reg(0xB602, 0x34);

	// AVEE Ratio setting
	spi_write_reg(0xB700, 0x24);
	spi_write_reg(0xB701, 0x24);
	spi_write_reg(0xB702, 0x24);

	// Power Control for VCL
	spi_write_reg(0xB800, 0x24);  //VCL=-1 x VCI
	spi_write_reg(0xB801, 0x24);
	spi_write_reg(0xB802, 0x24);

	// Setting VGH Ratio
	spi_write_reg(0xB900, 0x34);
	spi_write_reg(0xB901, 0x34);
	spi_write_reg(0xB902, 0x34);

	// VGLX Ratio
	spi_write_reg(0xBA00, 0x14);
	spi_write_reg(0xBA01, 0x14);
	spi_write_reg(0xBA02, 0x14);

	// Setting VGMP and VGSP Voltag
	spi_write_reg(0xBC00, 0x00);
	spi_write_reg(0xBC01, 0x68);  //VGMP=5.3V
	spi_write_reg(0xBC02, 0x00);

	// Setting VGMN and VGSN Voltage
	spi_write_reg(0xBD00, 0x00);
	spi_write_reg(0xBD01, 0x68);  //VGMN=-5.3V
	spi_write_reg(0xBD02, 0x00);

	// Setting VCOM Offset Voltage
	spi_write_reg(0xBE00, 0x00);
	spi_write_reg(0xBE01, 0x5F);  //VCOM=-1.38V

	// Control VGH booster voltage rang
	spi_write_reg(0xBF00, 0x01);

	//Gamma Setting
	spi_write_reg(0xD100, 0x00);
	spi_write_reg(0xD101, 0x00);
	spi_write_reg(0xD102, 0x00);
	spi_write_reg(0xD103, 0x11);
	spi_write_reg(0xD104, 0x00);
	spi_write_reg(0xD105, 0x2F);
	spi_write_reg(0xD106, 0x00);
	spi_write_reg(0xD107, 0x47);
	spi_write_reg(0xD108, 0x00);
	spi_write_reg(0xD109, 0x5C);
	spi_write_reg(0xD10A, 0x00);
	spi_write_reg(0xD10B, 0x80);
	spi_write_reg(0xD10C, 0x00);
	spi_write_reg(0xD10D, 0x9D);
	spi_write_reg(0xD10E, 0x00);
	spi_write_reg(0xD10F, 0xCF);
	spi_write_reg(0xD110, 0x00);
	spi_write_reg(0xD111, 0xF8);
	spi_write_reg(0xD112, 0x01);
	spi_write_reg(0xD113, 0x3B);
	spi_write_reg(0xD114, 0x01);
	spi_write_reg(0xD115, 0x70);
	spi_write_reg(0xD116, 0x01);
	spi_write_reg(0xD117, 0xC3);
	spi_write_reg(0xD118, 0x02);
	spi_write_reg(0xD119, 0x08);
	spi_write_reg(0xD11A, 0x02);
	spi_write_reg(0xD11B, 0x0A);
	spi_write_reg(0xD11C, 0x02);
	spi_write_reg(0xD11D, 0x47);
	spi_write_reg(0xD11E, 0x02);
	spi_write_reg(0xD11F, 0x89);
	spi_write_reg(0xD120, 0x02);
	spi_write_reg(0xD121, 0xB1);
	spi_write_reg(0xD122, 0x02);
	spi_write_reg(0xD123, 0xE6);
	spi_write_reg(0xD124, 0x03);
	spi_write_reg(0xD125, 0x09);
	spi_write_reg(0xD126, 0x03);
	spi_write_reg(0xD127, 0x36);
	spi_write_reg(0xD128, 0x03);
	spi_write_reg(0xD129, 0x54);
	spi_write_reg(0xD12A, 0x03);
	spi_write_reg(0xD12B, 0x7A);
	spi_write_reg(0xD12C, 0x03);
	spi_write_reg(0xD12D, 0x92);
	spi_write_reg(0xD12E, 0x03);
	spi_write_reg(0xD12F, 0xAF);
	spi_write_reg(0xD130, 0x03);
	spi_write_reg(0xD131, 0xBC);
	spi_write_reg(0xD132, 0x03);
	spi_write_reg(0xD133, 0xFF);

	spi_write_reg(0xD200, 0x00);
	spi_write_reg(0xD201, 0x00);
	spi_write_reg(0xD202, 0x00);
	spi_write_reg(0xD203, 0x11);
	spi_write_reg(0xD204, 0x00);
	spi_write_reg(0xD205, 0x2F);
	spi_write_reg(0xD206, 0x00);
	spi_write_reg(0xD207, 0x47);
	spi_write_reg(0xD208, 0x00);
	spi_write_reg(0xD209, 0x5C);
	spi_write_reg(0xD20A, 0x00);
	spi_write_reg(0xD20B, 0x80);
	spi_write_reg(0xD20C, 0x00);
	spi_write_reg(0xD20D, 0x9D);
	spi_write_reg(0xD20E, 0x00);
	spi_write_reg(0xD20F, 0xCF);
	spi_write_reg(0xD210, 0x00);
	spi_write_reg(0xD211, 0xF8);
	spi_write_reg(0xD212, 0x01);
	spi_write_reg(0xD213, 0x3B);
	spi_write_reg(0xD214, 0x01);
	spi_write_reg(0xD215, 0x70);
	spi_write_reg(0xD216, 0x01);
	spi_write_reg(0xD217, 0xC3);
	spi_write_reg(0xD218, 0x02);
	spi_write_reg(0xD219, 0x08);
	spi_write_reg(0xD21A, 0x02);
	spi_write_reg(0xD21B, 0x0A);
	spi_write_reg(0xD21C, 0x02);
	spi_write_reg(0xD21D, 0x47);
	spi_write_reg(0xD21E, 0x02);
	spi_write_reg(0xD21F, 0x89);
	spi_write_reg(0xD220, 0x02);
	spi_write_reg(0xD221, 0xB1);
	spi_write_reg(0xD222, 0x02);
	spi_write_reg(0xD223, 0xE6);
	spi_write_reg(0xD224, 0x03);
	spi_write_reg(0xD225, 0x09);
	spi_write_reg(0xD226, 0x03);
	spi_write_reg(0xD227, 0x36);
	spi_write_reg(0xD228, 0x03);
	spi_write_reg(0xD229, 0x54);
	spi_write_reg(0xD22A, 0x03);
	spi_write_reg(0xD22B, 0x7A);
	spi_write_reg(0xD22C, 0x03);
	spi_write_reg(0xD22D, 0x92);
	spi_write_reg(0xD22E, 0x03);
	spi_write_reg(0xD22F, 0xAF);
	spi_write_reg(0xD230, 0x03);
	spi_write_reg(0xD231, 0xBC);
	spi_write_reg(0xD232, 0x03);
	spi_write_reg(0xD233, 0xFF);

	spi_write_reg(0xD300, 0x00);
	spi_write_reg(0xD301, 0x00);
	spi_write_reg(0xD302, 0x00);
	spi_write_reg(0xD303, 0x11);
	spi_write_reg(0xD304, 0x00);
	spi_write_reg(0xD305, 0x2F);
	spi_write_reg(0xD306, 0x00);
	spi_write_reg(0xD307, 0x47);
	spi_write_reg(0xD308, 0x00);
	spi_write_reg(0xD309, 0x5C);
	spi_write_reg(0xD30A, 0x00);
	spi_write_reg(0xD30B, 0x80);
	spi_write_reg(0xD30C, 0x00);
	spi_write_reg(0xD30D, 0x9D);
	spi_write_reg(0xD30E, 0x00);
	spi_write_reg(0xD30F, 0xCF);
	spi_write_reg(0xD310, 0x00);
	spi_write_reg(0xD311, 0xF8);
	spi_write_reg(0xD312, 0x01);
	spi_write_reg(0xD313, 0x3B);
	spi_write_reg(0xD314, 0x01);
	spi_write_reg(0xD315, 0x70);
	spi_write_reg(0xD316, 0x01);
	spi_write_reg(0xD317, 0xC3);
	spi_write_reg(0xD318, 0x02);
	spi_write_reg(0xD319, 0x08);
	spi_write_reg(0xD31A, 0x02);
	spi_write_reg(0xD31B, 0x0A);
	spi_write_reg(0xD31C, 0x02);
	spi_write_reg(0xD31D, 0x47);
	spi_write_reg(0xD31E, 0x02);
	spi_write_reg(0xD31F, 0x89);
	spi_write_reg(0xD320, 0x02);
	spi_write_reg(0xD321, 0xB1);
	spi_write_reg(0xD322, 0x02);
	spi_write_reg(0xD323, 0xE6);
	spi_write_reg(0xD324, 0x03);
	spi_write_reg(0xD325, 0x09);
	spi_write_reg(0xD326, 0x03);
	spi_write_reg(0xD327, 0x36);
	spi_write_reg(0xD328, 0x03);
	spi_write_reg(0xD329, 0x54);
	spi_write_reg(0xD32A, 0x03);
	spi_write_reg(0xD32B, 0x7A);
	spi_write_reg(0xD32C, 0x03);
	spi_write_reg(0xD32D, 0x92);
	spi_write_reg(0xD32E, 0x03);
	spi_write_reg(0xD32F, 0xAF);
	spi_write_reg(0xD330, 0x03);
	spi_write_reg(0xD331, 0xBC);
	spi_write_reg(0xD332, 0x03);
	spi_write_reg(0xD333, 0xFF);

	spi_write_reg(0xD400, 0x00);
	spi_write_reg(0xD401, 0x00);
	spi_write_reg(0xD402, 0x00);
	spi_write_reg(0xD403, 0x11);
	spi_write_reg(0xD404, 0x00);
	spi_write_reg(0xD405, 0x2F);
	spi_write_reg(0xD406, 0x00);
	spi_write_reg(0xD407, 0x47);
	spi_write_reg(0xD408, 0x00);
	spi_write_reg(0xD409, 0x5C);
	spi_write_reg(0xD40A, 0x00);
	spi_write_reg(0xD40B, 0x80);
	spi_write_reg(0xD40C, 0x00);
	spi_write_reg(0xD40D, 0x9D);
	spi_write_reg(0xD40E, 0x00);
	spi_write_reg(0xD40F, 0xCF);
	spi_write_reg(0xD410, 0x00);
	spi_write_reg(0xD411, 0xF8);
	spi_write_reg(0xD412, 0x01);
	spi_write_reg(0xD413, 0x3B);
	spi_write_reg(0xD414, 0x01);
	spi_write_reg(0xD415, 0x70);
	spi_write_reg(0xD416, 0x01);
	spi_write_reg(0xD417, 0xC3);
	spi_write_reg(0xD418, 0x02);
	spi_write_reg(0xD419, 0x08);
	spi_write_reg(0xD41A, 0x02);
	spi_write_reg(0xD41B, 0x0A);
	spi_write_reg(0xD41C, 0x02);
	spi_write_reg(0xD41D, 0x47);
	spi_write_reg(0xD41E, 0x02);
	spi_write_reg(0xD41F, 0x89);
	spi_write_reg(0xD420, 0x02);
	spi_write_reg(0xD421, 0xB1);
	spi_write_reg(0xD422, 0x02);
	spi_write_reg(0xD423, 0xE6);
	spi_write_reg(0xD424, 0x03);
	spi_write_reg(0xD425, 0x09);
	spi_write_reg(0xD426, 0x03);
	spi_write_reg(0xD427, 0x36);
	spi_write_reg(0xD428, 0x03);
	spi_write_reg(0xD429, 0x54);
	spi_write_reg(0xD42A, 0x03);
	spi_write_reg(0xD42B, 0x7A);
	spi_write_reg(0xD42C, 0x03);
	spi_write_reg(0xD42D, 0x92);
	spi_write_reg(0xD42E, 0x03);
	spi_write_reg(0xD42F, 0xAF);
	spi_write_reg(0xD430, 0x03);
	spi_write_reg(0xD431, 0xBC);
	spi_write_reg(0xD432, 0x03);
	spi_write_reg(0xD433, 0xFF);

	spi_write_reg(0xD500, 0x00);
	spi_write_reg(0xD501, 0x00);
	spi_write_reg(0xD502, 0x00);
	spi_write_reg(0xD503, 0x11);
	spi_write_reg(0xD504, 0x00);
	spi_write_reg(0xD505, 0x2F);
	spi_write_reg(0xD506, 0x00);
	spi_write_reg(0xD507, 0x47);
	spi_write_reg(0xD508, 0x00);
	spi_write_reg(0xD509, 0x5C);
	spi_write_reg(0xD50A, 0x00);
	spi_write_reg(0xD50B, 0x80);
	spi_write_reg(0xD50C, 0x00);
	spi_write_reg(0xD50D, 0x9D);
	spi_write_reg(0xD50E, 0x00);
	spi_write_reg(0xD50F, 0xCF);
	spi_write_reg(0xD510, 0x00);
	spi_write_reg(0xD511, 0xF8);
	spi_write_reg(0xD512, 0x01);
	spi_write_reg(0xD513, 0x3B);
	spi_write_reg(0xD514, 0x01);
	spi_write_reg(0xD515, 0x70);
	spi_write_reg(0xD516, 0x01);
	spi_write_reg(0xD517, 0xC3);
	spi_write_reg(0xD518, 0x02);
	spi_write_reg(0xD519, 0x08);
	spi_write_reg(0xD51A, 0x02);
	spi_write_reg(0xD51B, 0x0A);
	spi_write_reg(0xD51C, 0x02);
	spi_write_reg(0xD51D, 0x47);
	spi_write_reg(0xD51E, 0x02);
	spi_write_reg(0xD51F, 0x89);
	spi_write_reg(0xD520, 0x02);
	spi_write_reg(0xD521, 0xB1);
	spi_write_reg(0xD522, 0x02);
	spi_write_reg(0xD523, 0xE6);
	spi_write_reg(0xD524, 0x03);
	spi_write_reg(0xD525, 0x09);
	spi_write_reg(0xD526, 0x03);
	spi_write_reg(0xD527, 0x36);
	spi_write_reg(0xD528, 0x03);
	spi_write_reg(0xD529, 0x54);
	spi_write_reg(0xD52A, 0x03);
	spi_write_reg(0xD52B, 0x7A);
	spi_write_reg(0xD52C, 0x03);
	spi_write_reg(0xD52D, 0x92);
	spi_write_reg(0xD52E, 0x03);
	spi_write_reg(0xD52F, 0xAF);
	spi_write_reg(0xD530, 0x03);
	spi_write_reg(0xD531, 0xBC);
	spi_write_reg(0xD532, 0x03);
	spi_write_reg(0xD533, 0xFF);

	spi_write_reg(0xD600, 0x00);
	spi_write_reg(0xD601, 0x00);
	spi_write_reg(0xD602, 0x00);
	spi_write_reg(0xD603, 0x11);
	spi_write_reg(0xD604, 0x00);
	spi_write_reg(0xD605, 0x2F);
	spi_write_reg(0xD606, 0x00);
	spi_write_reg(0xD607, 0x47);
	spi_write_reg(0xD608, 0x00);
	spi_write_reg(0xD609, 0x5C);
	spi_write_reg(0xD60A, 0x00);
	spi_write_reg(0xD60B, 0x80);
	spi_write_reg(0xD60C, 0x00);
	spi_write_reg(0xD60D, 0x9D);
	spi_write_reg(0xD60E, 0x00);
	spi_write_reg(0xD60F, 0xCF);
	spi_write_reg(0xD610, 0x00);
	spi_write_reg(0xD611, 0xF8);
	spi_write_reg(0xD612, 0x01);
	spi_write_reg(0xD613, 0x3B);
	spi_write_reg(0xD614, 0x01);
	spi_write_reg(0xD615, 0x70);
	spi_write_reg(0xD616, 0x01);
	spi_write_reg(0xD617, 0xC3);
	spi_write_reg(0xD618, 0x02);
	spi_write_reg(0xD619, 0x08);
	spi_write_reg(0xD61A, 0x02);
	spi_write_reg(0xD61B, 0x0A);
	spi_write_reg(0xD61C, 0x02);
	spi_write_reg(0xD61D, 0x47);
	spi_write_reg(0xD61E, 0x02);
	spi_write_reg(0xD61F, 0x89);
	spi_write_reg(0xD620, 0x02);
	spi_write_reg(0xD621, 0xB1);
	spi_write_reg(0xD622, 0x02);
	spi_write_reg(0xD623, 0xE6);
	spi_write_reg(0xD624, 0x03);
	spi_write_reg(0xD625, 0x09);
	spi_write_reg(0xD626, 0x03);
	spi_write_reg(0xD627, 0x36);
	spi_write_reg(0xD628, 0x03);
	spi_write_reg(0xD629, 0x54);
	spi_write_reg(0xD62A, 0x03);
	spi_write_reg(0xD62B, 0x7A);
	spi_write_reg(0xD62C, 0x03);
	spi_write_reg(0xD62D, 0x92);
	spi_write_reg(0xD62E, 0x03);
	spi_write_reg(0xD62F, 0xAF);
	spi_write_reg(0xD630, 0x03);
	spi_write_reg(0xD631, 0xBC);
	spi_write_reg(0xD632, 0x03);
	spi_write_reg(0xD633, 0xFF);

	// Select Command Page '0'
	spi_write_reg(0xF000, 0x55);
	spi_write_reg(0xF001, 0xAA);
	spi_write_reg(0xF002, 0x52);
	spi_write_reg(0xF003, 0x08);
	spi_write_reg(0xF004, 0x00);

	// Display Option Control
	spi_write_reg(0xB100, 0x7C);

	// Vivid Color
	spi_write_reg(0xB400, 0x10);

	// Vivid Color
	spi_write_reg(0xB600, 0x08);

	// EQ Control Function for Source Driver
	spi_write_reg(0xB800, 0x00);
	spi_write_reg(0xB801, 0x00);
	spi_write_reg(0xB802, 0x00);
	spi_write_reg(0xB803, 0x00);

	// Inversion Driver Control
	spi_write_reg(0xBC00, 0x00);
	spi_write_reg(0xBC01, 0x00);
	spi_write_reg(0xBC02, 0x00);

	// Vivid Color
	spi_write_reg(0xCC00, 0x03);

	spi_write_reg(0x3500, 0x00);//TE Enable

	spi_write_reg(0x3A00, 0x77);//Color Depth


	spi_write_single(0x1100);
	delay_ms(130);
	spi_write_single(0x2900); //Display on
	delay_ms(100);
}

#endif
#ifdef CFG_SPI_LCD_854X480_RM7701 

static void spi_send_byte(uint8_t raw, bool dc)
{
    unsigned int i;
    uint16_t data = 0;
    if (dc)
        data |= 0x0100;
    data |= raw;

    for (i = 0; i < 9; i++)
    {
        CLK_CLR();
        delay(20);
        if (data & 0x0100)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
        delay(20);
        CLK_SET();
        delay(20);
        data <<= 1;
    }
    CLK_CLR();
}

static uint8_t spi_read_byte(uint8_t cmd)
{
    unsigned int i;
    uint8_t data = 0;
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    ithGpioSet(DATA_PORT);
    ithGpioSetIn(DATA_PORT);
    delay(5);
    for (i = 0; i < 8; i++)
    {
        CLK_CLR();
        delay(30);
        CLK_SET();
        delay(20);
        if (RXD_GET())
        {
            data |= 0x01;
        }
        data <<= 1;
    }
    CLK_CLR();
    ithGpioSetOut(DATA_PORT);
    CS_SET();
    delay(2);
    return data;
}

static void spi_send_cmd(uint8_t cmd)
{
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    CS_SET();
    delay(2);
}

static void spi_send_para(uint8_t para)
{
    CS_CLR();
    delay(10);
    spi_send_byte(para, true);
    CS_SET();
    delay(2);
}

volatile uint8_t id;
void test_lcm_st7701()
{
    spi_gpio_init();
    ithGpioClear(LCM_RST);
    delay_ms(10);
    ithGpioSet(LCM_RST);
    delay_ms(20);        // Delay 1 20ms
    spi_send_cmd(0x28);
    delay_ms(20);
    spi_send_cmd(0x10);
    delay_ms(120);

    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);
    spi_send_para(0x80);

    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x91);
    delay_ms(400);

    ithGpioSet(LCM_RST);
    delay_ms(10);
    ithGpioClear(LCM_RST);
    delay_ms(10);
    ithGpioSet(LCM_RST);
    delay_ms(120);

    spi_send_cmd(0x01);
    delay_ms(120);
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);
    spi_send_cmd(0xD1);
    spi_send_para(0x11);
    spi_send_cmd(0x11);
    delay_ms(120);

    /*-------------------------------Bank0 Setting--------------------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x10);
    spi_send_cmd(0xC0);
    spi_send_para(0xE9);
    spi_send_para(0x03);
    spi_send_cmd(0xC1);
    spi_send_para(0x0B);
    spi_send_para(0x02);
    spi_send_cmd(0xC2);
    spi_send_para(0x30); //37 31
    spi_send_para(0x06);

    spi_send_cmd(0xC7);
    spi_send_para(0x04);
    /*---------------------------Gamma Cluster Setting----------------------------*/
    spi_send_cmd(0xB0);
    spi_send_para(0x00);
    spi_send_para(0x0D);
    spi_send_para(0x92);
    spi_send_para(0x0D);
    spi_send_para(0x10);
    spi_send_para(0x05);
    spi_send_para(0x00);
    spi_send_para(0x07);
    spi_send_para(0x07);
    spi_send_para(0x1C);
    spi_send_para(0x02);
    spi_send_para(0x11);
    spi_send_para(0x0F);
    spi_send_para(0x29);
    spi_send_para(0x38);
    spi_send_para(0x17);
    spi_send_cmd(0xB1);
    spi_send_para(0x00);
    spi_send_para(0x0D);
    spi_send_para(0x92);
    spi_send_para(0x0C);
    spi_send_para(0x0F);
    spi_send_para(0x05);
    spi_send_para(0x00);
    spi_send_para(0x07);
    spi_send_para(0x08);
    spi_send_para(0x1B);
    spi_send_para(0x06);
    spi_send_para(0x12);
    spi_send_para(0x11);
    spi_send_para(0x2A);
    spi_send_para(0x38);
    spi_send_para(0x17);

    /*-----------------------------End Gamma Setting------------------------------*/

    /*------------------------End Display Control setting-------------------------*/

    /*-----------------------------Bank0 Setting  End-----------------------------*/

    /*-------------------------------Bank1 Setting--------------------------------*/

    /*--------------------- Power Control Registers Initial ----------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);
    spi_send_cmd(0xB0);
    spi_send_para(0x4D);
    /*--------------------------------Vcom Setting--------------------------------*/
    spi_send_cmd(0xB1);
    spi_send_para(0x71);  //48 67  77

    /*------------------------------End Vcom Setting------------------------------*/
    spi_send_cmd(0xB2);
    spi_send_para(0x02);//07
    spi_send_cmd(0xB3);
    spi_send_para(0x80);
    spi_send_cmd(0xB5);
    spi_send_para(0x40);  //47
    spi_send_cmd(0xB7);
    spi_send_para(0x85);  //8a
    spi_send_cmd(0xB8);
    spi_send_para(0x20);   //21
    spi_send_cmd(0xC1);
    spi_send_para(0x78);
    spi_send_cmd(0xC2);
    spi_send_para(0x78);
    spi_send_cmd(0xD0);
    spi_send_para(0x88);
    /*--------------------End Power Control Registers Initial --------------------*/
    delay_ms(100);

    /*--------------------------------GIP Setting---------------------------------*/
    spi_send_cmd(0xE0);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x02);
    spi_send_cmd(0xE1);
    spi_send_para(0x05);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x06);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x40);
    spi_send_para(0x40);
    spi_send_cmd(0xE2);
    spi_send_para(0x33);
    spi_send_para(0x33);
    spi_send_para(0x34);
    spi_send_para(0x34);
    spi_send_para(0x5E);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x5F);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_cmd(0xE3);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x33);
    spi_send_para(0x33);
    spi_send_cmd(0xE4);
    spi_send_para(0x44);
    spi_send_para(0x44);
    spi_send_cmd(0xE5);
    spi_send_para(0x08);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0A);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0C);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0E);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_cmd(0xE6);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x33);
    spi_send_para(0x33);
    spi_send_cmd(0xE7);
    spi_send_para(0x44);
    spi_send_para(0x44);
    spi_send_cmd(0xE8);
    spi_send_para(0x07);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x09);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0B);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0D);
    spi_send_para(0x60);
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_cmd(0xEB);
    spi_send_para(0x02);
    spi_send_para(0x00);
    spi_send_para(0x39);
    spi_send_para(0x39);
    spi_send_para(0x88);
    spi_send_para(0x33);
    spi_send_para(0x10);
    spi_send_cmd(0xEC);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_cmd(0xED);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0x54);
    spi_send_para(0x76);
    spi_send_para(0x20);
    spi_send_para(0xFB);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0x2F);
    spi_send_para(0x0B);
    spi_send_para(0x67);
    spi_send_para(0x45);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    /*------------------------------End GIP Setting-------------------------------*/
    /*-------------------- Power Control Registers Initial End--------------------*/
    /*-------------------------------Bank1 Setting--------------------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_cmd(0x3A);
    spi_send_para(0x77);

    //spi_send_cmd(0x21);

    spi_send_cmd(0x29);
    delay_ms(10);
}


#endif
#ifdef CFG_SPI_LCD_854X480_ST7701_DX050C023
static void spi_send_byte(uint8_t raw, bool dc)
{
    unsigned int i;
    uint16_t data = 0;
    if (dc)
        data |= 0x0100;
    data |= raw;

    for (i = 0; i < 9; i++)
    {
        CLK_CLR();
        delay(20);
        if (data & 0x0100)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
        delay(20);
        CLK_SET();
        delay(20);
        data <<= 1;
    }
    CLK_CLR();
}

static uint8_t spi_read_byte(uint8_t cmd)
{
    unsigned int i;
    uint8_t data = 0;
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    ithGpioSet(DATA_PORT);
    ithGpioSetIn(DATA_PORT);
    delay(5);
    for (i = 0; i < 8; i++)
    {
        CLK_CLR();
        delay(30);
        CLK_SET();
        delay(20);
        if (RXD_GET())
        {
            data |= 0x01;
        }
        data <<= 1;
    }
    CLK_CLR();
    ithGpioSetOut(DATA_PORT);
    CS_SET();
    delay(2);
    return data;
}

static void spi_send_cmd(uint8_t cmd)
{
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    CS_SET();
    delay(2);
}

static void spi_send_para(uint8_t para)
{
    CS_CLR();
    delay(10);
    spi_send_byte(para, true);
    CS_SET();
    delay(2);
}

volatile uint8_t id;
void lcm_init_ST7701_DX050C023()
{
    printf("Init LCM: LCD_854X480_ST7701_DX050C023\r\n");
    spi_gpio_init();
    ithGpioClear(LCM_RST);
    delay_ms(10);
    ithGpioSet(LCM_RST);
    delay_ms(20);        // Delay 1 20ms
    spi_send_cmd(0x28);
    delay_ms(20);
    spi_send_cmd(0x10);
    delay_ms(120);

    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);
    spi_send_para(0x80);

    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x91);
    delay_ms(400);

    ithGpioSet(LCM_RST);
    delay_ms(10);
    ithGpioClear(LCM_RST);
    delay_ms(10);
    ithGpioSet(LCM_RST);
    delay_ms(120);

    spi_send_cmd(0x01);
    delay_ms(120);
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);
    spi_send_cmd(0xD1);
    spi_send_para(0x11);

    spi_send_cmd(0x11);
    delay_ms(120);
    spi_send_cmd(0x36); //��ʾ����
    spi_send_para(0x10);

    //modify by cvte_zxl
    /*-------------------------------Bank0 Setting--------------------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x10);
    spi_send_cmd(0xC0);
    spi_send_para(0xE9);
    spi_send_para(0x03);
    spi_send_cmd(0xC1);
    spi_send_para(0x0A); //--0x0B
    spi_send_para(0x02);
    spi_send_cmd(0xC2);
    spi_send_para(0x37); //--0x30 VCOM //37 31
    spi_send_para(0x08); //--0x06

    spi_send_cmd(0xC7);  //��ʾ����
    spi_send_para(0x04);
    /*---------------------------Gamma Cluster Setting----------------------------*/
    spi_send_cmd(0xB0);
    spi_send_para(0x00);
    spi_send_para(0x0D);
    spi_send_para(0x5A);
    spi_send_para(0x13);
    spi_send_para(0x18);
    spi_send_para(0x0A);
    spi_send_para(0x0C);
    spi_send_para(0x09);
    spi_send_para(0x08);
    spi_send_para(0x20);
    spi_send_para(0x05);
    spi_send_para(0x13);
    spi_send_para(0x12);
    spi_send_para(0x15);
    spi_send_para(0x1B);
    spi_send_para(0x15);

    spi_send_cmd(0xB1);
    spi_send_para(0x00);
    spi_send_para(0x0E);
    spi_send_para(0x1A);
    spi_send_para(0x11);
    spi_send_para(0x16);
    spi_send_para(0x0A);
    spi_send_para(0x0E);
    spi_send_para(0x08);
    spi_send_para(0x09);
    spi_send_para(0x24);
    spi_send_para(0x09);
    spi_send_para(0x17);
    spi_send_para(0x12);
    spi_send_para(0x15);
    spi_send_para(0x1B);
    spi_send_para(0x15);

    /*-----------------------------End Gamma Setting------------------------------*/

    /*------------------------End Display Control setting-------------------------*/

    /*-----------------------------Bank0 Setting  End-----------------------------*/

    /*-------------------------------Bank1 Setting--------------------------------*/

    /*--------------------- Power Control Registers Initial ----------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x11);

    spi_send_cmd(0xB0);
    spi_send_para(0x4D);
    /*--------------------------------Vcom Setting--------------------------------*/
    spi_send_cmd(0xB1);
    spi_send_para(0x2C);  //modify from 0x38 to 0x2C

    /*------------------------------End Vcom Setting------------------------------*/
    spi_send_cmd(0xB2);
    spi_send_para(0x07); //--0x02 //07
    spi_send_cmd(0xB3);
    spi_send_para(0x80);
    spi_send_cmd(0xB5);
    spi_send_para(0x47); //--0x40 //47
    spi_send_cmd(0xB7);
    spi_send_para(0x8A); //--0x85 //8a
    spi_send_cmd(0xB8);
    spi_send_para(0x10); //--0x20 //21
    spi_send_cmd(0xC1);
    spi_send_para(0x78);
    spi_send_cmd(0xC2);
    spi_send_para(0x78);
    spi_send_cmd(0xD0);
    spi_send_para(0x88);
    /*--------------------End Power Control Registers Initial --------------------*/
    delay_ms(100);

    /*--------------------------------GIP Setting---------------------------------*/
    spi_send_cmd(0xE0);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x02);

    spi_send_cmd(0xE1);
    spi_send_para(0x02); //--0x05
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x01); //--0x06
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x40);
    spi_send_para(0x40);

    spi_send_cmd(0xE2);
    spi_send_para(0x30); //--0x33
    spi_send_para(0x30); //--0x33
    spi_send_para(0x40); //--0x34
    spi_send_para(0x40); //--0x34
    spi_send_para(0x60); //--0x5E
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x5F);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);

    spi_send_cmd(0xE3);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x33);
    spi_send_para(0x33);

    spi_send_cmd(0xE4);
    spi_send_para(0x44);
    spi_send_para(0x44);

    spi_send_cmd(0xE5);
    spi_send_para(0x07); //--0x08
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x09); //--0x09
    spi_send_para(0x6B); //--0x6B
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0B); //--0x0B
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0D); //--0x0E
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);

    spi_send_cmd(0xE6);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x33);
    spi_send_para(0x33);

    spi_send_cmd(0xE7);
    spi_send_para(0x44);
    spi_send_para(0x44);

    spi_send_cmd(0xE8);
    spi_send_para(0x06); //--0x07
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x08); //--0x09
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0A); //--0x0B
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);
    spi_send_para(0x0C); //--0x0D
    spi_send_para(0x6B); //--0x60
    spi_send_para(0xA0);
    spi_send_para(0xA0);

    spi_send_cmd(0xEB);
    spi_send_para(0x02);
    spi_send_para(0x00);
    spi_send_para(0x93); //--0x39
    spi_send_para(0x93); //--0x39
    spi_send_para(0x88);
    spi_send_para(0x00); //--0x33
    spi_send_para(0x00); //--0x10

    //--spi_send_cmd(0xEC);
    //--spi_send_para(0x00);
    //--spi_send_para(0x00);

    spi_send_cmd(0xED);
    spi_send_para(0xFA); //--0xFF
    spi_send_para(0xB0); //--0xFF
    spi_send_para(0x2F); //--0x54
    spi_send_para(0xF4); //--0x76
    spi_send_para(0x65); //--0x20
    spi_send_para(0x7F); //--0xFB
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0xFF);
    spi_send_para(0xF7); //--0x2F
    spi_send_para(0x56); //--0x0B
    spi_send_para(0x4F); //--0x67
    spi_send_para(0xF2); //--0x45
    spi_send_para(0x0B); //--0xFF
    spi_send_para(0xAF); //--0xFF
    /*------------------------------End GIP Setting-------------------------------*/
    /*-------------------- Power Control Registers Initial End--------------------*/
    /*-------------------------------Bank1 Setting--------------------------------*/
    spi_send_cmd(0xFF);
    spi_send_para(0x77);
    spi_send_para(0x01);
    spi_send_para(0x00);
    spi_send_para(0x00);
    spi_send_para(0x00);

    spi_send_cmd(0x35);
    spi_send_para(0x00);
    //--spi_send_cmd(0x3A);
    //--spi_send_para(0x77);

    spi_send_cmd(0x21); //��ɫ��ת

    spi_send_cmd(0x29);
    delay_ms(10);
}
#endif
#ifdef CFG_SPI_LCD_800X480_NT35510
static void spi_write(unsigned char dat)
{
	unsigned char i;
	for(i = 0; i < 8; i++)
	{
        if (dat & 0x80)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
		dat <<= 1;
		CLK_CLR();
        usleep(2);
        CLK_SET();
        usleep(20);
	}
}

static void spi_write_comm(unsigned int dat)
{
	CS_CLR();
	spi_write(0x20);
	spi_write(dat>>8);
	spi_write(0x00);
	spi_write(dat);
	CS_SET();
}

static void spi_write_data(unsigned int dat)
{
	CS_CLR();
	spi_write(0x40);
	spi_write(dat);
	CS_SET();
}


static void spi_write_reg(uint16_t add, uint8_t val)
{
	spi_write_comm(add);
	spi_write_data(val);
}

static void lcd_init()
{
	spi_gpio_init();
    ithGpioClear(LCM_RST);
    usleep(800);
    ithGpioSet(LCM_RST);
	usleep(800);
	
	//LV2 Page 1 enable
    spi_write_reg(0xF000,0x55);
	spi_write_reg(0xF001,0xAA);
	spi_write_reg(0xF002,0x52);
	spi_write_reg(0xF003,0x08);
	spi_write_reg(0xF004,0x01);

	//AVDD Set AVDD 5.2V
	spi_write_reg(0xB000,0x0C);
	spi_write_reg(0xB001,0x0C);
	spi_write_reg(0xB002,0x0C);

	//AVDD ratio
	spi_write_reg(0xB600,0x05);   //	spi_write_reg(0xB600,0x34);
	spi_write_reg(0xB600,0x05);   //	spi_write_reg(0xB601,0x34);
	spi_write_reg(0xB600,0x05);   //	spi_write_reg(0xB602,0x34);
	 
	//AVEE  -5.2V
	spi_write_reg(0xB100,0x0C);
	spi_write_reg(0xB101,0x0C);
	spi_write_reg(0xB102,0x0C);

	//AVEE ratio
	spi_write_reg(0xB700,0x24);
	spi_write_reg(0xB701,0x24);
	spi_write_reg(0xB702,0x24);

	//VCL  -2.5V
	spi_write_reg(0xB200,0x00);
	//spi_write_reg(0xB201,0x00);
	//spi_write_reg(0xB202,0x00);

	//VCL ratio
	spi_write_reg(0xB800,0x34);
	//spi_write_reg(0xB801,0x24);
	//spi_write_reg(0xB802,0x24);


	//VGH 15V  (Free pump)
	spi_write_reg(0xBF00,0x01);
	spi_write_reg(0xB300,0x08);
	spi_write_reg(0xB301,0x08);
	spi_write_reg(0xB302,0x08);

	//VGH ratio
	spi_write_reg(0xB900,0x34);
	spi_write_reg(0xB901,0x34);
	spi_write_reg(0xB902,0x34);

	//VGL_REG -10V
	spi_write_reg(0xB500,0x08);//0x50
	spi_write_reg(0xB501,0x08);
	spi_write_reg(0xB502,0x08);

	spi_write_reg(0xC200,0x03);

	//VGLX ratio
	spi_write_reg(0xBA00,0x14);
	spi_write_reg(0xBA01,0x14);
	spi_write_reg(0xBA02,0x14);

	//VGMP/VGSP 4.5V/0V
	spi_write_reg(0xBC00,0x00);
	spi_write_reg(0xBC01,0x80);
	spi_write_reg(0xBC02,0x00);

	//VGMN/VGSN -4.5V/0V
	spi_write_reg(0xBD00,0x00);
	spi_write_reg(0xBD01,0x80);
	spi_write_reg(0xBD02,0x00);

	//VCOM  
	spi_write_reg(0xBE00,0x00);
	spi_write_reg(0xBE01,0x2F);

	//Gamma Setting
	spi_write_reg(0xD100,0x00);
	spi_write_reg(0xD101,0x37);
	spi_write_reg(0xD102,0x00);
	spi_write_reg(0xD103,0x53);
	spi_write_reg(0xD104,0x00);
	spi_write_reg(0xD105,0x79);
	spi_write_reg(0xD106,0x00);
	spi_write_reg(0xD107,0x97);
	spi_write_reg(0xD108,0x00);
	spi_write_reg(0xD109,0xB1);
	spi_write_reg(0xD10A,0x00);
	spi_write_reg(0xD10B,0xD5);
	spi_write_reg(0xD10C,0x00);
	spi_write_reg(0xD10D,0xF4);
	spi_write_reg(0xD10E,0x01);
	spi_write_reg(0xD10F,0x23);
	spi_write_reg(0xD110,0x01);
	spi_write_reg(0xD111,0x49);
	spi_write_reg(0xD112,0x01);
	spi_write_reg(0xD113,0x87);
	spi_write_reg(0xD114,0x01);
	spi_write_reg(0xD115,0xB6);
	spi_write_reg(0xD116,0x02);
	spi_write_reg(0xD117,0x00);
	spi_write_reg(0xD118,0x02);
	spi_write_reg(0xD119,0x3B);
	spi_write_reg(0xD11A,0x02);
	spi_write_reg(0xD11B,0x3D);
	spi_write_reg(0xD11C,0x02);
	spi_write_reg(0xD11D,0x75);
	spi_write_reg(0xD11E,0x02);
	spi_write_reg(0xD11F,0xB1);
	spi_write_reg(0xD120,0x02);
	spi_write_reg(0xD121,0xD5);
	spi_write_reg(0xD122,0x03);
	spi_write_reg(0xD123,0x09);
	spi_write_reg(0xD124,0x03);
	spi_write_reg(0xD125,0x28);
	spi_write_reg(0xD126,0x03);
	spi_write_reg(0xD127,0x52);
	spi_write_reg(0xD128,0x03);
	spi_write_reg(0xD129,0x6B);
	spi_write_reg(0xD12A,0x03);
	spi_write_reg(0xD12B,0x8D);
	spi_write_reg(0xD12C,0x03);
	spi_write_reg(0xD12D,0xA2);
	spi_write_reg(0xD12E,0x03);
	spi_write_reg(0xD12F,0xBB);
	spi_write_reg(0xD130,0x03);
	spi_write_reg(0xD131,0xC1);
	spi_write_reg(0xD132,0x03);
	spi_write_reg(0xD133,0xC1);

	spi_write_reg(0xD200,0x00);
	spi_write_reg(0xD201,0x37);
	spi_write_reg(0xD202,0x00);
	spi_write_reg(0xD203,0x53);
	spi_write_reg(0xD204,0x00);
	spi_write_reg(0xD205,0x79);
	spi_write_reg(0xD206,0x00);
	spi_write_reg(0xD207,0x97);
	spi_write_reg(0xD208,0x00);
	spi_write_reg(0xD209,0xB1);
	spi_write_reg(0xD20A,0x00);
	spi_write_reg(0xD20B,0xD5);
	spi_write_reg(0xD20C,0x00);
	spi_write_reg(0xD20D,0xF4);
	spi_write_reg(0xD20E,0x01);
	spi_write_reg(0xD20F,0x23);
	spi_write_reg(0xD210,0x01);
	spi_write_reg(0xD211,0x49);
	spi_write_reg(0xD212,0x01);
	spi_write_reg(0xD213,0x87);
	spi_write_reg(0xD214,0x01);
	spi_write_reg(0xD215,0xB6);
	spi_write_reg(0xD216,0x02);
	spi_write_reg(0xD217,0x00);
	spi_write_reg(0xD218,0x02);
	spi_write_reg(0xD219,0x3B);
	spi_write_reg(0xD21A,0x02);
	spi_write_reg(0xD21B,0x3D);
	spi_write_reg(0xD21C,0x02);
	spi_write_reg(0xD21D,0x75);
	spi_write_reg(0xD21E,0x02);
	spi_write_reg(0xD21F,0xB1);
	spi_write_reg(0xD220,0x02);
	spi_write_reg(0xD221,0xD5);
	spi_write_reg(0xD222,0x03);
	spi_write_reg(0xD223,0x09);
	spi_write_reg(0xD224,0x03);
	spi_write_reg(0xD225,0x28);
	spi_write_reg(0xD226,0x03);
	spi_write_reg(0xD227,0x52);
	spi_write_reg(0xD228,0x03);
	spi_write_reg(0xD229,0x6B);
	spi_write_reg(0xD22A,0x03);
	spi_write_reg(0xD22B,0x8D);
	spi_write_reg(0xD22C,0x03);
	spi_write_reg(0xD22D,0xA2);
	spi_write_reg(0xD22E,0x03);
	spi_write_reg(0xD22F,0xBB);
	spi_write_reg(0xD230,0x03);
	spi_write_reg(0xD231,0xC1);
	spi_write_reg(0xD232,0x03);
	spi_write_reg(0xD233,0xC1);

	spi_write_reg(0xD300,0x00);
	spi_write_reg(0xD301,0x37);
	spi_write_reg(0xD302,0x00);
	spi_write_reg(0xD303,0x53);
	spi_write_reg(0xD304,0x00);
	spi_write_reg(0xD305,0x79);
	spi_write_reg(0xD306,0x00);
	spi_write_reg(0xD307,0x97);
	spi_write_reg(0xD308,0x00);
	spi_write_reg(0xD309,0xB1);
	spi_write_reg(0xD30A,0x00);
	spi_write_reg(0xD30B,0xD5);
	spi_write_reg(0xD30C,0x00);
	spi_write_reg(0xD30D,0xF4);
	spi_write_reg(0xD30E,0x01);
	spi_write_reg(0xD30F,0x23);
	spi_write_reg(0xD310,0x01);
	spi_write_reg(0xD311,0x49);
	spi_write_reg(0xD312,0x01);
	spi_write_reg(0xD313,0x87);
	spi_write_reg(0xD314,0x01);
	spi_write_reg(0xD315,0xB6);
	spi_write_reg(0xD316,0x02);
	spi_write_reg(0xD317,0x00);
	spi_write_reg(0xD318,0x02);
	spi_write_reg(0xD319,0x3B);
	spi_write_reg(0xD31A,0x02);
	spi_write_reg(0xD31B,0x3D);
	spi_write_reg(0xD31C,0x02);
	spi_write_reg(0xD31D,0x75);
	spi_write_reg(0xD31E,0x02);
	spi_write_reg(0xD31F,0xB1);
	spi_write_reg(0xD320,0x02);
	spi_write_reg(0xD321,0xD5);
	spi_write_reg(0xD322,0x03);
	spi_write_reg(0xD323,0x09);
	spi_write_reg(0xD324,0x03);
	spi_write_reg(0xD325,0x28);
	spi_write_reg(0xD326,0x03);
	spi_write_reg(0xD327,0x52);
	spi_write_reg(0xD328,0x03);
	spi_write_reg(0xD329,0x6B);
	spi_write_reg(0xD32A,0x03);
	spi_write_reg(0xD32B,0x8D);
	spi_write_reg(0xD32C,0x03);
	spi_write_reg(0xD32D,0xA2);
	spi_write_reg(0xD32E,0x03);
	spi_write_reg(0xD32F,0xBB);
	spi_write_reg(0xD330,0x03);
	spi_write_reg(0xD331,0xC1);
	spi_write_reg(0xD332,0x03);
	spi_write_reg(0xD333,0xC1);

	spi_write_reg(0xD400,0x00);
	spi_write_reg(0xD401,0x37);
	spi_write_reg(0xD402,0x00);
	spi_write_reg(0xD403,0x53);
	spi_write_reg(0xD404,0x00);
	spi_write_reg(0xD405,0x79);
	spi_write_reg(0xD406,0x00);
	spi_write_reg(0xD407,0x97);
	spi_write_reg(0xD408,0x00);
	spi_write_reg(0xD409,0xB1);
	spi_write_reg(0xD40A,0x00);
	spi_write_reg(0xD40B,0xD5);
	spi_write_reg(0xD40C,0x00);
	spi_write_reg(0xD40D,0xF4);
	spi_write_reg(0xD40E,0x01);
	spi_write_reg(0xD40F,0x23);
	spi_write_reg(0xD410,0x01);
	spi_write_reg(0xD411,0x49);
	spi_write_reg(0xD412,0x01);
	spi_write_reg(0xD413,0x87);
	spi_write_reg(0xD414,0x01);
	spi_write_reg(0xD415,0xB6);
	spi_write_reg(0xD416,0x02);
	spi_write_reg(0xD417,0x00);
	spi_write_reg(0xD418,0x02);
	spi_write_reg(0xD419,0x3B);
	spi_write_reg(0xD41A,0x02);
	spi_write_reg(0xD41B,0x3D);
	spi_write_reg(0xD41C,0x02);
	spi_write_reg(0xD41D,0x75);
	spi_write_reg(0xD41E,0x02);
	spi_write_reg(0xD41F,0xB1);
	spi_write_reg(0xD420,0x02);
	spi_write_reg(0xD421,0xD5);
	spi_write_reg(0xD422,0x03);
	spi_write_reg(0xD423,0x09);
	spi_write_reg(0xD424,0x03);
	spi_write_reg(0xD425,0x28);
	spi_write_reg(0xD426,0x03);
	spi_write_reg(0xD427,0x52);
	spi_write_reg(0xD428,0x03);
	spi_write_reg(0xD429,0x6B);
	spi_write_reg(0xD42A,0x03);
	spi_write_reg(0xD42B,0x8D);
	spi_write_reg(0xD42C,0x03);
	spi_write_reg(0xD42D,0xA2);
	spi_write_reg(0xD42E,0x03);
	spi_write_reg(0xD42F,0xBB);
	spi_write_reg(0xD430,0x03);
	spi_write_reg(0xD431,0xC1);
	spi_write_reg(0xD432,0x03);
	spi_write_reg(0xD433,0xC1);

	spi_write_reg(0xD500,0x00);
	spi_write_reg(0xD501,0x37);
	spi_write_reg(0xD502,0x00);
	spi_write_reg(0xD503,0x53);
	spi_write_reg(0xD504,0x00);
	spi_write_reg(0xD505,0x79);
	spi_write_reg(0xD506,0x00);
	spi_write_reg(0xD507,0x97);
	spi_write_reg(0xD508,0x00);
	spi_write_reg(0xD509,0xB1);
	spi_write_reg(0xD50A,0x00);
	spi_write_reg(0xD50B,0xD5);
	spi_write_reg(0xD50C,0x00);
	spi_write_reg(0xD50D,0xF4);
	spi_write_reg(0xD50E,0x01);
	spi_write_reg(0xD50F,0x23);
	spi_write_reg(0xD510,0x01);
	spi_write_reg(0xD511,0x49);
	spi_write_reg(0xD512,0x01);
	spi_write_reg(0xD513,0x87);
	spi_write_reg(0xD514,0x01);
	spi_write_reg(0xD515,0xB6);
	spi_write_reg(0xD516,0x02);
	spi_write_reg(0xD517,0x00);
	spi_write_reg(0xD518,0x02);
	spi_write_reg(0xD519,0x3B);
	spi_write_reg(0xD51A,0x02);
	spi_write_reg(0xD51B,0x3D);
	spi_write_reg(0xD51C,0x02);
	spi_write_reg(0xD51D,0x75);
	spi_write_reg(0xD51E,0x02);
	spi_write_reg(0xD51F,0xB1);
	spi_write_reg(0xD520,0x02);
	spi_write_reg(0xD521,0xD5);
	spi_write_reg(0xD522,0x03);
	spi_write_reg(0xD523,0x09);
	spi_write_reg(0xD524,0x03);
	spi_write_reg(0xD525,0x28);
	spi_write_reg(0xD526,0x03);
	spi_write_reg(0xD527,0x52);
	spi_write_reg(0xD528,0x03);
	spi_write_reg(0xD529,0x6B);
	spi_write_reg(0xD52A,0x03);
	spi_write_reg(0xD52B,0x8D);
	spi_write_reg(0xD52C,0x03);
	spi_write_reg(0xD52D,0xA2);
	spi_write_reg(0xD52E,0x03);
	spi_write_reg(0xD52F,0xBB);
	spi_write_reg(0xD530,0x03);
	spi_write_reg(0xD531,0xC1);
	spi_write_reg(0xD532,0x03);
	spi_write_reg(0xD533,0xC1);

	spi_write_reg(0xD600,0x00);
	spi_write_reg(0xD601,0x37);
	spi_write_reg(0xD602,0x00);
	spi_write_reg(0xD603,0x53);
	spi_write_reg(0xD604,0x00);
	spi_write_reg(0xD605,0x79);
	spi_write_reg(0xD606,0x00);
	spi_write_reg(0xD607,0x97);
	spi_write_reg(0xD608,0x00);
	spi_write_reg(0xD609,0xB1);
	spi_write_reg(0xD60A,0x00);
	spi_write_reg(0xD60B,0xD5);
	spi_write_reg(0xD60C,0x00);
	spi_write_reg(0xD60D,0xF4);
	spi_write_reg(0xD60E,0x01);
	spi_write_reg(0xD60F,0x23);
	spi_write_reg(0xD610,0x01);
	spi_write_reg(0xD611,0x49);
	spi_write_reg(0xD612,0x01);
	spi_write_reg(0xD613,0x87);
	spi_write_reg(0xD614,0x01);
	spi_write_reg(0xD615,0xB6);
	spi_write_reg(0xD616,0x02);
	spi_write_reg(0xD617,0x00);
	spi_write_reg(0xD618,0x02);
	spi_write_reg(0xD619,0x3B);
	spi_write_reg(0xD61A,0x02);
	spi_write_reg(0xD61B,0x3D);
	spi_write_reg(0xD61C,0x02);
	spi_write_reg(0xD61D,0x75);
	spi_write_reg(0xD61E,0x02);
	spi_write_reg(0xD61F,0xB1);
	spi_write_reg(0xD620,0x02);
	spi_write_reg(0xD621,0xD5);
	spi_write_reg(0xD622,0x03);
	spi_write_reg(0xD623,0x09);
	spi_write_reg(0xD624,0x03);
	spi_write_reg(0xD625,0x28);
	spi_write_reg(0xD626,0x03);
	spi_write_reg(0xD627,0x52);
	spi_write_reg(0xD628,0x03);
	spi_write_reg(0xD629,0x6B);
	spi_write_reg(0xD62A,0x03);
	spi_write_reg(0xD62B,0x8D);
	spi_write_reg(0xD62C,0x03);
	spi_write_reg(0xD62D,0xA2);
	spi_write_reg(0xD62E,0x03);
	spi_write_reg(0xD62F,0xBB);
	spi_write_reg(0xD630,0x03);
	spi_write_reg(0xD631,0xC1);
	spi_write_reg(0xD632,0x03);
	spi_write_reg(0xD633,0xC1);

	//LV2 Page 0 enable
	spi_write_reg(0xF000,0x55);
	spi_write_reg(0xF001,0xAA);
	spi_write_reg(0xF002,0x52);
	spi_write_reg(0xF003,0x08);
	spi_write_reg(0xF004,0x00);

	//Display control
	spi_write_reg(0xB100,0xCC);
	spi_write_reg(0xB101,0x00);

	//Source hold time
	spi_write_reg(0xB600,0x05);

	//Gate EQ control
	spi_write_reg(0xB700,0x70);
	spi_write_reg(0xB701,0x70);

	//Source EQ control (Mode 2)
	spi_write_reg(0xB800,0x01);
	spi_write_reg(0xB801,0x03);
	spi_write_reg(0xB802,0x03);
	spi_write_reg(0xB803,0x03);

	//Inversion mode  (2-dot)
	spi_write_reg(0xBC00,0x02);
	spi_write_reg(0xBC01,0x00);
	spi_write_reg(0xBC02,0x00);

	//Timing control 4H w/ 4-Delay(
	spi_write_reg(0xC900,0xD0);
	spi_write_reg(0xC901,0x02);
	spi_write_reg(0xC902,0x50);
	spi_write_reg(0xC903,0x50);
	spi_write_reg(0xC904,0x50);

	spi_write_reg(0x3500,0x00);

	spi_write_reg(0x3600,0x00);

	spi_write_reg(0x3A00,0x77);
	 

	spi_write_reg(0x1100,0x00); 
	usleep(120);
	spi_write_reg(0x2900,0x00); //Display On  

	spi_write_comm(0x2C00);
	usleep(120);

}


#endif
#ifdef CFG_SPI_LCD_800X480_OTM8019A

static void spi_write(unsigned char dat)
{
	unsigned char i;
	for(i = 0; i < 8; i++)
	{
        if (dat & 0x80)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
		dat <<= 1;
		CLK_CLR();
        usleep(2);
        CLK_SET();
        usleep(20);
	}
}

static void spi_write_comm(unsigned int dat)
{
	CS_CLR();
	spi_write(0x20);
	spi_write(dat>>8);
	spi_write(0x00);
	spi_write(dat);
	CS_SET();
}

static void spi_write_data(unsigned int dat)
{
	CS_CLR();
	spi_write(0x40);
	spi_write(dat);
	CS_SET();
}


static void spi_write_reg(uint16_t add, uint8_t val)
{
	spi_write_comm(add);
	spi_write_data(val);
}

void lcm_init_OTM8019A()
{
	ithGpioSet(LCM_RST);
	delay_ms(3);
	ithGpioClear(LCM_RST);
	delay_ms(3);
	ithGpioSet(LCM_RST);
	delay_ms(3);


	spi_write_reg(0xFF00,0x80);
	spi_write_reg(0xFF01,0x19);
	spi_write_reg(0xFF02,0x01);

	spi_write_reg(0xFF80,0x80);
	spi_write_reg(0xFF81,0x19);

	spi_write_reg(0xFF03,0x01);

	spi_write_reg(0xC0B4,0x00);
	spi_write_reg(0xC0B5,0x18);  //add by cvte hcy

	spi_write_reg(0xC590,0x4E);
	spi_write_reg(0xC591,0x3C);
	spi_write_reg(0xC592,0x01);
	spi_write_reg(0xC593,0x03);
	spi_write_reg(0xC594,0x33);
	spi_write_reg(0xC595,0x34);
	spi_write_reg(0xC596,0x23);

	spi_write_reg(0xC582,0x80);

	spi_write_reg(0xC5B1,0xA9);

	spi_write_reg(0xD800,0x6F);
	spi_write_reg(0xD801,0x6F);

	spi_write_reg(0xC181,0x33);

	spi_write_reg(0xD900,0x39); //57

	spi_write_reg(0xC481,0x81);

	spi_write_reg(0xB3A6,0x20);
	spi_write_reg(0xB3A7,0x01);

	spi_write_reg(0xC090,0x00);
	spi_write_reg(0xC091,0x44);
	spi_write_reg(0xC092,0x00);
	spi_write_reg(0xC093,0x00);
	spi_write_reg(0xC094,0x00);
	spi_write_reg(0xC095,0x03);

	spi_write_reg(0xCE80,0x86);
	spi_write_reg(0xCE81,0x01);
	spi_write_reg(0xCE82,0x00);
	spi_write_reg(0xCE83,0x85);
	spi_write_reg(0xCE84,0x01);
	spi_write_reg(0xCE85,0x00);
	spi_write_reg(0xCE86,0x00);
	spi_write_reg(0xCE87,0x00);
	spi_write_reg(0xCE88,0x00);
	spi_write_reg(0xCE89,0x00);
	spi_write_reg(0xCE8A,0x00);
	spi_write_reg(0xCE8B,0x00);

	spi_write_reg(0xCEA0,0x18);
	spi_write_reg(0xCEA1,0x05);
	spi_write_reg(0xCEA2,0x03);
	spi_write_reg(0xCEA3,0x39);
	spi_write_reg(0xCEA4,0x00);
	spi_write_reg(0xCEA5,0x00);
	spi_write_reg(0xCEA6,0x00);
	spi_write_reg(0xCEA7,0x18);
	spi_write_reg(0xCEA8,0x04);
	spi_write_reg(0xCEA9,0x03);
	spi_write_reg(0xCEAA,0x3A);
	spi_write_reg(0xCEAB,0x00);
	spi_write_reg(0xCEAC,0x00);
	spi_write_reg(0xCEAD,0x00);

	spi_write_reg(0xCEB0,0x18);
	spi_write_reg(0xCEB1,0x03);
	spi_write_reg(0xCEB2,0x03);
	spi_write_reg(0xCEB3,0x3B);
	spi_write_reg(0xCEB4,0x00);
	spi_write_reg(0xCEB5,0x00);
	spi_write_reg(0xCEB6,0x00);
	spi_write_reg(0xCEB7,0x18);
	spi_write_reg(0xCEB8,0x02);
	spi_write_reg(0xCEB9,0x03);
	spi_write_reg(0xCEBA,0x3C);
	spi_write_reg(0xCEBB,0x00);
	spi_write_reg(0xCEBC,0x00);
	spi_write_reg(0xCEBD,0x00);

	spi_write_reg(0xCFC0,0x01);
	spi_write_reg(0xCFC1,0x01);
	spi_write_reg(0xCFC2,0x20);
	spi_write_reg(0xCFC3,0x20);
	spi_write_reg(0xCFC4,0x00);
	spi_write_reg(0xCFC5,0x00);
	spi_write_reg(0xCFC6,0x01);
	spi_write_reg(0xCFC7,0x00);
	spi_write_reg(0xCFC8,0x00);
	spi_write_reg(0xCFC9,0x00);

	spi_write_reg(0xCBC0,0x00);
	spi_write_reg(0xCBC1,0x01);
	spi_write_reg(0xCBC2,0x01);
	spi_write_reg(0xCBC3,0x01);
	spi_write_reg(0xCBC4,0x01);
	spi_write_reg(0xCBC5,0x01);
	spi_write_reg(0xCBC6,0x00);
	spi_write_reg(0xCBC7,0x00);
	spi_write_reg(0xCBC8,0x00);
	spi_write_reg(0xCBC9,0x00);
	spi_write_reg(0xCBCA,0x00);
	spi_write_reg(0xCBCB,0x00);
	spi_write_reg(0xCBCC,0x00);
	spi_write_reg(0xCBCD,0x00);
	spi_write_reg(0xCBCE,0x00);

	spi_write_reg(0xCBD0,0x00);

	spi_write_reg(0xCBD5,0x00);
	spi_write_reg(0xCBD6,0x00);
	spi_write_reg(0xCBD7,0x00);
	spi_write_reg(0xCBD8,0x00);
	spi_write_reg(0xCBD9,0x00);
	spi_write_reg(0xCBDA,0x00);
	spi_write_reg(0xCBDB,0x00);
	spi_write_reg(0xCBDC,0x00);
	spi_write_reg(0xCBDD,0x00);
	spi_write_reg(0xCBDE,0x00);

	spi_write_reg(0xCBE0,0x01);
	spi_write_reg(0xCBE1,0x01);
	spi_write_reg(0xCBE2,0x01);
	spi_write_reg(0xCBE3,0x01);
	spi_write_reg(0xCBE4,0x01);
	spi_write_reg(0xCBE5,0x00);

	spi_write_reg(0xCC80,0x00);
	spi_write_reg(0xCC81,0x26);
	spi_write_reg(0xCC82,0x09);
	spi_write_reg(0xCC83,0x0B);
	spi_write_reg(0xCC84,0x01);
	spi_write_reg(0xCC85,0x25);
	spi_write_reg(0xCC86,0x00);
	spi_write_reg(0xCC87,0x00);
	spi_write_reg(0xCC88,0x00);
	spi_write_reg(0xCC89,0x00);

	spi_write_reg(0xCC90,0x00);
	spi_write_reg(0xCC91,0x00);
	spi_write_reg(0xCC92,0x00);
	spi_write_reg(0xCC93,0x00);
	spi_write_reg(0xCC94,0x00);
	spi_write_reg(0xCC95,0x00);

	spi_write_reg(0xCC9A,0x00);
	spi_write_reg(0xCC9B,0x00);
	spi_write_reg(0xCC9C,0x00);
	spi_write_reg(0xCC9D,0x00);
	spi_write_reg(0xCC9E,0x00);

	spi_write_reg(0xCCA0,0x00);
	spi_write_reg(0xCCA1,0x00);
	spi_write_reg(0xCCA2,0x00);
	spi_write_reg(0xCCA3,0x00);
	spi_write_reg(0xCCA4,0x00);
	spi_write_reg(0xCCA5,0x25);
	spi_write_reg(0xCCA6,0x02);
	spi_write_reg(0xCCA7,0x0C);
	spi_write_reg(0xCCA8,0x0A);
	spi_write_reg(0xCCA9,0x26);
	spi_write_reg(0xCCAA,0x00);

	spi_write_reg(0xCCB0,0x00);
	spi_write_reg(0xCCB1,0x25);
	spi_write_reg(0xCCB2,0x0C);
	spi_write_reg(0xCCB3,0x0A);
	spi_write_reg(0xCCB4,0x02);
	spi_write_reg(0xCCB5,0x26);
	spi_write_reg(0xCCB6,0x00);
	spi_write_reg(0xCCB7,0x00);
	spi_write_reg(0xCCB8,0x00);
	spi_write_reg(0xCCB9,0x00);

	spi_write_reg(0xCCC0,0x00);
	spi_write_reg(0xCCC1,0x00);
	spi_write_reg(0xCCC2,0x00);
	spi_write_reg(0xCCC3,0x00);
	spi_write_reg(0xCCC4,0x00);
	spi_write_reg(0xCCC5,0x00);

	spi_write_reg(0xCCCA,0x00);
	spi_write_reg(0xCCCB,0x00);
	spi_write_reg(0xCCCC,0x00);
	spi_write_reg(0xCCCD,0x00);
	spi_write_reg(0xCCCE,0x00);

	spi_write_reg(0xCCD0,0x00);
	spi_write_reg(0xCCD1,0x00);
	spi_write_reg(0xCCD2,0x00);
	spi_write_reg(0xCCD3,0x00);
	spi_write_reg(0xCCD4,0x00);
	spi_write_reg(0xCCD5,0x26);
	spi_write_reg(0xCCD6,0x01);
	spi_write_reg(0xCCD7,0x09);
	spi_write_reg(0xCCD8,0x0B);
	spi_write_reg(0xCCD9,0x25);
	spi_write_reg(0xCCDA,0x00);


	spi_write_reg(0xC480,0x30);

	spi_write_reg(0xC1A0,0xE8);

	spi_write_reg(0xC180,0x03);

	spi_write_reg(0xC098,0x00);

	spi_write_reg(0xC0A9,0x06);

	spi_write_reg(0xC1B0,0x20);
	spi_write_reg(0xC1B1,0x00);
	spi_write_reg(0xC1B2,0x00);

	spi_write_reg(0xC0E1,0x40);
	spi_write_reg(0xC0E2,0x18);

	spi_write_reg(0xB690,0xB4);

	spi_write_reg(0xFF00,0xFF);
	spi_write_reg(0xFF01,0xFF);
	spi_write_reg(0xFF02,0xFF);



	spi_write_comm(0x1100);
	usleep(120);
	spi_write_comm(0x2900); //Display On
	delay_ms(10);
	spi_write_comm(0x2C00);
	usleep(120);
}

#endif
#ifdef CFG_SPI_LCD_854X480_ILI9806E
static void spi_send_byte(uint8_t raw, bool dc)
{
    unsigned int i;
    uint16_t data = 0;
    if (dc)
        data |= 0x0100;
    data |= raw;

    for (i = 0; i < 9; i++)
    {
        CLK_CLR();
        delay(20);
        if (data & 0x0100)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
        delay(20);
        CLK_SET();
        delay(20);
        data <<= 1;
    }
    CLK_CLR();
}

static uint8_t spi_read_byte(uint8_t cmd)
{
    unsigned int i;
    uint8_t data = 0;
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    ithGpioSet(DATA_PORT);
    ithGpioSetIn(DATA_PORT);
    delay(5);
    for (i = 0; i < 8; i++)
    {
        CLK_CLR();
        delay(30);
        CLK_SET();
        delay(20);
        if (RXD_GET())
        {
            data |= 0x01;
        }
        data <<= 1;
    }
    CLK_CLR();
    ithGpioSetOut(DATA_PORT);
    CS_SET();
    delay(2);
    return data;
}

static void spi_send_cmd(uint8_t cmd)
{
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    CS_SET();
    delay(2);
}

static void spi_send_para(uint8_t para)
{
    CS_CLR();
    delay(10);
    spi_send_byte(para, true);
    CS_SET();
    delay(2);
}

void lcm_init_ILI9806E()
{
    printf("Init LCM: lcm_init_ILI9806E\r\n");
    spi_gpio_init();
    ithGpioSet(LCM_RST);
    usleep(10*1000);
    ithGpioClear(LCM_RST);
    usleep(10*1000);
    ithGpioSet(LCM_RST);
    usleep(120*1000);        // Delay 1 20ms

    //****************************************************************************//
	//****************************** Page 0 Command ******************************//
	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x00);     				// Change to Page 0

	spi_send_cmd(0x36);
	spi_send_para(0x03);


	//****************************************************************************//
	//****************************** Page 1 Command ******************************//
	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x01);     				// Change to Page 1

	spi_send_cmd(0x08);
	spi_send_para(0x18);                 	// output SDA

	spi_send_cmd(0x21);
	spi_send_para(0x01);                 	// DE = 1 Active  Display Function Control

	spi_send_cmd(0x30);
	spi_send_para(0x01);                 	// 480 X 854

	spi_send_cmd(0x31);
	spi_send_para(0x00);                 	//  inversion

	spi_send_cmd(0x50);
	spi_send_para(0xA0);                 	// VGMP

	spi_send_cmd(0x51);
	spi_send_para(0xA0);                 	// VGMN

	spi_send_cmd(0x57);
	spi_send_para(0x60);

	spi_send_cmd(0x60);
	spi_send_para(0x07);                 	// SDTI

	spi_send_cmd(0x61);
	spi_send_para(0x00);                	// CRTI

	spi_send_cmd(0x62);
	spi_send_para(0x07);                 	// EQTI

	spi_send_cmd(0x63);
	spi_send_para(0x00);                	// PCTI


	spi_send_cmd(0x40);
	spi_send_para(0x15);                 	// BT

	spi_send_cmd(0x41);
	spi_send_para(0x55);                 	// DDVDH/DDVDL  Clamp

	spi_send_cmd(0x42);
	spi_send_para(0x03);                	// VGH/VGL

	spi_send_cmd(0x43);
	spi_send_para(0x0A);                 	// VGH Clamp 16V

	spi_send_cmd(0x44);
	spi_send_para(0x06);                 	// VGL Clamp -10V

	spi_send_cmd(0x46);
	spi_send_para(0x55);

	//spi_send_cmd(0x52);
	//spi_send_para(0x00);                   //Flicker

	spi_send_cmd(0x53);
	spi_send_para(0x0C);                    //Flicker

	spi_send_cmd(0x55);
	spi_send_para(0x0C);

	spi_send_cmd(0x57);
	spi_send_para(0x50);

	//++++++++++++++++++ Gamma Setting ++++++++++++++++++//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x01);     				// Change to Page 1

	spi_send_cmd(0xA0);
	spi_send_para(0x0A);    				// Gamma 255

	spi_send_cmd(0xA1);
	spi_send_para(0x18);     				// Gamma 251

	spi_send_cmd(0xA2);
	spi_send_para(0x20);     				// Gamma 247

	spi_send_cmd(0xA3);
	spi_send_para(0x10);     				// Gamma 239

	spi_send_cmd(0xA4);
	spi_send_para(0x08);     				// Gamma 231

	spi_send_cmd(0xA5);
	spi_send_para(0x0E);  					// Gamma 203

	spi_send_cmd(0xA6);
	spi_send_para(0x08);    				// Gamma 175

	spi_send_cmd(0xA7);
	spi_send_para(0x05);    				// Gamma 147

	spi_send_cmd(0xA8);
	spi_send_para(0x08);  					// Gamma 108

	spi_send_cmd(0xA9);
	spi_send_para(0x0C);  					// Gamma 80

	spi_send_cmd(0xAA);
	spi_send_para(0x0F);  					// Gamma 52

	spi_send_cmd(0xAB);
	spi_send_para(0x08);  					// Gamma 24

	spi_send_cmd(0xAC);
	spi_send_para(0x11);  					// Gamma 16

	spi_send_cmd(0xAD);
	spi_send_para(0x14); 					// Gamma 8

	spi_send_cmd(0xAE);
	spi_send_para(0x0E);  					// Gamma 4

	spi_send_cmd(0xAF);
	spi_send_para(0x03);  					// Gamma 0

	///==============Nagitive
	spi_send_cmd(0xC0);
	spi_send_para(0x0F);  					// Gamma 0

	spi_send_cmd(0xC1);
	spi_send_para(0x18);  					// Gamma 4

	spi_send_cmd(0xC2);
	spi_send_para(0x20);  					// Gamma 8

	spi_send_cmd(0xC3);
	spi_send_para(0x10);  					// Gamma 16

	spi_send_cmd(0xC4);
	spi_send_para(0x08); 					// Gamma 24

	spi_send_cmd(0xC5);
	spi_send_para(0x0E);  					// Gamma 52

	spi_send_cmd(0xC6);
	spi_send_para(0x08);  					// Gamma 80

	spi_send_cmd(0xC7);
	spi_send_para(0x05);  					// Gamma 108

	spi_send_cmd(0xC8);
	spi_send_para(0x08); 					// Gamma 147

	spi_send_cmd(0xC9);
	spi_send_para(0x0C);  					// Gamma 175

	spi_send_cmd(0xCA);
	spi_send_para(0x0F);  					// Gamma 203

	spi_send_cmd(0xCB);
	spi_send_para(0x08);  					// Gamma 231

	spi_send_cmd(0xCC);
	spi_send_para(0x11);  					// Gamma 239

	spi_send_cmd(0xCD);
	spi_send_para(0x14);  					// Gamma 247

	spi_send_cmd(0xCE);
	spi_send_para(0x0E);  					// Gamma 251

	spi_send_cmd(0xCF);
	spi_send_para(0x00);  					// Gamma 255


	//****************************************************************************//
	//****************************** Page 6 Command ******************************//
	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x06);     				// Change to Page 6spi_send_cmd(0x00);spi_send_para(0x21
	spi_send_cmd(0x00);
	spi_send_para(0x21);
	spi_send_cmd(0x01);
	spi_send_para(0x06);
	spi_send_cmd(0x02);
	spi_send_para(0xA0);
	spi_send_cmd(0x03);
	spi_send_para(0x02);
	spi_send_cmd(0x04);
	spi_send_para(0x01);
	spi_send_cmd(0x05);
	spi_send_para(0x01);
	spi_send_cmd(0x06);
	spi_send_para(0x80);
	spi_send_cmd(0x07);
	spi_send_para(0x03);  //04
	spi_send_cmd(0x08);
	spi_send_para(0x060);
	spi_send_cmd(0x09);
	spi_send_para(0x80);
	spi_send_cmd(0x0A);
	spi_send_para(0x00);
	spi_send_cmd(0x0B);
	spi_send_para(0x00);
	spi_send_cmd(0x0C);
	spi_send_para(0x20);
	spi_send_cmd(0x0D);
	spi_send_para(0x20);
	spi_send_cmd(0x0E);
	spi_send_para(0x09);
	spi_send_cmd(0x0F);
	spi_send_para(0x00);
	spi_send_cmd(0x10);
	spi_send_para(0xFF);
	spi_send_cmd(0x11);
	spi_send_para(0xE0);
	spi_send_cmd(0x12);
	spi_send_para(0x00);
	spi_send_cmd(0x13);
	spi_send_para(0x00);
	spi_send_cmd(0x14);
	spi_send_para(0x00);
	spi_send_cmd(0x15);
	spi_send_para(0xC0);
	spi_send_cmd(0x16);
	spi_send_para(0x08);
	spi_send_cmd(0x17);
	spi_send_para(0x00);
	spi_send_cmd(0x18);
	spi_send_para(0x00);
	spi_send_cmd(0x19);
	spi_send_para(0x00);
	spi_send_cmd(0x1A);
	spi_send_para(0x00);
	spi_send_cmd(0x1B);
	spi_send_para(0x00);

	spi_send_cmd(0x1C);
	spi_send_para(0x00);
	spi_send_cmd(0x1D);
	spi_send_para(0x00);
	spi_send_cmd(0x20);
	spi_send_para(0x01);
	spi_send_cmd(0x21);
	spi_send_para(0x23);
	spi_send_cmd(0x22);
	spi_send_para(0x45);
	spi_send_cmd(0x23);
	spi_send_para(0x67);
	spi_send_cmd(0x24);
	spi_send_para(0x01);
	spi_send_cmd(0x25);
	spi_send_para(0x23);
	spi_send_cmd(0x26);
	spi_send_para(0x45);
	spi_send_cmd(0x27);
	spi_send_para(0x67);

	spi_send_cmd(0x30);
	spi_send_para(0x12);
	spi_send_cmd(0x31);
	spi_send_para(0x22);
	spi_send_cmd(0x32);
	spi_send_para(0x22);
	spi_send_cmd(0x33);
	spi_send_para(0x22);
	spi_send_cmd(0x34);
	spi_send_para(0x87);
	spi_send_cmd(0x35);
	spi_send_para(0x96);
	spi_send_cmd(0x36);
    spi_send_para(0xAA);
	spi_send_cmd(0x37);
	spi_send_para(0xDB);
	spi_send_cmd(0x38);
	spi_send_para(0xCC);
	spi_send_cmd(0x39);
	spi_send_para(0xBD);
	spi_send_cmd(0x3A);
	spi_send_para(0x78);
	spi_send_cmd(0x3B);
	spi_send_para(0x69);
	spi_send_cmd(0x3C);
	spi_send_para(0x22);
	spi_send_cmd(0x3D);
	spi_send_para(0x22);
	spi_send_cmd(0x3E);
	spi_send_para(0x22);
	spi_send_cmd(0x3F);
	spi_send_para(0x22);
	spi_send_cmd(0x40);
	spi_send_para(0x22);
	spi_send_cmd(0x52);
	spi_send_para(0x10);
	spi_send_cmd(0x53);
	spi_send_para(0x10);
    spi_send_cmd(0x54);
    spi_send_para(0x13);

	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x07);     			// Change to Page 7

	spi_send_cmd(0x02);
	spi_send_para(0x77);

	spi_send_cmd(0x18);
	spi_send_para(0x1D);
	spi_send_cmd(0x26);
	spi_send_para(0xB2);
	spi_send_cmd(0xE1);
	spi_send_para(0x79);

	spi_send_cmd(0x17);
	spi_send_para(0x22);
	spi_send_cmd(0xB3);
	spi_send_para(0x10);
	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x00);    			 // Change to Page 0

	spi_send_cmd(0x11);
    //spi_send_para(0x00);                 // Sleep-Out
    //delay_ms(0x120);
    usleep(120*1000);

	spi_send_cmd(0x29);
    //spi_send_para(0x00);                 // Display On
    usleep(10*1000);


    spi_send_cmd(0x2C);
    usleep(120*1000);


}
#endif
#ifdef CFG_SPI_LCD_854X480_DX050H049
static void spi_send_byte(uint8_t raw, bool dc)
{
    unsigned int i;
    uint16_t data = 0;
    if (dc)
        data |= 0x0100;
    data |= raw;

    for (i = 0; i < 9; i++)
    {
        CLK_CLR();
        delay(20);
        if (data & 0x0100)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
        delay(20);
        CLK_SET();
        delay(20);
        data <<= 1;
    }
    CLK_CLR();
}

static uint8_t spi_read_byte(uint8_t cmd)
{
    unsigned int i;
    uint8_t data = 0;
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    ithGpioSet(DATA_PORT);
    ithGpioSetIn(DATA_PORT);
    delay(5);
    for (i = 0; i < 8; i++)
    {
        CLK_CLR();
        delay(30);
        CLK_SET();
        delay(20);
        if (RXD_GET())
        {
            data |= 0x01;
        }
        data <<= 1;
    }
    CLK_CLR();
    ithGpioSetOut(DATA_PORT);
    CS_SET();
    delay(2);
    return data;
}

static void spi_send_cmd(uint8_t cmd)
{
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    CS_SET();
    delay(2);
}

static void spi_send_para(uint8_t para)
{
    CS_CLR();
    delay(10);
    spi_send_byte(para, true);
    CS_SET();
    delay(2);
}

void lcm_init_DX050H049()
{
    printf("Init LCM: lcm_init_DX050H049\r\n");
    spi_gpio_init();
    ithGpioSet(LCM_RST);
    usleep(10*1000);
    ithGpioClear(LCM_RST);
    usleep(10*1000);
    ithGpioSet(LCM_RST);
    usleep(120*1000);        // Delay 1 20ms

	//SINQ 9806E+HSD5.0

	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);


	spi_send_para(0x01);     // Change to Page 1

	spi_send_cmd(0x08);
	spi_send_para(0x10);    // output SDA

	spi_send_cmd(0x21);
	spi_send_para(0x01);  	// DE = 1 Active

	spi_send_cmd(0x30);
	spi_send_para(0x01);    // 480 X 854

	//spi_send_cmd(0x31);
	//spi_send_para(0x00);  // 2-dot Inversion
	spi_send_cmd(0x31);
	spi_send_para(0x00);

	//spi_send_cmd(0x40);
	//spi_send_para(0x1A);  // BT
	spi_send_cmd(0x40);
	spi_send_para(0x15);

	//spi_send_cmd(0x41);
	//spi_send_para(0x44);                // DDVDH/DDVDL 33
	spi_send_cmd(0x41);
	spi_send_para(0x33);

	//spi_send_cmd(0x42);
	//spi_send_para(0x01);               // VGH/VGL
	spi_send_cmd(0x42);
	spi_send_para(0x03);

	//spi_send_cmd(0x43);
	//spi_send_para(0x89);                // VGH_CP_OFF
	spi_send_cmd(0x43);
	spi_send_para(0x09);

	//spi_send_cmd(0x44);
	//spi_send_para(0x86);                // VGL_CP_OFF
	spi_send_cmd(0x44);
	spi_send_para(0x09);

	//增加
	spi_send_cmd(0x45);
	spi_send_para(0x16);                // VGL_REG

	//spi_send_cmd(0x50);
	//spi_send_para(0x98);      		调高内部压// VGMP
	spi_send_cmd(0x50);
	spi_send_para(0x68);    			//10

	//spi_send_cmd(0x51);
	//spi_send_para(0x98);     			调高内部压// VGMN
	spi_send_cmd(0x51);
	spi_send_para(0x68);    			//10

	spi_send_cmd(0x52);
	spi_send_para(0x00);                //Flicker

	spi_send_cmd(0x53);
	spi_send_para(0x45);                //Flicker

	spi_send_cmd(0x54);
	spi_send_para(0x00);

	spi_send_cmd(0x55);
	spi_send_para(0x4A);

	spi_send_cmd(0x57);
	spi_send_para(0x50);

	spi_send_cmd(0x60);
	spi_send_para(0x07);                 // SDTI

	spi_send_cmd(0x61);
	spi_send_para(0x00);                // CRTI

	spi_send_cmd(0x62);
	spi_send_para(0x08);                 // EQTI

	spi_send_cmd(0x63);
	spi_send_para(0x00);                // PCTI

	//++++++++++++++++++ Gamma Setting ++++++++++++++++++//
	// Change to Page 1
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x01);

	spi_send_cmd(0xA0);
	spi_send_para(0x00);  // Gamma 0

	spi_send_cmd(0xA1);
	spi_send_para(0x09);  // Gamma 4

	spi_send_cmd(0xA2);
	spi_send_para(0x0f);  // Gamma 8

	spi_send_cmd(0xA3);
	spi_send_para(0x0b);  // Gamma 16

	spi_send_cmd(0xA4);
	spi_send_para(0x06);  // Gamma 24

	spi_send_cmd(0xA5);
	spi_send_para(0x09);  // Gamma 52

	spi_send_cmd(0xA6);
	spi_send_para(0x07);  // Gamma 80

	spi_send_cmd(0xA7);
	spi_send_para(0x05);  // Gamma 108

	spi_send_cmd(0xA8);
	spi_send_para(0x08);  // Gamma 147

	spi_send_cmd(0xA9);
	spi_send_para(0x0c);  // Gamma 175

	spi_send_cmd(0xAA);
	spi_send_para(0x12);  // Gamma 203

	spi_send_cmd(0xAB);
	spi_send_para(0x08);  // Gamma 231

	spi_send_cmd(0xAC);
	spi_send_para(0x0D);  // Gamma 239

	spi_send_cmd(0xAD);
	spi_send_para(0x17);  // Gamma 247

	spi_send_cmd(0xAE);
	spi_send_para(0x0e);  // Gamma 251

	spi_send_cmd(0xAF);
	spi_send_para(0x00);  // Gamma 255

	///==============Nagitive
	spi_send_cmd(0xC0);
	spi_send_para(0x00);  // Gamma 0

	spi_send_cmd(0xC1);
	spi_send_para(0x08);  // Gamma 4

	spi_send_cmd(0xC2);
	spi_send_para(0x0e);  // Gamma 8

	spi_send_cmd(0xC3);
	spi_send_para(0x0b); // Gamma 16

	spi_send_cmd(0xC4);
	spi_send_para(0x05); // Gamma 24

	spi_send_cmd(0xC5);
	spi_send_para(0x09);  // Gamma 52

	spi_send_cmd(0xC6);
	spi_send_para(0x07);  // Gamma 80

	spi_send_cmd(0xC7);
	spi_send_para(0x04);  // Gamma 108

	spi_send_cmd(0xC8);
	spi_send_para(0x08);  // Gamma 147

	spi_send_cmd(0xC9);
	spi_send_para(0x0c);  // Gamma 175

	spi_send_cmd(0xCA);
	spi_send_para(0x011);  // Gamma 203

	spi_send_cmd(0xCB);
	spi_send_para(0x07); // Gamma 231

	spi_send_cmd(0xCC);
	spi_send_para(0x0d);  // Gamma 239

	spi_send_cmd(0xCD);
	spi_send_para(0x17);  // Gamma 247

	spi_send_cmd(0xCE);
	spi_send_para(0x0e);  // Gamma 251

	spi_send_cmd(0xCF);
	spi_send_para(0x00);  // Gamma 255

	//+++++++++++++++++++++++++++++++++++++++++++++++++++//

	//****************************************************************************//
	//****************************** Page 6 Command ******************************//
	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x06);     // Change to Page 6

	spi_send_cmd(0x00);
	spi_send_para(0x21);

	spi_send_cmd(0x01);
	spi_send_para(0x09);

	spi_send_cmd(0x02);
	spi_send_para(0x00);

	spi_send_cmd(0x03);
	spi_send_para(0x00);

	spi_send_cmd(0x04);
	spi_send_para(0x01);

	spi_send_cmd(0x05);
	spi_send_para(0x01);

	spi_send_cmd(0x06);
	spi_send_para(0x80);

	spi_send_cmd(0x07);
	spi_send_para(0x05);
	spi_send_cmd(0x08);
	spi_send_para(0x02);

	spi_send_cmd(0x09);
	spi_send_para(0x80);

	spi_send_cmd(0x0A);
	spi_send_para(0x00);

	spi_send_cmd(0x0B);
	spi_send_para(0x00);

	spi_send_cmd(0x0C);
	spi_send_para(0x0a);

	spi_send_cmd(0x0D);
	spi_send_para(0x0a);
	spi_send_cmd(0x0E);
	spi_send_para(0x00);

	spi_send_cmd(0x0F);
	spi_send_para(0x00);

	spi_send_cmd(0x10);
	spi_send_para(0xe0);

	spi_send_cmd(0x11);
	spi_send_para(0xe4);

	spi_send_cmd(0x12);
	spi_send_para(0x04);

	spi_send_cmd(0x13);
	spi_send_para(0x00);

	spi_send_cmd(0x14);
	spi_send_para(0x00);

	spi_send_cmd(0x15);
	spi_send_para(0xC0);

	spi_send_cmd(0x16);
	spi_send_para(0x08);

	spi_send_cmd(0x17);
	spi_send_para(0x00);

	spi_send_cmd(0x18);
	spi_send_para(0x00);

	spi_send_cmd(0x19);
	spi_send_para(0x00);

	spi_send_cmd(0x1A);
	spi_send_para(0x00);

	spi_send_cmd(0x1B);
	spi_send_para(0x00);

	spi_send_cmd(0x1C);
	spi_send_para(0x00);

	spi_send_cmd(0x1D);
	spi_send_para(0x00);

	spi_send_cmd(0x20);
	spi_send_para(0x01);  //02

	spi_send_cmd(0x21);
	spi_send_para(0x23); //13

	spi_send_cmd(0x22);
	spi_send_para(0x45);

	spi_send_cmd(0x23);
	spi_send_para(0x67);

	spi_send_cmd(0x24);
	spi_send_para(0x01);

	spi_send_cmd(0x25);
	spi_send_para(0x23);

	spi_send_cmd(0x26);
	spi_send_para(0x45);

	spi_send_cmd(0x27);
	spi_send_para(0x67);

	spi_send_cmd(0x30);
	spi_send_para(0x01);

	spi_send_cmd(0x31);
	spi_send_para(0x11);    //15
	spi_send_cmd(0x32);
	spi_send_para(0x00);  //22

	spi_send_cmd(0x33);
	spi_send_para(0xEE);

	spi_send_cmd(0x34);
	spi_send_para(0xFF);
	spi_send_cmd(0x35);
	spi_send_para(0xBB);

	spi_send_cmd(0x36);
	spi_send_para(0xCA);

	spi_send_cmd(0x37);
	spi_send_para(0xDD);

	spi_send_cmd(0x38);
	spi_send_para(0xAC);

	spi_send_cmd(0x39);
	spi_send_para(0x76);

	spi_send_cmd(0x3A);
	spi_send_para(0x67);

	spi_send_cmd(0x3B);
	spi_send_para(0x22);

	spi_send_cmd(0x3C);
	spi_send_para(0x22);

	spi_send_cmd(0x3D);
	spi_send_para(0x22);

	spi_send_cmd(0x3E);
	spi_send_para(0x22);

	spi_send_cmd(0x3F);
	spi_send_para(0x22);

	spi_send_cmd(0x40);
	spi_send_para(0x22);

	spi_send_cmd(0x52);
	spi_send_para(0x10);

	spi_send_cmd(0x53);
	spi_send_para(0x10);
	spi_send_cmd(0x54);
	spi_send_para(0x13);

	// Change to Page 7
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x07);

	spi_send_cmd(0x17);
	spi_send_para(0x22);

	spi_send_cmd(0x02);
	spi_send_para(0x77);

	spi_send_cmd(0xE1);
	spi_send_para(0x79);

	//****************************************************************************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x00);    // Change to Page 0
	spi_send_cmd(0x36);		//旋转180度
	spi_send_para(0x03);

	spi_send_cmd(0x11);
	spi_send_para(0x00);    // Sleep-Out
	usleep(120*1000);
	spi_send_cmd(0x29);
	spi_send_para(0x00);	//Display On
	usleep(10*1000);
}
#endif
#ifdef CFG_SPI_LCD_800X480_DX040B043
static void spi_send_byte(uint8_t raw, bool dc)
{
    unsigned int i;
    uint16_t data = 0;
    if (dc)
        data |= 0x0100;
    data |= raw;

    for (i = 0; i < 9; i++)
    {
        CLK_CLR();
        delay(20);
        if (data & 0x0100)
        {
            TXD_SET();
        }
        else
        {
            TXD_CLR();
        }
        delay(20);
        CLK_SET();
        delay(20);
        data <<= 1;
    }
    CLK_CLR();
}

static uint8_t spi_read_byte(uint8_t cmd)
{
    unsigned int i;
    uint8_t data = 0;
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    ithGpioSet(DATA_PORT);
    ithGpioSetIn(DATA_PORT);
    delay(5);
    for (i = 0; i < 8; i++)
    {
        CLK_CLR();
        delay(30);
        CLK_SET();
        delay(20);
        if (RXD_GET())
        {
            data |= 0x01;
        }
        data <<= 1;
    }
    CLK_CLR();
    ithGpioSetOut(DATA_PORT);
    CS_SET();
    delay(2);
    return data;
}

static void spi_send_cmd(uint8_t cmd)
{
    CS_CLR();
    delay(10);
    spi_send_byte(cmd, false);
    CS_SET();
    delay(2);
}

static void spi_send_para(uint8_t para)
{
    CS_CLR();
    delay(10);
    spi_send_byte(para, true);
    CS_SET();
    delay(2);
}

void lcm_init_DX040B043()
{
    printf("Init LCM: lcm_init_DX040B043\r\n");
    spi_gpio_init();
    ithGpioClear(LCM_RST);
    delay_ms(10);
    ithGpioSet(LCM_RST);
    delay_ms(20);        // Delay 1 20ms
    spi_send_cmd(0x28);
    delay_ms(20);
    spi_send_cmd(0x10);
    delay_ms(120);


	spi_send_cmd(0xFF);         //Change to page1 CMD
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x01);

	spi_send_cmd(0x08);         // Output SDA
	spi_send_para(0x10);

	spi_send_cmd(0x21);         // DE = 1 Active
	spi_send_para(0x01);

	spi_send_cmd(0x30);         // Resolution setting 480 X 800
	spi_send_para(0x02);

	spi_send_cmd(0x31);         // Inversion setting for for 2-dot
	spi_send_para(0x02);

	spi_send_cmd(0x60);         //
	spi_send_para(0x07);

	spi_send_cmd(0x61);         //
	spi_send_para(0x00);

	spi_send_cmd(0x62);         //
	spi_send_para(0x09);

	spi_send_cmd(0x63);         //
	spi_send_para(0x00);

	spi_send_cmd(0x40);         // Pump for DDVDH-L
	spi_send_para(0x14);

	spi_send_cmd(0x41);         // DDVDH/ DDVDL clamp
	spi_send_para(0x33);

	spi_send_cmd(0x42);         // VGH/VGL
	spi_send_para(0x01);

	spi_send_cmd(0x43);
	spi_send_para(0x03);

	spi_send_cmd(0x44);
	spi_send_para(0x09);

	spi_send_cmd(0x50);         // VREG1OUT VOLTAGE
	spi_send_para(0x60);

	spi_send_cmd(0x51);         // VREG2OUT VOLTAGE
	spi_send_para(0x60);

	spi_send_cmd(0x52);         // Flicker MSB
	spi_send_para(0x00);

	spi_send_cmd(0x53);         // Flicker LSB
	spi_send_para(0x52);         //50

	//++++++++++++++++++ Gamma Setting ++++++++++++++++++//

	spi_send_cmd(0xFF);         //Change to page1 CMD
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x01);

	spi_send_cmd(0xA0);         // Positive Gamma
	spi_send_para(0x00);

	spi_send_cmd(0xA1);
	spi_send_para(0x0c);

	spi_send_cmd(0xA2);
	spi_send_para(0x13);

	spi_send_cmd(0xA3);
	spi_send_para(0x0c);

	spi_send_cmd(0xA4);
	spi_send_para(0x04);

	spi_send_cmd(0xA5);
	spi_send_para(0x09);

	spi_send_cmd(0xA6);
	spi_send_para(0x07);

	spi_send_cmd(0xA7);
	spi_send_para(0x06);

	spi_send_cmd(0xA8);
	spi_send_para(0x06);

	spi_send_cmd(0xA9);
	spi_send_para(0x0a);

	spi_send_cmd(0xAA);
	spi_send_para(0x11);

	spi_send_cmd(0xAB);
	spi_send_para(0x08);

	spi_send_cmd(0xAC);
	spi_send_para(0x0c);

	spi_send_cmd(0xAD);
	spi_send_para(0x1b);

	spi_send_cmd(0xAE);
	spi_send_para(0x0d);

	spi_send_cmd(0xAF);
	spi_send_para(0x00);

	spi_send_cmd(0xC0);         // Negative Gamma
	spi_send_para(0x00);

	spi_send_cmd(0xC1);
	spi_send_para(0x0c);

	spi_send_cmd(0xC2);
	spi_send_para(0x13);

	spi_send_cmd(0xC3);
	spi_send_para(0x0c);

	spi_send_cmd(0xC4);
	spi_send_para(0x04);

	spi_send_cmd(0xC5);
	spi_send_para(0x09);

	spi_send_cmd(0xC6);
	spi_send_para(0x07);

	spi_send_cmd(0xC7);
	spi_send_para(0x06);

	spi_send_cmd(0xC8);
	spi_send_para(0x06);

	spi_send_cmd(0xC9);
	spi_send_para(0x0a);

	spi_send_cmd(0xCA);
	spi_send_para(0x11);

	spi_send_cmd(0xCB);
	spi_send_para(0x08);

	spi_send_cmd(0xCC);
	spi_send_para(0x0c);

	spi_send_cmd(0xCD);
	spi_send_para(0x1b);

	spi_send_cmd(0xCE);
	spi_send_para(0x0d);

	spi_send_cmd(0xCF);
	spi_send_para(0x00);
	 //***************Page 6 Command*************//
	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x06);         //Change to page6 for GIP timing

	spi_send_cmd(0x00);
	spi_send_para(0xa0);

	spi_send_cmd(0x01);
	spi_send_para(0x05);

	spi_send_cmd(0x02);
	spi_send_para(0x00);

	spi_send_cmd(0x03);
	spi_send_para(0x00);

	spi_send_cmd(0x04);
	spi_send_para(0x01);

	spi_send_cmd(0x05);
	spi_send_para(0x01);

	spi_send_cmd(0x06);
	spi_send_para(0x88);

	spi_send_cmd(0x07);
	spi_send_para(0x04);

	spi_send_cmd(0x08);
	spi_send_para(0x01);

	spi_send_cmd(0x09);
	spi_send_para(0x90);

	spi_send_cmd(0x0A);
	spi_send_para(0x04);

	spi_send_cmd(0x0B);
	spi_send_para(0x01);

	spi_send_cmd(0x0C);
	spi_send_para(0x01);

	spi_send_cmd(0x0D);
	spi_send_para(0x01);

	spi_send_cmd(0x0E);
	spi_send_para(0x00);

	spi_send_cmd(0x0F);
	spi_send_para(0x00);

	spi_send_cmd(0x10);
	spi_send_para(0x55);

	spi_send_cmd(0x11);
	spi_send_para(0x50);

	spi_send_cmd(0x12);
	spi_send_para(0x01);

	spi_send_cmd(0x13);
	spi_send_para(0x85);

	spi_send_cmd(0x14);
	spi_send_para(0x85);

	spi_send_cmd(0x15);
	spi_send_para(0xC0);

	spi_send_cmd(0x16);
	spi_send_para(0x0b);

	spi_send_cmd(0x17);
	spi_send_para(0x00);

	spi_send_cmd(0x18);
	spi_send_para(0x00);

	spi_send_cmd(0x19);
	spi_send_para(0x00);

	spi_send_cmd(0x1A);
	spi_send_para(0x00);

	spi_send_cmd(0x1B);
	spi_send_para(0x00);

	spi_send_cmd(0x1C);
	spi_send_para(0x00);

	spi_send_cmd(0x1D);
	spi_send_para(0x00);

	spi_send_cmd(0x20);
	spi_send_para(0x01);

	spi_send_cmd(0x21);
	spi_send_para(0x23);

	spi_send_cmd(0x22);
	spi_send_para(0x45);

	spi_send_cmd(0x23);
	spi_send_para(0x67);

	spi_send_cmd(0x24);
	spi_send_para(0x01);

	spi_send_cmd(0x25);
	spi_send_para(0x23);

	spi_send_cmd(0x26);
	spi_send_para(0x45);

	spi_send_cmd(0x27);
	spi_send_para(0x67);

	spi_send_cmd(0x30);
	spi_send_para(0x02);

	spi_send_cmd(0x31);
	spi_send_para(0x22);

	spi_send_cmd(0x32);
	spi_send_para(0x11);

	spi_send_cmd(0x33);
	spi_send_para(0xaa);

	spi_send_cmd(0x34);
	spi_send_para(0xbb);

	spi_send_cmd(0x35);
	spi_send_para(0x66);

	spi_send_cmd(0x36);
	spi_send_para(0x00);

	spi_send_cmd(0x37);
	spi_send_para(0x22);

	spi_send_cmd(0x38);
	spi_send_para(0x22);

	spi_send_cmd(0x39);
	spi_send_para(0x22);

	spi_send_cmd(0x3A);
	spi_send_para(0x22);

	spi_send_cmd(0x3B);
	spi_send_para(0x22);

	spi_send_cmd(0x3C);
	spi_send_para(0x22);

	spi_send_cmd(0x3D);
	spi_send_para(0x22);

	spi_send_cmd(0x3E);
	spi_send_para(0x22);

	spi_send_cmd(0x3F);
	spi_send_para(0x22);

	spi_send_cmd(0x40);
	spi_send_para(0x22);

	spi_send_cmd(0x52);
	spi_send_para(0x10);

	spi_send_cmd(0x53);
	spi_send_para(0x10);


	spi_send_cmd(0xFF);        // Change to Page 7 CMD for user command
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x07);

	spi_send_cmd(0x18);        //
	spi_send_para(0x1D);        //Default

	spi_send_cmd(0x17);        //
	spi_send_para(0x22);        //Default

	spi_send_cmd(0x02);        //
	spi_send_para(0x77);        //

	spi_send_cmd(0xE1);        //
	spi_send_para(0x79);        //

	spi_send_cmd(0xFF);
	spi_send_para(0xFF);
	spi_send_para(0x98);
	spi_send_para(0x06);
	spi_send_para(0x04);
	spi_send_para(0x00);         //Change to page0

	//spi_send_cmd(0x55);
	//spi_send_para(0xB0);

	spi_send_cmd(0x36);
	spi_send_para(0x03);
	spi_send_cmd(0x3a);
	spi_send_para(0x77);

	spi_send_cmd(0x20);

	spi_send_cmd(0x11);
	spi_send_para(0x00);                 // Sleep-Out
	delay_ms(0x120);
	spi_send_cmd(0x29);
	spi_send_para(0x00);                 // Display On
	delay_ms(0x10);

}
#endif

#ifdef CFG_UPGRADE_USB_DEVICE

static bool DetectUsbDeviceMode(void)
{
    bool ret = false;
    LOG_INFO "Detect USB device mode...\r\n" LOG_END
    
    // init usb
#if defined(CFG_USB0_ENABLE) || defined(CFG_USB1_ENABLE)
    itpRegisterDevice(ITP_DEVICE_USB, &itpDeviceUsb);
    if (ioctl(ITP_DEVICE_USB, ITP_IOCTL_INIT, NULL) != -1)
        usbInited = true;    
#endif

    // init usb device mode as mass storage device
#ifdef CFG_USBD_FILE_STORAGE
    if (usbInited)
    {
        int timeout = CFG_UPGRADE_USB_DETECT_TIMEOUT;
        
        itpRegisterDevice(ITP_DEVICE_USBDFSG, &itpDeviceUsbdFsg);
        ioctl(ITP_DEVICE_USBDFSG, ITP_IOCTL_INIT, NULL);
        
        while (!ret)
        {
            if (ioctl(ITP_DEVICE_USBDFSG, ITP_IOCTL_IS_CONNECTED, NULL))
            {
                ret = true;
                break;
            }

            timeout -= 10;
            if (timeout <= 0)
            {
                LOG_INFO "USB device mode not connected.\n" LOG_END
                break;
            }
            usleep(10000);
        }
    }
#endif // CFG_USBD_FILE_STORAGE
    return ret;
}
#endif // CFG_UPGRADE_USB_DEVICE

static void InitFileSystem(void)
{
    // init card device
#if  !defined(_WIN32) && (defined(CFG_SD0_ENABLE) || defined(CFG_SD1_ENABLE) || defined(CFG_CF_ENABLE) || defined(CFG_MS_ENABLE) || defined(CFG_XD_ENABLE) || defined(CFG_MSC_ENABLE) || defined(CFG_RAMDISK_ENABLE))
    itpRegisterDevice(ITP_DEVICE_CARD, &itpDeviceCard);
    ioctl(ITP_DEVICE_CARD, ITP_IOCTL_INIT, NULL);
#endif

    // init usb
#ifdef CFG_MSC_ENABLE
    if (!usbInited)
    {
        itpRegisterDevice(ITP_DEVICE_USB, &itpDeviceUsb);
        if (ioctl(ITP_DEVICE_USB, ITP_IOCTL_INIT, NULL) != -1)
            usbInited = true;
    }
#endif

    // init fat
#ifdef CFG_FS_FAT
    itpRegisterDevice(ITP_DEVICE_FAT, &itpFSDeviceFat.dev);
    ioctl(ITP_DEVICE_FAT, ITP_IOCTL_INIT, NULL);
    ioctl(ITP_DEVICE_FAT, ITP_IOCTL_ENABLE, NULL);
#endif

    // init drive table
#if defined(CFG_FS_FAT) || defined(CFG_FS_NTFS)
    itpRegisterDevice(ITP_DEVICE_DRIVE, &itpDeviceDrive);
    ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_INIT, NULL);

#ifdef CFG_MSC_ENABLE
    ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_ENABLE, NULL);
    ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_INIT_TASK, NULL);
#endif // CFG_MSC_ENABLE
#endif // defined(CFG_FS_FAT) || defined(CFG_FS_NTFS)

    // mount disks on booting
    ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_MOUNT, NULL);

#ifdef CFG_MSC_ENABLE
    // wait msc is inserted
    if (usbInited)
    {
        ITPDriveStatus* driveStatusTable;
        ITPDriveStatus* driveStatus = NULL;
        int i, timeout = CFG_UPGRADE_USB_DETECT_TIMEOUT;
        bool found = false;
        
        while (!found)
        {
            for (i = 0; i < 1; i++)
            {
                if (ioctl(ITP_DEVICE_USB, ITP_IOCTL_IS_CONNECTED, (void*)(USB0 + i)))
                {
                    found = true;
                    break;
                }
            }

            if (found)
            {
                break;
            }
            else
            {
                timeout -= 10;
                if (timeout <= 0)
                {
                    LOG_INFO "USB device not found.\n" LOG_END
                    return;
                }
                usleep(10000);
            }
        }

        found = false;
        timeout = CFG_UPGRADE_USB_TIMEOUT;

        ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_GET_TABLE, &driveStatusTable);
        
        while (!found)
        {
            for (i = 0; i < ITP_MAX_DRIVE; i++)
            {
                driveStatus = &driveStatusTable[i];
                if (driveStatus->disk >= ITP_DISK_MSC00 && driveStatus->disk <= ITP_DISK_MSC17 && driveStatus->avail)
                {
                    LOG_DBG "drive[%d]:usb disk=%d\n", i, driveStatus->disk LOG_END
                    found = true;
                }
            }
            if (!found)
            {
                timeout -= 100;
                if (timeout <= 0)
                {
                    LOG_INFO "USB disk not found.\n" LOG_END
                    break;
                }
                usleep(100000);
            }
        }
    }
#endif // CFG_MSC_ENABLE
}

#if !defined(CFG_LCD_ENABLE) && defined(CFG_CHIP_PKG_IT9910)
static void* UgLedTask(void* arg)
{
    int gpio_pin = 20;
    ithGpioSetOut(21);
    ithGpioSetMode(21,ITH_GPIO_MODE0);
    ithGpioSetOut(20);
    ithGpioSetMode(20,ITH_GPIO_MODE0);
	stop_led = false;
	
    for(;;)
    {
    	if(stop_led == true)
    	{
    		ithGpioSet(20); 
			ithGpioSet(21);
			while(1)
				usleep(500000);
    	}
        ithGpioClear(gpio_pin);
        if(gpio_pin==21)
            gpio_pin = 20;
        else
            gpio_pin = 21;
        ithGpioSet(gpio_pin); 
        usleep(500000);
    }
}
#endif

#if defined(CFG_CHIP_PKG_IT9910)
static void DetectKey(void)
{
    int ret;
    int phase = 0;
    int time_counter = 0;
    int key_counter = 0;
    bool key_pressed;
    bool key_released;
    ITPKeypadEvent ev;

    while (1)
    {
        key_pressed = key_released = false;
        ioctl(ITP_DEVICE_KEYPAD, ITP_IOCTL_PROBE, NULL);
        if (read(ITP_DEVICE_KEYPAD, &ev, sizeof (ITPKeypadEvent)) == sizeof (ITPKeypadEvent))
        {
            if (ev.code == 0)
            {
                if (ev.flags & ITP_KEYPAD_DOWN)
                    key_pressed = true;
                else if (ev.flags & ITP_KEYPAD_UP)
                    key_released = true;
            }
        }

        if (phase == 0)
        {
            if (key_pressed)
            {
                printf("key detected\n");
                phase = 1;
            }
            else
                break;
        }
        else if (phase == 1)
        {
            if (key_released)
                break;
            if (time_counter > 100)
            {
                phase = 2;
                ithGpioSetOut(21);
                ithGpioSetMode(21, ITH_GPIO_MODE0);
                ithGpioSetOut(20);
                ithGpioSetMode(20, ITH_GPIO_MODE0);
            }
        }
        else if (phase == 2)
        {
            if (key_pressed)
            {
                ithGpioSet(20);
                key_counter++;
            }
            if (key_released)
                ithGpioClear(20);

            if (time_counter > 200)
            {
                ithGpioSet(21);
                ithGpioClear(20);
                phase = 3;
            }

            // blink per 6*50000 us
            if ((time_counter/6)%2)
                ithGpioSet(21);
            else
                ithGpioClear(21);
        }
        else if (phase == 3)
        {
            printf("key_counter: %d\n", key_counter);
            if (key_counter == 1)
            {
                // do reset
                InitFileSystem();
                ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_MOUNT, NULL);
                ret = ugResetFactory();
				#if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
				ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
				#endif
                exit(ret);
            }
            if (key_counter == 2)
            {
                // dump addressbook.xml
                InitFileSystem();
                ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_MOUNT, NULL);
                CopyUclFile();
            }
            ithGpioClear(21);
            break;
        }

        usleep(50000);
        time_counter++;
    }
}
#endif

#if defined (CFG_ENABLE_UART_CLI)
static int parseCommand(char* str, int strlength)
{

	int ret;
	
	if (strncmp(str, "boot", 4) == 0)
	{
		printf("going to boot procedure\n");
		boot = true;
	}

	if (strncmp(str, "update", 6) == 0)
	{
		ITCStream* upgradeFile;
		uint8_t* imagebuf;
		uint32_t imagesize;

		upgradeFile = OpenRecoveryPackage();	
		if (upgradeFile)
		{			
			BootBin(upgradeFile);
			return 1;
		}
	}

	if (strncmp(str, "upgrade", 7) == 0)
	{
		ITCStream* upgradeFile;
		
		upgradeFile = OpenRecoveryPackage();
        if (upgradeFile)
        {
            if (ugCheckCrc(upgradeFile, NULL))
                LOG_ERR "Upgrade failed.\n" LOG_END
            else
                ret = ugUpgradePackage(upgradeFile);

            #if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
                LOG_INFO "Flushing NOR cache...\n" LOG_END
                ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
            #endif

            if (ret)
                LOG_INFO "Upgrade failed.\n" LOG_END
            else
                LOG_INFO "Upgrade finished.\n" LOG_END

            exit(ret);
        }
        else
        {
        #ifdef CFG_UPGRADE_RECOVERY_LED
            ioctl(fd, ITP_IOCTL_OFF, NULL);
        #endif
        }
		while (1);
	}

	if (strncmp(str, "setenv", 6) == 0)
	{
		char para[128] = {0};
		int i = 0;
		
		strncpy(para, str+7, strlength -7);		
		memset(tftppara, 0 , 128);
		strcpy(tftppara, para);
		printf("\ntftppara=%s\n", tftppara);
	}

	
	return 0;
}

static void CommandReciver(void)
{
	char rcvbuf[128], cmdbuf[128];
	static int wp = 0;
	int fd, len, i, strlength;	

#if defined (CFG_UART0_ENABLE) && defined(CFG_DBG_UART0)
	itpRegisterDevice(ITP_DEVICE_UART0, &itpDeviceUart0);
	ioctl(ITP_DEVICE_UART0, ITP_IOCTL_RESET, (void*)CFG_UART0_BAUDRATE);
#elif defined (CFG_UART1_ENABLE) && defined(CFG_DBG_UART1)
	itpRegisterDevice(ITP_DEVICE_UART1, &itpDeviceUart1);
	ioctl(ITP_DEVICE_UART1, ITP_IOCTL_RESET, (void*)CFG_UART1_BAUDRATE);
#elif defined (CFG_UART2_ENABLE) && defined(CFG_DBG_UART2)
	itpRegisterDevice(ITP_DEVICE_UART2, &itpDeviceUart2);
	ioctl(ITP_DEVICE_UART2, ITP_IOCTL_RESET, (void*)CFG_UART2_BAUDRATE);
#elif defined (CFG_UART3_ENABLE) && defined(CFG_DBG_UART3)
	itpRegisterDevice(ITP_DEVICE_UART3, &itpDeviceUart3);
	ioctl(ITP_DEVICE_UART3, ITP_IOCTL_RESET, (void*)CFG_UART3_BAUDRATE);
#endif

	fd = open(UART_PORT, O_RDONLY);
	
	if(fd < 0)	
		return;

	for(;;)
	{
	
		memset(rcvbuf, 0, 128);
		len = read(fd, rcvbuf, 128);

		if (len)
		{
			for (i = 0; i < len; i++)
			{			
				cmdbuf[wp] = rcvbuf[i];								
				wp++;					
				if (rcvbuf[i] == '\0')
				{					
					strlength = strlen(cmdbuf);					
					parseCommand(cmdbuf, strlength);
					memset(cmdbuf, 0, 128);
					wp = 0;						
				}		
			}
		}
		if(boot)
			break;
			
	}
	printf("Exit CommandReciver\n");
}
#endif
static void DoUpgrade(void)
{
    ITCStream* upgradeFile;

    LOG_INFO "Do Upgrade...\r\n" LOG_END

    upgradeFile = OpenUpgradePackage();
    if (upgradeFile)
    {
        int ret;

    #if !defined(CFG_LCD_ENABLE) && defined(CFG_CHIP_PKG_IT9910)
        //---light on red/green led task
        pthread_t task;
        pthread_create(&task, NULL, UgLedTask, NULL);
        //------
    #endif    
        // output messages to LCD console
    #if defined(CFG_LCD_ENABLE) && defined(CFG_BL_LCD_CONSOLE)
        if (!blLcdOn)
        {
        #if !defined(CFG_BL_SHOW_LOGO)
            extern uint32_t __lcd_base_a;
            extern uint32_t __lcd_base_b;
            extern uint32_t __lcd_base_c;
        
            itpRegisterDevice(ITP_DEVICE_SCREEN, &itpDeviceScreen);
            ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_RESET, NULL);
            ithLcdSetBaseAddrA((uint32_t) &__lcd_base_a);
            ithLcdSetBaseAddrB((uint32_t) &__lcd_base_b);        
        
        #ifdef CFG_BACKLIGHT_ENABLE
            itpRegisterDevice(ITP_DEVICE_BACKLIGHT, &itpDeviceBacklight);
            ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_INIT, NULL);
        #endif // CFG_BACKLIGHT_ENABLE        
        
        #endif // !defined(CFG_BL_SHOW_LOGO)
            
            ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
            ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
            blLcdOn = true;
        }
        if (!blLcdConsoleOn)
        {
            itpRegisterDevice(ITP_DEVICE_STD, &itpDeviceLcdConsole);
            itpRegisterDevice(ITP_DEVICE_LCDCONSOLE, &itpDeviceLcdConsole);
            ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_INIT, NULL);
            ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_CLEAR, NULL);
            blLcdConsoleOn = true;
        }
    #endif // defined(CFG_LCD_ENABLE) && defined(BL_LCD_CONSOLE)

        if (ugCheckCrc(upgradeFile, NULL))
            LOG_ERR "Upgrade failed.\n" LOG_END
        else
            ret = ugUpgradePackage(upgradeFile);

    #ifdef CFG_UPGRADE_DELETE_PKGFILE_AFTER_FINISH
        DeleteUpgradePackage();
    #endif

        #if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
            LOG_INFO "Flushing NOR cache...\n" LOG_END
            ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
        #endif

        if (ret)
            LOG_INFO "Upgrade failed.\n" LOG_END
        else
        {
        	stop_led = true;
            LOG_INFO "Upgrade finished.\n" LOG_END
        }
    #if defined(CFG_UPGRADE_DELAY_AFTER_FINISH) && CFG_UPGRADE_DELAY_AFTER_FINISH > 0
        sleep(CFG_UPGRADE_DELAY_AFTER_FINISH);
    #endif

        exit(ret);
        while (1);
    }
}

void* BootloaderMain(void* arg)
{
    int ret;

#if defined(CFG_UPGRADE_PRESSKEY) || defined(CFG_UPGRADE_RESET_FACTORY) || defined(CFG_UPGRADE_RECOVERY)
    ITPKeypadEvent ev;
#endif

#ifdef CFG_WATCHDOG_ENABLE
    ioctl(ITP_DEVICE_WATCHDOG, ITP_IOCTL_DISABLE, NULL);
#endif

    ithMemDbgDisable(ITH_MEMDBG0);
    ithMemDbgDisable(ITH_MEMDBG1);

#ifdef CFG_BL_SHOW_LOGO
    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
    ShowLogo();
#ifdef CFG_BACKLIGHT_ENABLE
#ifdef CFG_SPI_LCD_800X480_NT35510
		printf("Init NT35510\r\n");
		spi_gpio_init();
		lcd_init();
#endif
#ifdef CFG_SPI_LCD_854X480_RM7701
		printf("init rm7701\r\n");
		spi_gpio_init();
		test_lcm_st7701();
#endif
#ifdef CFG_SPI_LCD_854X480_ST7701_DX050C023
		spi_gpio_init();
		lcm_init_ST7701_DX050C023();
#endif
#ifdef CFG_SPI_LCD_800X480_OTM8019A
		spi_gpio_init();
		printf("OTM8019A!!!!!!!!!!!\n");
		lcm_init_OTM8019A();
#endif	
#ifdef CFG_SPI_LCD_854X480_ILI9806E
		spi_gpio_init();
		printf("ILI9806E!!!!!!!!!!!\n");
		lcm_init_ILI9806E();
#endif
#ifdef CFG_SPI_LCD_854X480_DX050H049
	spi_gpio_init();
	printf("DX050H049!!!!!!!!!!!\n");
	lcm_init_DX050H049();
#endif
#ifdef CFG_SPI_LCD_854X480_DX040B043
		lcm_init_DX040B043();
#endif	
#ifdef CFG_SPI_LCD_800X480_RM68180
		spi_gpio_init();
		printf("init rm68180\r\n");
		test_lcm_rm68180();
#endif
    usleep(100000);
    ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
#endif // CFG_BACKLIGHT_ENABLE
    blLcdOn = true;
#endif // CFG_BL_SHOW_LOGO

#ifdef CFG_UPGRADE_USB_DEVICE
    if (DetectUsbDeviceMode())
    {
		ITCStream* upgradeFile = OpenUsbDevicePackage();
        if (upgradeFile)
        {
            if (ugCheckCrc(upgradeFile, NULL))
                LOG_ERR "Upgrade failed.\n" LOG_END
            else
                ret = ugUpgradePackage(upgradeFile);

            #if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
                LOG_INFO "Flushing NOR cache...\n" LOG_END
                ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
            #endif

            if (ret)
                LOG_INFO "Upgrade failed.\n" LOG_END
            else
                LOG_INFO "Upgrade finished.\n" LOG_END

            exit(ret);
        }
    }
#endif // CFG_UPGRADE_USB_DEVICE

#if defined(CFG_UPGRADE_PRESSKEY) || defined(CFG_UPGRADE_RESET_FACTORY) || defined(CFG_UPGRADE_RECOVERY)
    ioctl(ITP_DEVICE_KEYPAD, ITP_IOCTL_PROBE, NULL);
    if (read(ITP_DEVICE_KEYPAD, &ev, sizeof (ITPKeypadEvent)) == sizeof (ITPKeypadEvent))
    {
        int key = ev.code, delay = 0;

#endif // defined(CFG_UPGRADE_PRESSKEY) || defined(CFG_UPGRADE_RESET_FACTORY) || defined(CFG_UPGRADE_RECOVERY)

    #ifdef CFG_UPGRADE_RECOVERY
        if (key == CFG_UPGRADE_RECOVERY_PRESSKEY_NUM)
        {
            struct timeval time = ev.time;

            // detect key pressed time
            for (;;)
            {
                if (ev.flags & ITP_KEYPAD_UP)
                    break;

                ioctl(ITP_DEVICE_KEYPAD, ITP_IOCTL_PROBE, NULL);
                if (read(ITP_DEVICE_KEYPAD, &ev, sizeof (ITPKeypadEvent)) == 0)
                    continue;

                delay += itpTimevalDiff(&time, &ev.time);
                time = ev.time;

                LOG_DBG "recovery key: time=%ld.%ld,code=%d,down=%d,up=%d,repeat=%d,delay=%d\r\n",
                    ev.time.tv_sec,
                    ev.time.tv_usec / 1000,
                    ev.code,
                    (ev.flags & ITP_KEYPAD_DOWN) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_UP) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_REPEAT) ? 1 : 0,
                    delay
                LOG_END

                if (delay >= CFG_UPGRADE_RECOVERY_PRESSKEY_DELAY)
                    break;
            };

            if (delay >= CFG_UPGRADE_RECOVERY_PRESSKEY_DELAY)
            {
                ITCStream* upgradeFile;
            #ifdef CFG_UPGRADE_RECOVERY_LED
                int fd = open(":led:" CFG_UPGRADE_RECOVERY_LED_NUM, O_RDONLY);
                ioctl(fd, ITP_IOCTL_FLICKER, (void*)500);
            #endif

                // output messages to LCD console
            #if defined(CFG_LCD_ENABLE) && defined(CFG_BL_LCD_CONSOLE)
                if (!blLcdConsoleOn)
                {
                    itpRegisterDevice(ITP_DEVICE_STD, &itpDeviceLcdConsole);
                    itpRegisterDevice(ITP_DEVICE_LCDCONSOLE, &itpDeviceLcdConsole);
                    ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_INIT, NULL);
                    ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_CLEAR, NULL);
                    blLcdConsoleOn = true;
                }
                if (!blLcdOn)
                {
                    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
                    ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
                    blLcdOn = true;
                }
            #endif // defined(CFG_LCD_ENABLE) && defined(BL_LCD_CONSOLE)

                LOG_INFO "Do Recovery...\r\n" LOG_END

                InitFileSystem();

                upgradeFile = OpenRecoveryPackage();
                if (upgradeFile)
                {
                    if (ugCheckCrc(upgradeFile, NULL))
                        LOG_ERR "Recovery failed.\n" LOG_END
                    else
                        ret = ugUpgradePackage(upgradeFile);

                    #if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
                        LOG_INFO "Flushing NOR cache...\n" LOG_END
                        ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
                    #endif

                    if (ret)
                        LOG_INFO "Recovery failed.\n" LOG_END
                    else
                        LOG_INFO "Recovery finished.\n" LOG_END

                    exit(ret);
                }
                else
                {
                #ifdef CFG_UPGRADE_RECOVERY_LED
                    ioctl(fd, ITP_IOCTL_OFF, NULL);
                #endif
                }
                while (1);
            }
        }
    #endif // CFG_UPGRADE_RECOVERY

    #ifdef CFG_UPGRADE_RESET_FACTORY
        if (key == CFG_UPGRADE_RESET_FACTORY_PRESSKEY_NUM)
        {
            struct timeval time = ev.time;

            // detect key pressed time
            for (;;)
            {
                if (ev.flags & ITP_KEYPAD_UP)
                    break;

                ioctl(ITP_DEVICE_KEYPAD, ITP_IOCTL_PROBE, NULL);
                if (read(ITP_DEVICE_KEYPAD, &ev, sizeof (ITPKeypadEvent)) == 0)
                    continue;

                delay += itpTimevalDiff(&time, &ev.time);
                time = ev.time;

                LOG_DBG "reset key: time=%ld.%ld,code=%d,down=%d,up=%d,repeat=%d,delay=%d\r\n",
                    ev.time.tv_sec,
                    ev.time.tv_usec / 1000,
                    ev.code,
                    (ev.flags & ITP_KEYPAD_DOWN) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_UP) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_REPEAT) ? 1 : 0,
                    delay
                LOG_END

                if (delay >= CFG_UPGRADE_RESET_FACTORY_PRESSKEY_DELAY)
                    break;
            };

            if (delay >= CFG_UPGRADE_RESET_FACTORY_PRESSKEY_DELAY)
            {
                // output messages to LCD console
            #if defined(CFG_LCD_ENABLE) && defined(CFG_BL_LCD_CONSOLE)
                if (!blLcdConsoleOn)
                {
                    itpRegisterDevice(ITP_DEVICE_STD, &itpDeviceLcdConsole);
                    itpRegisterDevice(ITP_DEVICE_LCDCONSOLE, &itpDeviceLcdConsole);
                    ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_INIT, NULL);
                    ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_CLEAR, NULL);
                    blLcdConsoleOn = true;
                }
                if (!blLcdOn)
                {
                    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
                    ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
                    blLcdOn = true;
                }
            #endif // defined(CFG_LCD_ENABLE) && defined(BL_LCD_CONSOLE)

                LOG_INFO "Do Reset...\r\n" LOG_END

                InitFileSystem();
                ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_MOUNT, NULL);

                ret = ugResetFactory();

            #if defined(CFG_NOR_ENABLE) && CFG_NOR_CACHE_SIZE > 0
                LOG_INFO "Flushing NOR cache...\n" LOG_END
                ioctl(ITP_DEVICE_NOR, ITP_IOCTL_FLUSH, NULL);
            #endif

                if (ret)
                    LOG_INFO "Reset failed.\n" LOG_END
                else
                    LOG_INFO "Reset finished.\n" LOG_END

                exit(ret);
                while (1);
            }
        }
    #endif // CFG_UPGRADE_RESET_FACTORY

    #ifdef CFG_UPGRADE_PRESSKEY
        if (key == CFG_UPGRADE_PRESSKEY_NUM)
        {
            struct timeval time = ev.time;

            // detect key pressed time
            for (;;)
            {
                if (ev.flags & ITP_KEYPAD_UP)
                    break;

                ioctl(ITP_DEVICE_KEYPAD, ITP_IOCTL_PROBE, NULL);
                if (read(ITP_DEVICE_KEYPAD, &ev, sizeof (ITPKeypadEvent)) == 0)
                    continue;

                delay += itpTimevalDiff(&time, &ev.time);
                time = ev.time;

                LOG_DBG "upgrade key: time=%ld.%ld,code=%d,down=%d,up=%d,repeat=%d,delay=%d\r\n",
                    ev.time.tv_sec,
                    ev.time.tv_usec / 1000,
                    ev.code,
                    (ev.flags & ITP_KEYPAD_DOWN) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_UP) ? 1 : 0,
                    (ev.flags & ITP_KEYPAD_REPEAT) ? 1 : 0,
                    delay
                LOG_END

                if (delay >= CFG_UPGRADE_PRESSKEY_DELAY)
                    break;
            };

            if (delay >= CFG_UPGRADE_PRESSKEY_DELAY)
            {
                InitFileSystem();
                DoUpgrade();
            }
        }
    #endif // CFG_UPGRADE_PRESSKEY
#if defined(CFG_UPGRADE_PRESSKEY) || defined(CFG_UPGRADE_RESET_FACTORY) || defined(CFG_UPGRADE_RECOVERY)
    }
#endif

#if defined(CFG_CHIP_PKG_IT9910)
	DetectKey();
#endif

#if !defined(CFG_UPGRADE_PRESSKEY) && defined(CFG_UPGRADE_OPEN_FILE)
    InitFileSystem();
    DoUpgrade();
#if defined (CFG_ENABLE_UART_CLI)	
	CommandReciver();
#endif
#endif

#ifdef CFG_UPGRADE_BACKUP_PACKAGE
	if (ugUpgradeCheck())
    {
        // output messages to LCD console
    #if defined(CFG_LCD_ENABLE) && defined(CFG_BL_LCD_CONSOLE)
        if (!blLcdConsoleOn)
        {
            itpRegisterDevice(ITP_DEVICE_STD, &itpDeviceLcdConsole);
            itpRegisterDevice(ITP_DEVICE_LCDCONSOLE, &itpDeviceLcdConsole);
            ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_INIT, NULL);
            ioctl(ITP_DEVICE_LCDCONSOLE, ITP_IOCTL_CLEAR, NULL);
            blLcdConsoleOn = true;
        }
        if (!blLcdOn)
        {
            ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
            ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);
            blLcdOn = true;
        }
    #endif // defined(CFG_LCD_ENABLE) && defined(BL_LCD_CONSOLE)
                    
        LOG_INFO "Last upgrade failed, try to restore from internal package...\r\n" LOG_END        

        // init fat
    #ifdef CFG_FS_FAT
        itpRegisterDevice(ITP_DEVICE_FAT, &itpFSDeviceFat.dev);
        ioctl(ITP_DEVICE_FAT, ITP_IOCTL_INIT, NULL);
        ioctl(ITP_DEVICE_FAT, ITP_IOCTL_ENABLE, NULL);
    #endif

        // mount disks on booting
        ioctl(ITP_DEVICE_DRIVE, ITP_IOCTL_MOUNT, NULL);

        RestorePackage();
    }
#endif // CFG_UPGRADE_BACKUP_PACKAGE

#ifdef CFG_BL_SHOW_VIDEO
    itpInit();
    ioctl(ITP_DEVICE_SCREEN, ITP_IOCTL_POST_RESET, NULL);
    ioctl(ITP_DEVICE_BACKLIGHT, ITP_IOCTL_RESET, NULL);

    ituLcdInit(); //init itu
    
#ifdef CFG_M2D_ENABLE
    ituM2dInit();
#ifdef CFG_VIDEO_ENABLE
    ituFrameFuncInit();
#endif // CFG_VIDEO_ENABLE
#else
    ituSWInit();
#endif // CFG_M2D_ENABLE

    PlayVideo();
    WaitPlayVideoFinish();
#endif //CFG_BL_SHOW_VIDEO

    LOG_INFO "Do Booting...\r\n" LOG_END

    BootImage();

    return NULL;
}
/*
#if (CFG_CHIP_PKG_IT9079)
void
ithCodecPrintfWrite(
    char* string,
    int length)
{
    // this is a dummy function for linking with library itp_boot. (itp_swuart_codec.c)
}

#endif
*/
