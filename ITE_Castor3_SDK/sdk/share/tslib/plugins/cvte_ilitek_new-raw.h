
/*
* include/linux/hy461x_touch.h
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#ifndef __LINUX_HY461X_TS_H__
#define __LINUX_HY461X_TS_H__

#define HY_461X_DR_VER 	0X06		/* Driver sample code Version 201560427 */
//#define HY_UPDATE_FW_ONLINE			/* Updata FW Enadle */
//#define HY_CHECK_BUF_UP				/* check buffer and 0xff */
#define HY461X_NAME				"hy461x_ts"
#define HY461X_PT_MAX			5
#define HY_MAX_ID	0x0a
#define HY_TOUCH_STEP	6
#define HY_TOUCH_STEP	6
#define HY_TOUCH_X_H_POS		3
#define HY_TOUCH_X_L_POS		4
#define HY_TOUCH_Y_H_POS		5
#define HY_TOUCH_Y_L_POS		6
#define HY_TOUCH_EVENT_POS		3
#define HY_TOUCH_ID_POS			5
#define HY_PRESS			0x7F

#define POINT_READ_BUF	(3 + HY_TOUCH_STEP * HY461X_PT_MAX)

//---------------------------------------------------------

enum hy561x_ts_regs {
	HY461X_REG_THGROUP = 0x80,
	//	HY461X_REG_THPEAK			= 0x81,

	//	HY461X_REG_TIMEENTERMONITOR	= 0x87,
	//	HY461X_REG_PERIODACTIVE		= 0x88,
	//	HY461X_REG_PERIODMONITOR	= 0x89,

	//	HY461X_REG_AUTO_CLB_MODE	= 0xa0,

	HY461X_REG_PMODE = 0xa5,			/* Power Consume Mode */
	HY461X_REG_FIRMID = 0xa6,
	HY461X_REG_MODE = 0x00,
	HY461X_REG_BASE = 0xae,

	//	HY461X_REG_ERR				= 0xa9,
	//	HY461X_REG_CLB				= 0xaa,
};

// HY461X_REG_PMODE
#define PMODE_ACTIVE			0x00
#define PMODE_MONITOR			0x01
#define PMODE_STANDBY			0x02
#define PMODE_HIBERNATE			0x03

#define HYS_POINT_UP			0x01
#define HYS_POINT_DOWN			0x00
#define HYS_POINT_CONTACT		0x02

typedef enum
{
	ERR_OK,
	ERR_MODE,
	ERR_READID,
	ERR_ERASE,
	ERR_STATUS,
	ERR_ECC,
	ERR_DL_ERASE_FAIL,
	ERR_DL_PROGRAM_FAIL,
	ERR_DL_VERIFY_FAIL
}E_UPGRADE_ERR_TYPE;

#endif	// __LINUX_HY461X_TS_H__