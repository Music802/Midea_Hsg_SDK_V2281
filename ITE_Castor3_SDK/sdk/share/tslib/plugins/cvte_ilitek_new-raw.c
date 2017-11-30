#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <pthread.h>
#include "ite/ith.h"
#include "ite/itp.h"
#include "config.h"
#include "tslib-private.h"
#include "cvte_ilitek_new-raw.h"

#ifdef __OPENRTOS__
#include "openrtos/FreeRTOS.h"		//must "include FreeRTOS.h" before "include queue.h"
#include "openrtos/queue.h"			//for using xQueue function
#endif

//#define EN_DISABLE_ALL_INTR
#define	ENABLE_PATCH_Y_SHIFT_ISSUE
/****************************************************************************
 * ENABLE_TOUCH_POSITION_MSG :: just print X,Y coordination &
 * 								touch-down/touch-up
 * ENABLE_TOUCH_IIC_DBG_MSG  :: show the IIC command
 * ENABLE_TOUCH_PANEL_DBG_MSG:: show send-queue recieve-queue,
 *                              and the xy value of each INTr
 ****************************************************************************/

//#define ENABLE_TOUCH_POSITION_MSG
//#define ENABLE_TOUCH_PANEL_DBG_MSG
//#define ENABLE_TOUCH_IIC_DBG_MSG
//#define ENABLE_SEND_FAKE_SAMPLE


/****************************************************************************
 * 0.pre-work(build a test project, set storage for tslib config file)
 * 1.INT pin response(OK)
 * 2.IIC send read/write command, register address, and response
 * 3.get x,y coordiantion(palse data buffer)
 * 4.celibration(rotate, scale, reverse...)
 * 5.get 4 corner touch point(should be (0,0)(800,0)(0,600)(800,600) )
 * 6.add interrupt, thread, Xqueue, Timer..
 * 7.check interrupt response time(NO event loss)
 * 8.check sample rate (33ms)
 * 9.check touch down/up event
 * 10.draw line test
 * 11.check IIC 400kHz and IIC acess procedure(sleep 300~500us)
 ****************************************************************************/

#ifdef BUILD_FOUR_INCH
//#define ENABLE_SWITCH_XY
//#define ENABLE_SCALE_X
//#define ENABLE_SCALE_Y
//#define ENABLE_REVERSE_X
//#define ENABLE_REVERSE_Y
#else
//#define ENABLE_SWITCH_XY
//#define ENABLE_SCALE_X
//#define ENABLE_SCALE_Y
#define ENABLE_REVERSE_X
//#define ENABLE_REVERSE_Y
#endif

static int g_xResolution = CFG_TOUCH_X_MAX_VALUE;
static int g_yResolution = CFG_TOUCH_Y_MAX_VALUE;


/***************************
 *
 **************************/
//add by cvte_zxl {
#define TP_GPIO_PIN			CFG_GPIO_TOUCH_INT
//170914 add by cvte_hcy{
#define TP_RST_GPIO_PIN     CFG_GPIO_TOUCH_RESET

//170914 add by cvte_hcy}
//add by cvte_zxl }

#ifdef	CFG_GPIO_TOUCH_WAKE
#define TP_GPIO_WAKE_PIN	CFG_GPIO_TOUCH_WAKE
#endif

#if (TP_GPIO_PIN<32)
#define TP_GPIO_MASK    (1<<TP_GPIO_PIN)
#else
#define TP_GPIO_MASK    (1<<(TP_GPIO_PIN-32))
#endif


#define TOUCH_DEVICE_ID     (0x70 >> 1)  //7位地址
#define QUEUE_LEN 			(256)
#define TOUCH_SAMPLE_RATE_200ms (200)
#define TOUCH_SAMPLE_RATE_100ms	(100)
#define TOUCH_SAMPLE_RATE_50ms	(50)
#define TOUCH_SAMPLE_RATE_66ms	(66)
#define TOUCH_SAMPLE_RATE_33ms	(33)
#define TOUCH_SAMPLE_RATE_16ms	(16)
#define TOUCH_SAMPLE_RATE_10ms	(10)
#define TOUCH_SAMPLE_RATE_1ms	(1)

#define	TOUCH_NO_CONTACT		(0)
#define	TOUCH_DOWN				(1)
#define	TOUCH_UP				(2)

/***************************
 *
 **************************/
//add by cvte_zxl {
struct ilitek_tp {
	int protocol_ver;		//protocol version
	int max_x; 				// maximum x
	int max_y;				// maximum y
	int min_x;				// minimum x
	int min_y;				// minimum y
	int max_tp;				// maximum touch point
	int max_btn;			// maximum key button
	int x_ch;				// the total number of x channel
	int y_ch;				// the total number of y channel
	int keycount;
};

struct ilitek_tp cvte_ilitek_tp;
//add by cvte_zxl }
static short g_currIntr=0;
static char g_1stSampHasSend = 0;
static char g_TouchStatus = TOUCH_NO_CONTACT;	//0:normal, 1:down, 2:up

//#ifdef CFG_TOUCH_INTR
static char g_TouchDownIntr = false;
//#endif

static char g_IsTpInitialized = false;
static short lastx=0;
static short lasty=0;
static short lastp=0;

static unsigned int dur=0;
struct timeval startT, endT;
struct timeval T1, T2;

static int g_tchDevFd=0;

#ifdef __OPENRTOS__
static QueueHandle_t tpQueue;
static pthread_mutex_t 	gTpMutex;
#endif

#ifdef	ENABLE_SEND_FAKE_SAMPLE
static int g_tpCntr = 0;
static unsigned char MAX_FAKE_NUM = 31;
static int gFakeTableX[] = {688,100,100,100,100,100,562,436,100,100,100,100,100,310,200,100};
static int gFakeTableY[] = {406,250,200,160,120, 70,406,406,250,200,160,120, 70,406,406,406};
//static int gFakeTableX[] = {688,100,100,100,100,100,562,436,100,100,100,100,100,310,200,100,100,100,100,688,562,436,310,200,100,688,562,436,310,200,100};
//static int gFakeTableY[] = {406,250,200,160,120, 70,406,406,250,200,160,120, 70,406,406,406,160,120, 70,406,406,406,406,406,406,406,406,406,406,406,406};
#endif

//17.5.19 hcy
bool isLargePitch = false;
//17.6.12 tqw
unsigned char revArrSum = 0;
long revArrLen = 0;

/*##################################################################################
 *                        the private function implementation
 ###################################################################################*/
#ifdef CFG_TOUCH_INTR
void _tp_isr(void* data)
{
	unsigned int regValue;

	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	//ithPrintf("$in\n");
	#endif

	regValue = ithGpioGet(TP_GPIO_PIN);
	if ( (regValue & TP_GPIO_MASK) )
	{
		g_TouchDownIntr = false;
	}
	else
	{
		g_TouchDownIntr = true;
	}

    ithGpioClearIntr(TP_GPIO_PIN);

	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	//ithPrintf("$out(%x,%x)\n",g_TouchDownIntr,regValue);
	#endif
}

static void _initTouchIntr(void)
{
	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
    printf("TP init in\n");
    #endif

    ithEnterCritical();

    ithGpioClearIntr(TP_GPIO_PIN);
    ithGpioRegisterIntrHandler(TP_GPIO_PIN, _tp_isr, NULL);
    ithGpioCtrlDisable(TP_GPIO_PIN, ITH_GPIO_INTR_LEVELTRIGGER);	//set as edge trigger
    ithGpioCtrlDisable(TP_GPIO_PIN, ITH_GPIO_INTR_BOTHEDGE);		//set as single edge
    ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_INTR_TRIGGERFALLING);	//set as falling edge
    ithIntrEnableIrq(ITH_INTR_GPIO);
    ithGpioEnableIntr(TP_GPIO_PIN);

    ithExitCritical();

    #ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
    printf("TP init out\n");
    #endif
}
#endif

//add by cvte_hcy{
void PullDownReserveGpio(void)
{
    ithGpioSetMode(ITH_GPIO_PIN8, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN8);
    ithGpioSetMode(ITH_GPIO_PIN12, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN12);
    ithGpioSetMode(ITH_GPIO_PIN13, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN13);
    ithGpioSetMode(ITH_GPIO_PIN15, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN15);
    ithGpioSetMode(ITH_GPIO_PIN16, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN16);
    ithGpioSetMode(ITH_GPIO_PIN17, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN17);
    ithGpioSetMode(ITH_GPIO_PIN21, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN21);
    ithGpioSetMode(ITH_GPIO_PIN24, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN24);
    ithGpioSetMode(ITH_GPIO_PIN30, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN30);
    ithGpioSetMode(ITH_GPIO_PIN31, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN31);
    ithGpioSetMode(ITH_GPIO_PIN32, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN32);
    ithGpioSetMode(ITH_GPIO_PIN33, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN33);
    ithGpioSetMode(ITH_GPIO_PIN34, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN34);
    ithGpioSetMode(ITH_GPIO_PIN35, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN35);
    ithGpioSetMode(ITH_GPIO_PIN36, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN36);
    ithGpioSetMode(ITH_GPIO_PIN74, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN74);
    ithGpioSetMode(ITH_GPIO_PIN75, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN75);
    ithGpioSetMode(ITH_GPIO_PIN76, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN76);
    ithGpioSetMode(ITH_GPIO_PIN77, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN77);
    ithGpioSetMode(ITH_GPIO_PIN79, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN79);
    ithGpioSetMode(ITH_GPIO_PIN80, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN80);
    ithGpioSetMode(ITH_GPIO_PIN81, ITH_GPIO_MODE0);
    ithGpioSetOut(ITH_GPIO_PIN81);

    ithGpioClear(ITH_GPIO_PIN8);
    ithGpioClear(ITH_GPIO_PIN12);
    ithGpioClear(ITH_GPIO_PIN13);
    ithGpioClear(ITH_GPIO_PIN15);
    ithGpioClear(ITH_GPIO_PIN16);
    ithGpioClear(ITH_GPIO_PIN17);
    ithGpioClear(ITH_GPIO_PIN21);
    ithGpioClear(ITH_GPIO_PIN24);
    ithGpioClear(ITH_GPIO_PIN30);
    ithGpioClear(ITH_GPIO_PIN31);
    ithGpioClear(ITH_GPIO_PIN32);
    ithGpioClear(ITH_GPIO_PIN33);
    ithGpioClear(ITH_GPIO_PIN34);
    ithGpioClear(ITH_GPIO_PIN35);
    ithGpioClear(ITH_GPIO_PIN36);
    ithGpioClear(ITH_GPIO_PIN74);
    ithGpioClear(ITH_GPIO_PIN75);
    ithGpioClear(ITH_GPIO_PIN76);
    ithGpioClear(ITH_GPIO_PIN77);
    ithGpioClear(ITH_GPIO_PIN79);
    ithGpioClear(ITH_GPIO_PIN80);
    ithGpioClear(ITH_GPIO_PIN81);
}
//add by cvte_hcy}

void _initTouchGpioPin(void)
{
	ithGpioSetMode(TP_GPIO_PIN, ITH_GPIO_MODE0);
	ithGpioSetIn(TP_GPIO_PIN);
	ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_PULL_ENABLE);
	ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_PULL_UP);
	ithGpioEnable(TP_GPIO_PIN);

	//add by cvte_zxl {
	ithGpioSet(TP_RST_GPIO_PIN);
	ithGpioSetMode(TP_RST_GPIO_PIN, ITH_GPIO_MODE0);
	ithGpioSetOut(TP_RST_GPIO_PIN);
	//add by cvte_zxl }

	ithGpioSet(TP_RST_GPIO_PIN);
	usleep(50 * 1000);
	ithGpioClear(TP_RST_GPIO_PIN);
	usleep(50 * 1000);
	ithGpioSet(TP_RST_GPIO_PIN);
	usleep(200 * 1000);// 目前，上电，设置引脚而已。

	printf("_initTouchGpioPin\n");
}

/******************************************************************************
 * the read flow for reading the zt2083's register by using iic repead start
 ******************************************************************************/
static int _readChipReg_test(int fd, unsigned char addr, unsigned char regAddr, unsigned char *dBuf, unsigned char dLen)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;

#ifdef	ENABLE_TOUCH_IIC_DBG_MSG
	printf("	RdTchIcReg(fd=%x, reg=%x, buf=%x, len=%x)\n", fd, regAddr, dBuf, dLen);
#endif

#ifdef EN_DISABLE_ALL_INTR
	portSAVEDISABLE_INTERRUPTS();
#endif

	I2cCmd = regAddr;	//1000 0010
	evt.slaveAddress = addr;
	evt.cmdBuffer = &I2cCmd;
	evt.cmdBufferSize = 1;
	evt.dataBuffer = dBuf;
	evt.dataBufferSize = dLen;

	i2cret = read(fd, &evt, 1);

#ifdef EN_DISABLE_ALL_INTR
	portRESTORE_INTERRUPTS();
#endif

	if (i2cret<0)
	{
		printf("[TOUCH ERROR].iic read fail\n");
		return -1;
	}

	return 0;
}

static int _readChipReg(int fd, unsigned char regAddr, unsigned char *dBuf, unsigned char dLen)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;

	#ifdef	ENABLE_TOUCH_IIC_DBG_MSG
	printf("	RdTchIcReg(fd=%x, reg=%x, buf=%x, len=%x)\n", fd, regAddr, dBuf, dLen);
	#endif

	#ifdef EN_DISABLE_ALL_INTR
	portSAVEDISABLE_INTERRUPTS();
	#endif

	I2cCmd = regAddr;	//1000 0010
	evt.slaveAddress   = TOUCH_DEVICE_ID;
	evt.cmdBuffer      = &I2cCmd;
	evt.cmdBufferSize  = 1;
	evt.dataBuffer     = dBuf;
	evt.dataBufferSize = dLen;

	i2cret = read(fd, &evt, 1);

	#ifdef EN_DISABLE_ALL_INTR
    portRESTORE_INTERRUPTS();
	#endif

	if(i2cret<0)
	{
		printf("[TOUCH ERROR].iic read fail\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * the write flow for writing the zt2083's register by using iic repead start
 ******************************************************************************/
static int _writeChipReg(int fd, unsigned char regAddr)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;

	#ifdef	ENABLE_TOUCH_IIC_DBG_MSG
	printf("	WtTchIcReg(fd=%x, reg=%x)\n", fd, regAddr);
	#endif

	#ifdef EN_DISABLE_ALL_INTR
	portSAVEDISABLE_INTERRUPTS();
	#endif

	I2cCmd = regAddr;	//1000 0010
	evt.slaveAddress   = TOUCH_DEVICE_ID;
	evt.cmdBuffer      = &I2cCmd;
	evt.cmdBufferSize  = 1;
	evt.dataBuffer     = 0;
	evt.dataBufferSize = 0;
	i2cret = write(fd, &evt, 1);

	#ifdef EN_DISABLE_ALL_INTR
    portRESTORE_INTERRUPTS();
	#endif

	if(i2cret<0)
	{
		printf("[TOUCH ERROR].iic read fail\n");
		return -1;
	}

	return 0;
}

/******************************************************************************
 * to read the Point Buffer by reading the register "0xE0"
 ******************************************************************************/

static int _readPointBuffer(int fd, unsigned char *Pbuf)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;
	int ret = 0, i = 0, j = 0, x = 0, y = 0, tmp = 0, tp_status = 0, packet = 0; //add by cvte_zxl

	#ifdef	ENABLE_TOUCH_IIC_DBG_MSG
	printf("	RdPntbuf,%x,%x\n",fd,Pbuf);
	#endif

	while(1)
	{
		//add by cvte_zxl
		i2cret = _readChipReg(fd, 0x00, Pbuf, 33); //ILITEK_TP_CMD_READ_DATA == 0x00  //这里仅读出数据，不解析
		if(i2cret<0)
		{
			printf("[TOUCH ERROR].Read Point buffer fail\n");
			return -1;
		}
		//packet = Pbuf[0];
		//printf("cvte# ilitek packet = %d\n", packet);	//ilitek packet = 1
        //add by cvte_zxl }
		break;
	}

	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	{
		int i;
		printf("\nRdPbuf:");
		for(i=0; i<8; i++)	printf("%02x ",Pbuf[i]);
		printf("\n\n");
	}
	#endif

	return 0;
}

static void _initSample(struct ts_sample *s, int nr)
{
	int i;
	struct ts_sample *samp=s;

	//samp=s;

	for(i = 0; i < nr; i++)
	{
		samp->x = 0;
		samp->y = 0;
		samp->pressure = 0;
		gettimeofday(&(samp->tv),NULL);
		samp++;
	}
}

static void _ilitek_getPoint(struct ts_sample *samp, int nr)
{
	int i2cret;
	int real_nr=0;
	int tmpX=0;
	int tmpY=0;
    int reg_index=0;
	struct ts_sample *s=samp;
	unsigned char finger_status_reg[33]={0};
	int touch_status=0;

	_initSample(s, nr);

    i2cret = _readPointBuffer(g_tchDevFd,finger_status_reg);
        if(i2cret<0)
            return;


	while(real_nr<nr)
	{

        reg_index = 3 + real_nr * 6;

        //id = finger_status_reg[5]>>4;
		touch_status = finger_status_reg[reg_index]>>6;

        if(touch_status==0||touch_status==2)
            touch_status = 1;
        else
            touch_status = 0;


        if((!touch_status) && (g_TouchStatus==TOUCH_DOWN))
        {
            samp->x = (int)0;
            samp->y = (int)0;
            samp->pressure = 0;
        }
        else
        {
            samp->x = (unsigned short)((finger_status_reg[reg_index]&0x0f)<<8) | (unsigned short)(finger_status_reg[reg_index+1]);			//仅处理一点
            samp->y = (unsigned short)((finger_status_reg[reg_index+2]&0x0f)<<8) | (unsigned short)(finger_status_reg[reg_index+3]);
            samp->pressure = 1;			//?? how to get ft5306's presure?

            #ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
            printf("[TP INFO]raw xy=(%d,%d)\n",samp->x,samp->y);
            #endif

            //printf("[TP INFO]raw xy=(%d,%d)\n",samp->x,samp->y);
        }

        /* 2017/6/6, modified by cvte_lzw, @{*/        
		//if( samp->x > 480 || samp->y > 800)
        if (samp->x > CFG_TOUCH_X_MAX_VALUE || samp->y > CFG_TOUCH_Y_MAX_VALUE)
        /* @}*/
		{
			isLargePitch = true;
		}
        #ifdef	ENABLE_SWITCH_XY
        {
            int	tmp = samp->x;
            samp->x = samp->y;
            samp->y = tmp;
        }
        #endif

        #ifdef	ENABLE_REVERSE_X
        if(samp->pressure)
        {
            short tempX = (short)samp->x;
            if(g_xResolution>=tempX)	samp->x = (int)(g_xResolution - tempX);
            else						samp->x = 0;
        }
        #endif

        #ifdef	ENABLE_REVERSE_Y
        if(samp->pressure)
        {
            short tempY = (short)samp->y;
            if(g_yResolution>=tempY)	samp->y = (int)(g_yResolution - tempY);
            else						samp->y = 0;
        }
        #endif

        #ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
        printf("	RAW->--------------------> %d %d %d\n", samp->pressure, samp->x, samp->y);
        #endif
        gettimeofday(&samp->tv,NULL);
		s++;
		real_nr++;
	}
}

//static int zt2083_read(struct tslib_module_info *inf, struct ts_sample *samp, int nr)
static void* _tpProbeHandler(void* arg)
{
    unsigned int regValue;
    unsigned char NeedToGetSample = 1;	//for sending a I2C command when each INTr happens.
    unsigned int TchIntrCnt=0;			//for detecting the touch-UP event
    printf("_tpProbeHandler.start~t~\n");

	while(1)
	{
		if(g_IsTpInitialized==true)
		{
			#ifdef CFG_TOUCH_INTR
			if ( g_TouchDownIntr )
			#else
			regValue = ithGpioGet(TP_GPIO_PIN);
			if ( !(regValue & TP_GPIO_MASK) )
			#endif
			{
				struct ts_sample tmpSamp;
				unsigned int NeedUpdateSample = 0;

				TchIntrCnt=0;

				#ifdef CFG_TOUCH_INTR
				ithGpioDisableIntr(TP_GPIO_PIN);
				#endif

				gettimeofday(&endT,NULL);
				dur = (unsigned int)itpTimevalDiff(&startT, &endT);

				#ifndef CFG_TOUCH_INTR
				if( !NeedToGetSample )	continue;	//to prevent over sampling
				NeedToGetSample = 0;
				#endif

				_ilitek_getPoint(&tmpSamp, 1);

				if(isLargePitch)
				{
//					printf("isLargePitch == true!!!!!!\n");
					isLargePitch = false;
					continue;
				}
				pthread_mutex_lock(&gTpMutex);

				#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
				printf("	lastp=%x, dur=%x, p=%x, s=%x\n",lastp, dur, tmpSamp.pressure, g_TouchStatus);
				#endif

				switch(g_TouchStatus)
				{
					case TOUCH_NO_CONTACT:
						if(tmpSamp.pressure)
						{
							g_TouchStatus = TOUCH_DOWN;
							g_currIntr = 1;
							NeedUpdateSample = 1;
							g_1stSampHasSend = 0;
						}
						break;

					case TOUCH_DOWN:
						if(!tmpSamp.pressure)
						{
							g_TouchStatus = TOUCH_UP;
						}
						if(g_1stSampHasSend)	NeedUpdateSample = 1;
						break;

					case TOUCH_UP:
						if(!tmpSamp.pressure)
						{
							g_TouchStatus = TOUCH_NO_CONTACT;
							g_currIntr = 0;
						}
						else
						{
							g_TouchStatus = TOUCH_DOWN;
							g_currIntr = 1;
							NeedUpdateSample = 1;
						}
						break;

					default:
						printf("ERROR touch STATUS, need to check it!!\n");
						break;
				}

				#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
				printf("	tpStatus=%x, NSQ=%x, cINT=%x, send=%x\n", g_TouchStatus, NeedUpdateSample, g_currIntr, g_1stSampHasSend);
				#endif

				pthread_mutex_unlock(&gTpMutex);

				if(NeedUpdateSample)
				{
					gettimeofday(&startT,NULL);

					pthread_mutex_lock(&gTpMutex);

					lastp = tmpSamp.pressure;
					lastx = tmpSamp.x;
					lasty = tmpSamp.y;

					#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
					printf("	EnQue:p=%x,xy=%d,%d\n", tmpSamp.pressure, tmpSamp.x, tmpSamp.y);
					#endif

					pthread_mutex_unlock(&gTpMutex);
				}

				#ifdef CFG_TOUCH_INTR
				g_TouchDownIntr = 0;
				ithGpioEnableIntr(TP_GPIO_PIN);
				#endif

				#ifndef CFG_TOUCH_INTR
				usleep(2000);
				#endif
			}
			else	//if ( g_TouchDownIntr )
			{
				#ifndef CFG_TOUCH_INTR
				NeedToGetSample = 1;
				#endif

				if(g_TouchStatus == TOUCH_UP)
				{
					if(g_1stSampHasSend)
					{
						pthread_mutex_lock(&gTpMutex);

						g_TouchStatus = TOUCH_NO_CONTACT;
						g_currIntr = 0;
						lastp = 0;
						lastx = 0;
						lasty = 0;

						pthread_mutex_unlock(&gTpMutex);
					}
					#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
					printf("INT=0, force to set status=0!!\n");
					#endif
				}

				if( (g_TouchStatus == TOUCH_DOWN) && ithGpioGet(TP_GPIO_PIN) )
				{
					if(!TchIntrCnt)
					{
						gettimeofday(&T1,NULL);
					}

					gettimeofday(&T2,NULL);
					dur = (unsigned int)itpTimevalDiff(&T1, &T2);

					if( dur>TOUCH_SAMPLE_RATE_200ms && g_1stSampHasSend )
					{
						pthread_mutex_lock(&gTpMutex);

						g_TouchStatus = TOUCH_UP;
						g_currIntr = 0;
						lastp = 0;
						lastx = 0;
						lasty = 0;

						#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
						printf("INT=0, and dur>%dms, so force to set status=2!!\n",TOUCH_SAMPLE_RATE_16ms);
						#endif

						pthread_mutex_unlock(&gTpMutex);

						gettimeofday(&startT,NULL);


					}

					TchIntrCnt++;
				}
				#ifndef CFG_TOUCH_INTR
				usleep(2000);
				#endif
			}
		}
		else	//if(g_IsTpInitialized==true)
		{
			printf("WARNING:: TP has not initial, yet~~~\n");
			usleep(100000);
		}
	}
	return NULL;
}

/******************************************************************************
 * do initial flow of zt2083
 * 1.indentify zt2083
 * 2.get 2D resolution
 * 3.set interrupt information
 ******************************************************************************/
static int _initTouchChip(int fd)
{
	int i=0;

	//identify FW
	//TODO:

	//create thread
	if(g_IsTpInitialized==0)
	{
	    int res;
	    pthread_t task;
	    pthread_attr_t attr;
	    printf("Create xQueue & pthread~~\n");
	    #ifdef __OPENRTOS__
		tpQueue = xQueueCreate(QUEUE_LEN, (unsigned portBASE_TYPE) sizeof(struct ts_sample));

	    pthread_attr_init(&attr);
	    res = pthread_create(&task, &attr, _tpProbeHandler, NULL);
	    if(res)
	    {
	    	printf( "[TouchPanel]%s() L#%ld: ERROR, create _tpProbeHandler() thread fail! res=%ld\n", res );
	    	return -1;
	    }
	    res = pthread_mutex_init(&gTpMutex, NULL);
    	if(res)
    	{
    	    printf("[AudioLink]%s() L#%ld: ERROR, init touch mutex fail! res=%ld\r\n", __FUNCTION__, __LINE__, res);
    	    return -1;
    	}
		#endif
    	return 0;
    }

	return -1;
}

static int _getTouchSample(struct ts_sample *samp, int nr)
{
	int ret=0;
	struct ts_sample *s=samp;

	pthread_mutex_lock(&gTpMutex);

	if(g_currIntr)
	{
		s->pressure = lastp;
		s->x = lastx;
		s->y = lasty;
		gettimeofday(&s->tv,NULL);
		if(s->pressure)	g_1stSampHasSend = 1;

		ret++;
	}

	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	printf("gfQ, r=%x, INT=%x, s=%x, pxy=(%d,%d,%d)\n", ret, g_currIntr, g_TouchStatus, s->pressure, s->x, s->y);
	#endif

	pthread_mutex_unlock(&gTpMutex);

	return ret;
}

static void showAhbReg(unsigned int RegBase, unsigned int len)
{
	unsigned int i;
	printf("RegBase=%x, Len=%x\n",RegBase,len);
	for(i=RegBase; i<RegBase+len; i+=4)
	{
		printf("%08x ",ithReadRegA(i));
		if( (i&0x0C)==0x0C )	printf("\n");
	}
	printf("\n");
}

#ifdef	ENABLE_SEND_FAKE_SAMPLE
static int _getFakeSample(struct ts_sample *samp, int nr)
{
	_initSample(samp, nr);

	printf("tp_getXY::cnt=%x\n",g_tpCntr);

	if(g_tpCntr++>0x100)
	{
		if( !(g_tpCntr&0x07) )
		{
			unsigned int i;
			i = (g_tpCntr>>3)&0x1F;
			if(i<MAX_FAKE_NUM)
			{
				samp->pressure = 1;
				samp->x = gFakeTableX[i];
				samp->y = gFakeTableY[i];
				printf("sendXY.=%d,%d\n",samp->x,samp->y);
			}
		}
	}

	return nr;
}
#endif
/*##################################################################################
 *                           private function above
 ###################################################################################*/










/*##################################################################################
 #                       public function implementation
 ###################################################################################*/
static int cvte_zero_count = 0;
const int tpLen = 4;
unsigned char TPVerBuf[10] = { 0 };

static int zt2083_read(struct tslib_module_info *inf, struct ts_sample *samp, int nr)
{
	struct tsdev *ts = inf->dev;
	unsigned int regValue;
	int ret;
	int total = 0;
	int tchdev = ts->fd;
	struct ts_sample *s=samp;

	#ifdef	ENABLE_SEND_FAKE_SAMPLE
	return _getFakeSample(samp,nr);
	#endif

	if(g_IsTpInitialized==false)
	{
		//_touchInit();

		printf("TP first init(INT is GPIO %d)\n",TP_GPIO_PIN);
		//showAhbReg(0xDE000000,0x100);

		gettimeofday(&startT,NULL);

		//init touch GPIO pin
		_initTouchGpioPin();

		#ifdef CFG_TOUCH_INTR
		_initTouchIntr();
		#endif

        //170914 add cvte_hcy{
//        #ifdef BOARD_1954_C
//        PullDownReserveGpio();
//        #endif
       //170914 add cvte_hcy}

		ret = _initTouchChip(tchdev);
		if(ret<0)
		{
			printf("[TOUCH]warning:: touch panel initial fail\n");
			return -1;
		}
		usleep(100000);		//sleep 100ms for get the 1st touch event

		g_tchDevFd = tchdev;
		g_IsTpInitialized = true;
		printf("## TP init OK, gTpIntr = %x\n", g_TouchDownIntr);
		//add by cvte_zxl {
		{
			unsigned char buf[10] = { 0 };

			_readChipReg(g_tchDevFd, 0x61, &buf, 5); //ILITEK_TP_CMD_GET_KERNEL_VERSION == 0x61
			printf("cvte# TP KERNEL_VERSION: %d.%d.%d.%d.%d\n", buf[0], buf[1], buf[2], buf[3], buf[4]); //TP KERNEL_VERSION:23.33.10.0.1

			_readChipReg(g_tchDevFd, 0xa6, &TPVerBuf, tpLen); //ILITEK_TP_CMD_GET_FIRMWARE_VERSION == 0x40
			printf("cvte# TP FIRWARE_VERSION: %d.%d.%d.%d\n", TPVerBuf[0], TPVerBuf[1], TPVerBuf[2], TPVerBuf[3]);           //TP FIRWARE_VERSION: 0.0.2.0

			_readChipReg(g_tchDevFd, 0x42, &buf, 2); //ILITEK_TP_CMD_GET_PROTOCOL_VERSION == 0x42
			printf("cvte# TP PROTOCOL_VERSION: %d.%d\n", buf[0], buf[1]);                                //TP PROTOCOL_VERSION: 3.0
			cvte_ilitek_tp.protocol_ver = (((int)buf[0]) << 8) + buf[1];

			_readChipReg(g_tchDevFd, 0x20, &buf, 10); //ILITEK_TP_CMD_GET_RESOLUTION == 0x20
			cvte_ilitek_tp.max_x	= buf[0];
			cvte_ilitek_tp.max_x   += ((int)buf[1]) * 256;
			cvte_ilitek_tp.max_y	= buf[2];
			cvte_ilitek_tp.max_y   += ((int)buf[3]) * 256;
			cvte_ilitek_tp.x_ch		= buf[4];
			cvte_ilitek_tp.y_ch		= buf[5];
			cvte_ilitek_tp.max_tp	= buf[6];
			cvte_ilitek_tp.max_btn	= buf[7];
			cvte_ilitek_tp.keycount = buf[8];

			printf("cvte# TP RESOLUTION: max_x: %d, max_y: %d, ch_x: %d, ch_y: %d\n",					//TP RESOLUTION: max_x: 480, max_y: 272, ch_x: 11, ch_y: 19
										cvte_ilitek_tp.max_x,
										cvte_ilitek_tp.max_y,
										cvte_ilitek_tp.x_ch,
										cvte_ilitek_tp.y_ch);
			printf("cvte# TP KEY: max_tp: %d, max_btn: %d, key_count: %d\n",							//TP KEY : max_tp : 5, max_btn : 0, key_count : 0
										cvte_ilitek_tp.max_tp,
										cvte_ilitek_tp.max_btn,
										cvte_ilitek_tp.keycount);

		}
		//add by cvte_zxl }
	}
	_initSample(s, nr);

	//to receive touch sample
	ret = _getTouchSample(samp, nr);

	#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	if(ret)	printf("	deQue-O:%x=(%d,%d,%d)r=%d\n", samp, samp->pressure, samp->x, samp->y, ret);
	#endif

	return nr;
}
//api--getIPFirmware
unsigned char* CvteGetTPFirmware()
{

	printf("TP VERSION: %d.%d.%d.%d\n", TPVerBuf[0], TPVerBuf[1], TPVerBuf[2], TPVerBuf[3]);

	return TPVerBuf;
}
unsigned int TPVerBufSize()
{
	int count = 0;
	int i = 0;
	for (i = 0; i < sizeof(TPVerBuf); i++){
		if (TPVerBuf[i]!=0){
			count++;
		}
	}
	//int tmpTPVer = sizeof(TPVerBuf);
		//printf("TPBufLen: %d\n", tmpTPVer);
	return count;
}
static const struct tslib_ops zt2083_ops =
{
	zt2083_read,
};

TSAPI struct tslib_module_info *cvte_ilitek_new_mod_init(struct tsdev *dev, const char *params)
{
	struct tslib_module_info *m;

	m = malloc(sizeof(struct tslib_module_info));
	if (m == NULL)
		return NULL;

	m->ops = &zt2083_ops;
	return m;
}

#ifndef TSLIB_STATIC_CASTOR3_MODULE
	TSLIB_MODULE_INIT(castor3_mod_init);
#endif

