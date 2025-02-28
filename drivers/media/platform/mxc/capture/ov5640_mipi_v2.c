/*
 * Copyright (C) 2011-2016 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/v4l2-mediabus.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

#define OV5640_VOLTAGE_ANALOG               2800000
#define OV5640_VOLTAGE_DIGITAL_CORE         1500000
#define OV5640_VOLTAGE_DIGITAL_IO           1800000

#define MIN_FPS 15
#define MAX_FPS 30
#define DEFAULT_FPS 30

#define OV5640_XCLK_MIN 6000000
#define OV5640_XCLK_MAX 24000000
#define OV5640_XCLK_20MHZ 20000000

#define OV5640_CHIP_ID_HIGH_BYTE	0x300A
#define OV5640_CHIP_ID_LOW_BYTE		0x300B

enum ov5640_mode {
	ov5640_mode_MIN = 0,
	ov5640_mode_VGA_640_480 = 0,
	ov5640_mode_NTSC_720_480 = 1,
	ov5640_mode_720P_1280_720 = 2,
	ov5640_mode_1080P_1920_1080 = 3,
	ov5640_mode_QSXGA_2592_1944 = 4,
	ov5640_mode_MAX = 5,
	ov5640_mode_INIT = 0xff, /*only for sensor init*/
};

enum ov5640_frame_rate {
	ov5640_15_fps,
	ov5640_30_fps
};

static const int ov5640_framerates[] = {
	[ov5640_15_fps] = 15,
	[ov5640_30_fps] = 30,
};

struct ov5640_datafmt {
	u32	code;
	enum v4l2_colorspace		colorspace;
};

/* image size under 1280 * 960 are SUBSAMPLING
 * image size upper 1280 * 960 are SCALING
 */
enum ov5640_downsize_mode {
	SUBSAMPLING,
	SCALING,
};

struct reg_value {
	u16 u16RegAddr;
	u8 u8Val;
	u8 u8Mask;
	u32 u32Delay_ms;
};

struct ov5640_mode_info {
	enum ov5640_mode mode;
	enum ov5640_downsize_mode dn_mode;
	u32 width;
	u32 height;
	const struct reg_value *init_data_ptr;
	u32 init_data_size;
};

struct ov5640 {
	struct v4l2_subdev		subdev;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	const struct ov5640_datafmt	*fmt;
	struct v4l2_captureparm streamcap;
	bool on;

	/* control settings */
	int brightness;
	int hue;
	int contrast;
	int saturation;
	int red;
	int green;
	int blue;
	int ae_mode;

	u32 mclk;
	struct clk *sensor_clk;
	int csi;

	int pwn_gpio;
	int rst_gpio;
	int prev_sysclk;
	int prev_HTS;
	int AE_low;
	int AE_high;
	int AE_Target;
	struct regulator *io_regulator;
	struct regulator *core_regulator;
	struct regulator *analog_regulator;
	struct regulator *gpo_regulator;
};

struct ov5640_res {
	int width;
	int height;
};

/*!
 * Maintains the information on the current state of the sesor.
 */

struct ov5640_res ov5640_valid_res[] = {
	[0] = {640, 480},
	[1] = {720, 480},
	[2] = {1280, 720},
	[3] = {1920, 1080},
	[4] = {2592, 1944},
};

static const struct reg_value ov5640_init_setting_30fps_VGA[] = {

	{0x3103, 0x11, 0, 0}, {0x3008, 0x82, 0, 5}, {0x3008, 0x42, 0, 0},
	{0x3103, 0x03, 0, 0}, {0x3017, 0x00, 0, 0}, {0x3018, 0x00, 0, 0},
	{0x3034, 0x18, 0, 0}, {0x3035, 0x14, 0, 0}, {0x3036, 0x38, 0, 0},
	{0x3037, 0x13, 0, 0}, {0x3108, 0x01, 0, 0}, {0x3630, 0x36, 0, 0},
	{0x3631, 0x0e, 0, 0}, {0x3632, 0xe2, 0, 0}, {0x3633, 0x12, 0, 0},
	{0x3621, 0xe0, 0, 0}, {0x3704, 0xa0, 0, 0}, {0x3703, 0x5a, 0, 0},
	{0x3715, 0x78, 0, 0}, {0x3717, 0x01, 0, 0}, {0x370b, 0x60, 0, 0},
	{0x3705, 0x1a, 0, 0}, {0x3905, 0x02, 0, 0}, {0x3906, 0x10, 0, 0},
	{0x3901, 0x0a, 0, 0}, {0x3731, 0x12, 0, 0}, {0x3600, 0x08, 0, 0},
	{0x3601, 0x33, 0, 0}, {0x302d, 0x60, 0, 0}, {0x3620, 0x52, 0, 0},
	{0x371b, 0x20, 0, 0}, {0x471c, 0x50, 0, 0}, {0x3a13, 0x43, 0, 0},
	{0x3a18, 0x00, 0, 0}, {0x3a19, 0xf8, 0, 0}, {0x3635, 0x13, 0, 0},
	{0x3636, 0x03, 0, 0}, {0x3634, 0x40, 0, 0}, {0x3622, 0x01, 0, 0},
	{0x3c01, 0xa4, 0, 0}, {0x3c04, 0x28, 0, 0}, {0x3c05, 0x98, 0, 0},
	{0x3c06, 0x00, 0, 0}, {0x3c07, 0x08, 0, 0}, {0x3c08, 0x00, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9b, 0, 0},
	{0x3808, 0x02, 0, 0}, {0x3809, 0x80, 0, 0}, {0x380a, 0x01, 0, 0},
	{0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x68, 0, 0},
	{0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x06, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x27, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x02, 0, 0}, {0x3000, 0x00, 0, 0},
	{0x3002, 0x1c, 0, 0}, {0x3004, 0xff, 0, 0}, {0x3006, 0xc3, 0, 0},
	{0x300e, 0x45, 0, 0}, {0x302e, 0x08, 0, 0}, {0x4300, 0x32, 0, 0},
	{0x501f, 0x00, 0, 0}, {0x4713, 0x03, 0, 0}, {0x4407, 0x04, 0, 0},
	{0x440e, 0x00, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x4837, 0x0a, 0, 0}, {0x4800, 0x04, 0, 0}, {0x3824, 0x02, 0, 0},
	{0x5000, 0xa7, 0, 0}, {0x5001, 0xa3, 0, 0}, {0x5180, 0xff, 0, 0},
	{0x5181, 0xf2, 0, 0}, {0x5182, 0x00, 0, 0}, {0x5183, 0x14, 0, 0},
	{0x5184, 0x25, 0, 0}, {0x5185, 0x24, 0, 0}, {0x5186, 0x09, 0, 0},
	{0x5187, 0x09, 0, 0}, {0x5188, 0x09, 0, 0}, {0x5189, 0x88, 0, 0},
	{0x518a, 0x54, 0, 0}, {0x518b, 0xee, 0, 0}, {0x518c, 0xb2, 0, 0},
	{0x518d, 0x50, 0, 0}, {0x518e, 0x34, 0, 0}, {0x518f, 0x6b, 0, 0},
	{0x5190, 0x46, 0, 0}, {0x5191, 0xf8, 0, 0}, {0x5192, 0x04, 0, 0},
	{0x5193, 0x70, 0, 0}, {0x5194, 0xf0, 0, 0}, {0x5195, 0xf0, 0, 0},
	{0x5196, 0x03, 0, 0}, {0x5197, 0x01, 0, 0}, {0x5198, 0x04, 0, 0},
	{0x5199, 0x6c, 0, 0}, {0x519a, 0x04, 0, 0}, {0x519b, 0x00, 0, 0},
	{0x519c, 0x09, 0, 0}, {0x519d, 0x2b, 0, 0}, {0x519e, 0x38, 0, 0},
	{0x5381, 0x1e, 0, 0}, {0x5382, 0x5b, 0, 0}, {0x5383, 0x08, 0, 0},
	{0x5384, 0x0a, 0, 0}, {0x5385, 0x7e, 0, 0}, {0x5386, 0x88, 0, 0},
	{0x5387, 0x7c, 0, 0}, {0x5388, 0x6c, 0, 0}, {0x5389, 0x10, 0, 0},
	{0x538a, 0x01, 0, 0}, {0x538b, 0x98, 0, 0}, {0x5300, 0x08, 0, 0},
	{0x5301, 0x30, 0, 0}, {0x5302, 0x10, 0, 0}, {0x5303, 0x00, 0, 0},
	{0x5304, 0x08, 0, 0}, {0x5305, 0x30, 0, 0}, {0x5306, 0x08, 0, 0},
	{0x5307, 0x16, 0, 0}, {0x5309, 0x08, 0, 0}, {0x530a, 0x30, 0, 0},
	{0x530b, 0x04, 0, 0}, {0x530c, 0x06, 0, 0}, {0x5480, 0x01, 0, 0},
	{0x5481, 0x08, 0, 0}, {0x5482, 0x14, 0, 0}, {0x5483, 0x28, 0, 0},
	{0x5484, 0x51, 0, 0}, {0x5485, 0x65, 0, 0}, {0x5486, 0x71, 0, 0},
	{0x5487, 0x7d, 0, 0}, {0x5488, 0x87, 0, 0}, {0x5489, 0x91, 0, 0},
	{0x548a, 0x9a, 0, 0}, {0x548b, 0xaa, 0, 0}, {0x548c, 0xb8, 0, 0},
	{0x548d, 0xcd, 0, 0}, {0x548e, 0xdd, 0, 0}, {0x548f, 0xea, 0, 0},
	{0x5490, 0x1d, 0, 0}, {0x5580, 0x02, 0, 0}, {0x5583, 0x40, 0, 0},
	{0x5584, 0x10, 0, 0}, {0x5589, 0x10, 0, 0}, {0x558a, 0x00, 0, 0},
	{0x558b, 0xf8, 0, 0}, {0x5800, 0x23, 0, 0}, {0x5801, 0x14, 0, 0},
	{0x5802, 0x0f, 0, 0}, {0x5803, 0x0f, 0, 0}, {0x5804, 0x12, 0, 0},
	{0x5805, 0x26, 0, 0}, {0x5806, 0x0c, 0, 0}, {0x5807, 0x08, 0, 0},
	{0x5808, 0x05, 0, 0}, {0x5809, 0x05, 0, 0}, {0x580a, 0x08, 0, 0},
	{0x580b, 0x0d, 0, 0}, {0x580c, 0x08, 0, 0}, {0x580d, 0x03, 0, 0},
	{0x580e, 0x00, 0, 0}, {0x580f, 0x00, 0, 0}, {0x5810, 0x03, 0, 0},
	{0x5811, 0x09, 0, 0}, {0x5812, 0x07, 0, 0}, {0x5813, 0x03, 0, 0},
	{0x5814, 0x00, 0, 0}, {0x5815, 0x01, 0, 0}, {0x5816, 0x03, 0, 0},
	{0x5817, 0x08, 0, 0}, {0x5818, 0x0d, 0, 0}, {0x5819, 0x08, 0, 0},
	{0x581a, 0x05, 0, 0}, {0x581b, 0x06, 0, 0}, {0x581c, 0x08, 0, 0},
	{0x581d, 0x0e, 0, 0}, {0x581e, 0x29, 0, 0}, {0x581f, 0x17, 0, 0},
	{0x5820, 0x11, 0, 0}, {0x5821, 0x11, 0, 0}, {0x5822, 0x15, 0, 0},
	{0x5823, 0x28, 0, 0}, {0x5824, 0x46, 0, 0}, {0x5825, 0x26, 0, 0},
	{0x5826, 0x08, 0, 0}, {0x5827, 0x26, 0, 0}, {0x5828, 0x64, 0, 0},
	{0x5829, 0x26, 0, 0}, {0x582a, 0x24, 0, 0}, {0x582b, 0x22, 0, 0},
	{0x582c, 0x24, 0, 0}, {0x582d, 0x24, 0, 0}, {0x582e, 0x06, 0, 0},
	{0x582f, 0x22, 0, 0}, {0x5830, 0x40, 0, 0}, {0x5831, 0x42, 0, 0},
	{0x5832, 0x24, 0, 0}, {0x5833, 0x26, 0, 0}, {0x5834, 0x24, 0, 0},
	{0x5835, 0x22, 0, 0}, {0x5836, 0x22, 0, 0}, {0x5837, 0x26, 0, 0},
	{0x5838, 0x44, 0, 0}, {0x5839, 0x24, 0, 0}, {0x583a, 0x26, 0, 0},
	{0x583b, 0x28, 0, 0}, {0x583c, 0x42, 0, 0}, {0x583d, 0xce, 0, 0},
	{0x5025, 0x00, 0, 0}, {0x3a0f, 0x30, 0, 0}, {0x3a10, 0x28, 0, 0},
	{0x3a1b, 0x30, 0, 0}, {0x3a1e, 0x26, 0, 0}, {0x3a11, 0x60, 0, 0},
	{0x3a1f, 0x14, 0, 0}, {0x3008, 0x42, 0, 0}, {0x3c00, 0x04, 0, 300},
};

static const struct reg_value ov5640_setting_30fps_VGA_640_480[] = {
	{0x3008, 0x42, 0, 0},
	{0x3035, 0x12, 0, 0}, {0x3036, 0x38, 0, 0}, {0x3c07, 0x08, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9b, 0, 0},
	{0x3808, 0x02, 0, 0}, {0x3809, 0x80, 0, 0}, {0x380a, 0x01, 0, 0},
	{0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x68, 0, 0},
	{0x380e, 0x04, 0, 0}, {0x380f, 0x38, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x06, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x0e, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x02, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x3824, 0x02, 0, 0}, {0x5001, 0xa3, 0, 0},
	{0x4005, 0x1a, 0, 0}, {0x3008, 0x02, 0, 0}, {0x3503, 0x00, 0, 0},
};

static const struct reg_value ov5640_setting_30fps_NTSC_720_480[] = {
	{0x3008, 0x42, 0, 0},
	{0x3035, 0x12, 0, 0}, {0x3036, 0x38, 0, 0}, {0x3c07, 0x08, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x04, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9b, 0, 0},
	{0x3808, 0x02, 0, 0}, {0x3809, 0xd0, 0, 0}, {0x380a, 0x01, 0, 0},
	{0x380b, 0xe0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x68, 0, 0},
	{0x380e, 0x03, 0, 0}, {0x380f, 0xd8, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x3c, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x27, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x02, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x3824, 0x02, 0, 0}, {0x5001, 0xa3, 0, 0},
	{0x4005, 0x1a, 0, 0}, {0x3008, 0x02, 0, 0}, {0x3503, 0, 0, 0},
};

static const struct reg_value ov5640_setting_30fps_720P_1280_720[] = {
	{0x3008, 0x42, 0, 0},
	{0x3035, 0x21, 0, 0}, {0x3036, 0x54, 0, 0}, {0x3c07, 0x07, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x41, 0, 0}, {0x3821, 0x07, 0, 0}, {0x3814, 0x31, 0, 0},
	{0x3815, 0x31, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0xfa, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x06, 0, 0}, {0x3807, 0xa9, 0, 0},
	{0x3808, 0x05, 0, 0}, {0x3809, 0x00, 0, 0}, {0x380a, 0x02, 0, 0},
	{0x380b, 0xd0, 0, 0}, {0x380c, 0x07, 0, 0}, {0x380d, 0x64, 0, 0},
	{0x380e, 0x02, 0, 0}, {0x380f, 0xe4, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x04, 0, 0},
	{0x3618, 0x00, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3709, 0x52, 0, 0}, {0x370c, 0x03, 0, 0}, {0x3a02, 0x02, 0, 0},
	{0x3a03, 0xe4, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0xbc, 0, 0},
	{0x3a0a, 0x01, 0, 0}, {0x3a0b, 0x72, 0, 0}, {0x3a0e, 0x01, 0, 0},
	{0x3a0d, 0x02, 0, 0}, {0x3a14, 0x02, 0, 0}, {0x3a15, 0xe4, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x02, 0, 0}, {0x4713, 0x02, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x37, 0, 0}, {0x460c, 0x20, 0, 0},
	{0x3824, 0x04, 0, 0}, {0x5001, 0x83, 0, 0}, {0x4005, 0x1a, 0, 0},
	{0x3008, 0x02, 0, 0}, {0x3503, 0,    0, 0},
};

static const struct reg_value ov5640_setting_30fps_1080P_1920_1080[] = {
	{0x3008, 0x42, 0, 0},
	{0x3035, 0x21, 0, 0}, {0x3036, 0x54, 0, 0}, {0x3c07, 0x08, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x40, 0, 0}, {0x3821, 0x06, 0, 0}, {0x3814, 0x11, 0, 0},
	{0x3815, 0x11, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x00, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9f, 0, 0},
	{0x3808, 0x0a, 0, 0}, {0x3809, 0x20, 0, 0}, {0x380a, 0x07, 0, 0},
	{0x380b, 0x98, 0, 0}, {0x380c, 0x0b, 0, 0}, {0x380d, 0x1c, 0, 0},
	{0x380e, 0x07, 0, 0}, {0x380f, 0xb0, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x04, 0, 0},
	{0x3618, 0x04, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x21, 0, 0},
	{0x3709, 0x12, 0, 0}, {0x370c, 0x00, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x27, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x06, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x3824, 0x02, 0, 0}, {0x5001, 0x83, 0, 0}, {0x3035, 0x11, 0, 0},
	{0x3036, 0x54, 0, 0}, {0x3c07, 0x07, 0, 0}, {0x3c08, 0x00, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3800, 0x01, 0, 0}, {0x3801, 0x50, 0, 0}, {0x3802, 0x01, 0, 0},
	{0x3803, 0xb2, 0, 0}, {0x3804, 0x08, 0, 0}, {0x3805, 0xef, 0, 0},
	{0x3806, 0x05, 0, 0}, {0x3807, 0xf1, 0, 0}, {0x3808, 0x07, 0, 0},
	{0x3809, 0x80, 0, 0}, {0x380a, 0x04, 0, 0}, {0x380b, 0x38, 0, 0},
	{0x380c, 0x09, 0, 0}, {0x380d, 0xc4, 0, 0}, {0x380e, 0x04, 0, 0},
	{0x380f, 0x60, 0, 0}, {0x3612, 0x2b, 0, 0}, {0x3708, 0x64, 0, 0},
	{0x3a02, 0x04, 0, 0}, {0x3a03, 0x60, 0, 0}, {0x3a08, 0x01, 0, 0},
	{0x3a09, 0x50, 0, 0}, {0x3a0a, 0x01, 0, 0}, {0x3a0b, 0x18, 0, 0},
	{0x3a0e, 0x03, 0, 0}, {0x3a0d, 0x04, 0, 0}, {0x3a14, 0x04, 0, 0},
	{0x3a15, 0x60, 0, 0}, {0x4713, 0x02, 0, 0}, {0x4407, 0x04, 0, 0},
	{0x460b, 0x37, 0, 0}, {0x460c, 0x20, 0, 0}, {0x3824, 0x04, 0, 0},
	{0x4005, 0x1a, 0, 0}, {0x3008, 0x02, 0, 0},
	{0x3503, 0, 0, 0},
};

static const struct reg_value ov5640_setting_15fps_QSXGA_2592_1944[] = {
	{0x3008, 0x42, 0, 0},
	{0x4202, 0x0f, 0, 0},	/* stream off the sensor */
	{0x3820, 0x40, 0, 0}, {0x3821, 0x06, 0, 0}, /*disable flip*/
	{0x3035, 0x21, 0, 0}, {0x3036, 0x54, 0, 0}, {0x3c07, 0x08, 0, 0},
	{0x3c09, 0x1c, 0, 0}, {0x3c0a, 0x9c, 0, 0}, {0x3c0b, 0x40, 0, 0},
	{0x3820, 0x40, 0, 0}, {0x3821, 0x06, 0, 0}, {0x3814, 0x11, 0, 0},
	{0x3815, 0x11, 0, 0}, {0x3800, 0x00, 0, 0}, {0x3801, 0x00, 0, 0},
	{0x3802, 0x00, 0, 0}, {0x3803, 0x00, 0, 0}, {0x3804, 0x0a, 0, 0},
	{0x3805, 0x3f, 0, 0}, {0x3806, 0x07, 0, 0}, {0x3807, 0x9f, 0, 0},
	{0x3808, 0x0a, 0, 0}, {0x3809, 0x20, 0, 0}, {0x380a, 0x07, 0, 0},
	{0x380b, 0x98, 0, 0}, {0x380c, 0x0b, 0, 0}, {0x380d, 0x1c, 0, 0},
	{0x380e, 0x07, 0, 0}, {0x380f, 0xb0, 0, 0}, {0x3810, 0x00, 0, 0},
	{0x3811, 0x10, 0, 0}, {0x3812, 0x00, 0, 0}, {0x3813, 0x04, 0, 0},
	{0x3618, 0x04, 0, 0}, {0x3612, 0x29, 0, 0}, {0x3708, 0x21, 0, 0},
	{0x3709, 0x12, 0, 0}, {0x370c, 0x00, 0, 0}, {0x3a02, 0x03, 0, 0},
	{0x3a03, 0xd8, 0, 0}, {0x3a08, 0x01, 0, 0}, {0x3a09, 0x27, 0, 0},
	{0x3a0a, 0x00, 0, 0}, {0x3a0b, 0xf6, 0, 0}, {0x3a0e, 0x03, 0, 0},
	{0x3a0d, 0x04, 0, 0}, {0x3a14, 0x03, 0, 0}, {0x3a15, 0xd8, 0, 0},
	{0x4001, 0x02, 0, 0}, {0x4004, 0x06, 0, 0}, {0x4713, 0x03, 0, 0},
	{0x4407, 0x04, 0, 0}, {0x460b, 0x35, 0, 0}, {0x460c, 0x22, 0, 0},
	{0x3824, 0x02, 0, 0}, {0x5001, 0x83, 0, 70}, {0x3008, 0x02, 0, 0},
	{0x4202, 0x00, 0, 0},	/* stream on the sensor */
};

static const struct ov5640_mode_info ov5640_mode_info_data[2][ov5640_mode_MAX + 1] = {
	{
		{ov5640_mode_VGA_640_480, -1, 0, 0, NULL, 0},
		{ov5640_mode_NTSC_720_480, -1, 0, 0, NULL, 0},
		{ov5640_mode_720P_1280_720, -1, 0, 0, NULL, 0},
		{ov5640_mode_1080P_1920_1080, -1, 0, 0, NULL, 0},
		{ov5640_mode_QSXGA_2592_1944, SCALING, 2592, 1944,
		ov5640_setting_15fps_QSXGA_2592_1944,
		ARRAY_SIZE(ov5640_setting_15fps_QSXGA_2592_1944)},
	},
	{
		{ov5640_mode_VGA_640_480, SUBSAMPLING, 640,  480,
		ov5640_setting_30fps_VGA_640_480,
		ARRAY_SIZE(ov5640_setting_30fps_VGA_640_480)},
		{ov5640_mode_NTSC_720_480, SUBSAMPLING, 720, 480,
		ov5640_setting_30fps_NTSC_720_480,
		ARRAY_SIZE(ov5640_setting_30fps_NTSC_720_480)},
		{ov5640_mode_720P_1280_720, SUBSAMPLING, 1280, 720,
		ov5640_setting_30fps_720P_1280_720,
		ARRAY_SIZE(ov5640_setting_30fps_720P_1280_720)},
		{ov5640_mode_1080P_1920_1080, SCALING, 1920, 1080,
		ov5640_setting_30fps_1080P_1920_1080,
		ARRAY_SIZE(ov5640_setting_30fps_1080P_1920_1080)},
		{ov5640_mode_QSXGA_2592_1944, -1, 0, 0, NULL, 0},
	},
};

static int ov5640_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int ov5640_remove(struct i2c_client *client);

static const struct i2c_device_id ov5640_id[] = {
	{"ov5640_mipisubdev", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "ov5640_mipisubdev",
		  },
	.probe  = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_id,
};

static const struct ov5640_datafmt ov5640_colour_fmts[] = {
	{MEDIA_BUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
};

static int get_capturemode(int width, int height)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5640_valid_res); i++) {
		if ((ov5640_valid_res[i].width == width) &&
		     (ov5640_valid_res[i].height == height))
			return i;
	}

	return -1;
}

static struct ov5640 *to_ov5640(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct ov5640, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct ov5640_datafmt
			*ov5640_find_datafmt(u32 code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ov5640_colour_fmts); i++)
		if (ov5640_colour_fmts[i].code == code)
			return ov5640_colour_fmts + i;

	return NULL;
}

static inline void ov5640_power_down(struct ov5640 *sensor, int enable)
{
	if (sensor->pwn_gpio < 0)
		return;

	if (!enable)
		gpio_set_value_cansleep(sensor->pwn_gpio, 0);
	else
		gpio_set_value_cansleep(sensor->pwn_gpio, 1);

	msleep(2);
}

static void ov5640_reset(struct ov5640 *sensor)
{
	if (sensor->rst_gpio < 0 || sensor->pwn_gpio < 0)
		return;

	/* camera reset */
	gpio_set_value(sensor->rst_gpio, 1);

	/* camera power dowmn */
	gpio_set_value(sensor->pwn_gpio, 1);
	msleep(5);

	gpio_set_value(sensor->rst_gpio, 0);
	msleep(1);

	gpio_set_value(sensor->pwn_gpio, 0);
	msleep(5);

	gpio_set_value(sensor->rst_gpio, 1);
	msleep(5);
}

static int ov5640_regulator_enable(struct ov5640 *sensor, struct device *dev)
{
	int ret = 0;

	sensor->io_regulator = devm_regulator_get(dev, "DOVDD");
	if (!IS_ERR(sensor->io_regulator)) {
		regulator_set_voltage(sensor->io_regulator,
				      OV5640_VOLTAGE_DIGITAL_IO,
				      OV5640_VOLTAGE_DIGITAL_IO);
		ret = regulator_enable(sensor->io_regulator);
		if (ret) {
			pr_err("%s:io set voltage error\n", __func__);
			return ret;
		} else {
			dev_dbg(dev,
				"%s:io set voltage ok\n", __func__);
		}
	} else {
		pr_err("%s: cannot get io voltage error\n", __func__);
		sensor->io_regulator = NULL;
	}

	sensor->core_regulator = devm_regulator_get(dev, "DVDD");
	if (!IS_ERR(sensor->core_regulator)) {
		regulator_set_voltage(sensor->core_regulator,
				      OV5640_VOLTAGE_DIGITAL_CORE,
				      OV5640_VOLTAGE_DIGITAL_CORE);
		ret = regulator_enable(sensor->core_regulator);
		if (ret) {
			pr_err("%s:core set voltage error\n", __func__);
			return ret;
		} else {
			dev_dbg(dev,
				"%s:core set voltage ok\n", __func__);
		}
	} else {
		sensor->core_regulator = NULL;
		pr_err("%s: cannot get core voltage error\n", __func__);
	}

	sensor->analog_regulator = devm_regulator_get(dev, "AVDD");
	if (!IS_ERR(sensor->analog_regulator)) {
		regulator_set_voltage(sensor->analog_regulator,
				      OV5640_VOLTAGE_ANALOG,
				      OV5640_VOLTAGE_ANALOG);
		ret = regulator_enable(sensor->analog_regulator);
		if (ret) {
			pr_err("%s:analog set voltage error\n",
				__func__);
			return ret;
		} else {
			dev_dbg(dev,
				"%s:analog set voltage ok\n", __func__);
		}
	} else {
		sensor->analog_regulator = NULL;
		pr_err("%s: cannot get analog voltage error\n", __func__);
	}

	return ret;
}

static s32 ov5640_write_reg(struct ov5640 *sensor, u16 reg, u8 val)
{
	u8 au8Buf[3] = {0};

	au8Buf[0] = reg >> 8;
	au8Buf[1] = reg & 0xff;
	au8Buf[2] = val;

	if (i2c_master_send(sensor->i2c_client, au8Buf, 3) < 0) {
		pr_err("%s(mipi):write reg error:reg=%x,val=%x\n",
			__func__, reg, val);
		return -1;
	}

	return 0;
}

static s32 ov5640_read_reg(struct ov5640 *sensor, u16 reg, u8 *val)
{
	struct i2c_client *client = sensor->i2c_client;
	struct i2c_msg msgs[2];
	u8 buf[2];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf = buf;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("%s(mipi):reg=%x ret=%d\n", __func__, reg, ret);
		return ret;
	}

	*val = buf[0];
	pr_debug("%s(mipi):reg=%x,val=%x\n", __func__, reg, buf[0]);
	return buf[0];
}

static void OV5640_stream_on(struct ov5640 *sensor)
{
	ov5640_write_reg(sensor, 0x4202, 0x00);
}

static void OV5640_stream_off(struct ov5640 *sensor)
{
	ov5640_write_reg(sensor, 0x4202, 0x0f);
	ov5640_write_reg(sensor, 0x3008, 0x42);
}

static int OV5640_get_sysclk(struct ov5640 *sensor)
{
	/* calculate sysclk */
	int tmp;
	unsigned Multiplier, PreDiv, SysDiv, Pll_rdiv, Bit_div2x = 1;
	unsigned div, sclk_rdiv, sysclk;
	u8 temp;

	int sclk_rdiv_map[] = {1, 2, 4, 8};

	tmp = ov5640_read_reg(sensor, 0x3034, &temp);
	if (tmp < 0)
		return tmp;
	tmp &= 0x0f;
	if (tmp == 8 || tmp == 10)
		Bit_div2x = tmp / 2;

	tmp = ov5640_read_reg(sensor, 0x3035, &temp);
	if (tmp < 0)
		return tmp;
	SysDiv = tmp >> 4;
	if (SysDiv == 0)
		SysDiv = 16;

	tmp = ov5640_read_reg(sensor, 0x3036, &temp);
	if (tmp < 0)
		return tmp;
	Multiplier = tmp;

	tmp = ov5640_read_reg(sensor, 0x3037, &temp);
	if (tmp < 0)
		return tmp;
	PreDiv = tmp & 0x0f;
	Pll_rdiv = ((tmp >> 4) & 0x01) + 1;

	tmp = ov5640_read_reg(sensor, 0x3108, &temp);
	if (tmp < 0)
		return tmp;
	sclk_rdiv = sclk_rdiv_map[tmp & 0x03];

	sysclk = sensor->mclk / 10000 * Multiplier;
	div = PreDiv * SysDiv * Pll_rdiv * Bit_div2x * sclk_rdiv;
	if (!div) {
		pr_err("%s:Error divide by 0, (%d * %d * %d * %d * %d)\n",
		       __func__, PreDiv, SysDiv, Pll_rdiv, Bit_div2x, sclk_rdiv);
		return -EINVAL;
	}
	if (!sysclk) {
		pr_err("%s:Error 0 clk, sensor->mclk=%d, Multiplier=%d\n",
		       __func__, sensor->mclk, Multiplier);
		return -EINVAL;
	}
	sysclk /= div;
	pr_debug("%s: sysclk(%d) = %d / 10000 * %d / (%d * %d * %d * %d * %d)\n",
		 __func__, sysclk, sensor->mclk, Multiplier,
		 PreDiv, SysDiv, Pll_rdiv, Bit_div2x, sclk_rdiv);

	return sysclk;
}

static void OV5640_set_night_mode(struct ov5640 *sensor)
{
	 /* read HTS from register settings */
	u8 mode;

	ov5640_read_reg(sensor, 0x3a00, &mode);
	mode &= 0xfb;
	ov5640_write_reg(sensor, 0x3a00, mode);
}

static int OV5640_get_HTS(struct ov5640 *sensor)
{
	 /* read HTS from register settings */
	int HTS;
	u8 temp;

	HTS = ov5640_read_reg(sensor, 0x380c, &temp);
	HTS = (HTS<<8) + ov5640_read_reg(sensor, 0x380d, &temp);

	return HTS;
}

static int OV5640_get_VTS(struct ov5640 *sensor)
{
	 /* read VTS from register settings */
	int VTS;
	u8 temp;

	/* total vertical size[15:8] high byte */
	VTS = ov5640_read_reg(sensor, 0x380e, &temp);

	VTS = (VTS<<8) + ov5640_read_reg(sensor, 0x380f, &temp);

	return VTS;
}

static int OV5640_set_VTS(struct ov5640 *sensor, int VTS)
{
	 /* write VTS to registers */
	 int temp;

	 temp = VTS & 0xff;
	 ov5640_write_reg(sensor, 0x380f, temp);

	 temp = VTS>>8;
	 ov5640_write_reg(sensor, 0x380e, temp);

	 return 0;
}

static int OV5640_get_shutter(struct ov5640 *sensor)
{
	 /* read shutter, in number of line period */
	int shutter;
	u8 temp;

	shutter = (ov5640_read_reg(sensor, 0x03500, &temp) & 0x0f);
	shutter = (shutter<<8) + ov5640_read_reg(sensor, 0x3501, &temp);
	shutter = (shutter<<4) + (ov5640_read_reg(sensor, 0x3502, &temp)>>4);

	 return shutter;
}

static int OV5640_set_shutter(struct ov5640 *sensor, int shutter)
{
	 /* write shutter, in number of line period */
	 int temp;

	 shutter = shutter & 0xffff;

	 temp = shutter & 0x0f;
	 temp = temp<<4;
	 ov5640_write_reg(sensor, 0x3502, temp);

	 temp = shutter & 0xfff;
	 temp = temp>>4;
	 ov5640_write_reg(sensor, 0x3501, temp);

	 temp = shutter>>12;
	 ov5640_write_reg(sensor, 0x3500, temp);

	 return 0;
}

static int OV5640_get_gain16(struct ov5640 *sensor)
{
	 /* read gain, 16 = 1x */
	int gain16;
	u8 temp;

	gain16 = ov5640_read_reg(sensor, 0x350a, &temp) & 0x03;
	gain16 = (gain16<<8) + ov5640_read_reg(sensor, 0x350b, &temp);

	return gain16;
}

static int OV5640_set_gain16(struct ov5640 *sensor, int gain16)
{
	/* write gain, 16 = 1x */
	u8 temp;
	gain16 = gain16 & 0x3ff;

	temp = gain16 & 0xff;
	ov5640_write_reg(sensor, 0x350b, temp);

	temp = gain16>>8;
	ov5640_write_reg(sensor, 0x350a, temp);

	return 0;
}

static int OV5640_get_light_freq(struct ov5640 *sensor)
{
	/* get banding filter value */
	int temp, temp1, light_freq = 0;
	u8 tmp;

	temp = ov5640_read_reg(sensor, 0x3c01, &tmp);

	if (temp & 0x80) {
		/* manual */
		temp1 = ov5640_read_reg(sensor, 0x3c00, &tmp);
		if (temp1 & 0x04) {
			/* 50Hz */
			light_freq = 50;
		} else {
			/* 60Hz */
			light_freq = 60;
		}
	} else {
		/* auto */
		temp1 = ov5640_read_reg(sensor, 0x3c0c, &tmp);
		if (temp1 & 0x01) {
			/* 50Hz */
			light_freq = 50;
		} else {
			/* 60Hz */
		}
	}
	return light_freq;
}

static void OV5640_set_bandingfilter(struct ov5640 *sensor)
{
	int prev_VTS;
	int band_step60, max_band60, band_step50, max_band50;

	/* read preview PCLK */
	sensor->prev_sysclk = OV5640_get_sysclk(sensor);
	/* read preview HTS */
	sensor->prev_HTS = OV5640_get_HTS(sensor);

	/* read preview VTS */
	prev_VTS = OV5640_get_VTS(sensor);

	/* calculate banding filter */
	/* 60Hz */
	band_step60 = sensor->prev_sysclk * 100/sensor->prev_HTS * 100/120;
	ov5640_write_reg(sensor, 0x3a0a, (band_step60 >> 8));
	ov5640_write_reg(sensor, 0x3a0b, (band_step60 & 0xff));

	max_band60 = (int)((prev_VTS-4)/band_step60);
	ov5640_write_reg(sensor, 0x3a0d, max_band60);

	/* 50Hz */
	band_step50 = sensor->prev_sysclk * 100/sensor->prev_HTS;
	ov5640_write_reg(sensor, 0x3a08, (band_step50 >> 8));
	ov5640_write_reg(sensor, 0x3a09, (band_step50 & 0xff));

	max_band50 = (int)((prev_VTS-4)/band_step50);
	ov5640_write_reg(sensor, 0x3a0e, max_band50);
}

static int OV5640_set_AE_target(struct ov5640 *sensor, int target)
{
	/* stable in high */
	int fast_high, fast_low;
	sensor->AE_low = target * 23 / 25;	/* 0.92 */
	sensor->AE_high = target * 27 / 25;	/* 1.08 */

	fast_high = sensor->AE_high<<1;
	if (fast_high > 255)
		fast_high = 255;

	fast_low = sensor->AE_low >> 1;

	ov5640_write_reg(sensor, 0x3a0f, sensor->AE_high);
	ov5640_write_reg(sensor, 0x3a10, sensor->AE_low);
	ov5640_write_reg(sensor, 0x3a1b, sensor->AE_high);
	ov5640_write_reg(sensor, 0x3a1e, sensor->AE_low);
	ov5640_write_reg(sensor, 0x3a11, fast_high);
	ov5640_write_reg(sensor, 0x3a1f, fast_low);

	return 0;
}

static void OV5640_turn_on_AE_AG(struct ov5640 *sensor, int enable)
{
	u8 ae_ag_ctrl;

	ov5640_read_reg(sensor, 0x3503, &ae_ag_ctrl);
	if (enable) {
		/* turn on auto AE/AG */
		ae_ag_ctrl = ae_ag_ctrl & ~(0x03);
	} else {
		/* turn off AE/AG */
		ae_ag_ctrl = ae_ag_ctrl | 0x03;
	}
	ov5640_write_reg(sensor, 0x3503, ae_ag_ctrl);
}

static bool binning_on(struct ov5640 *sensor)
{
	u8 temp;
	ov5640_read_reg(sensor, 0x3821, &temp);
	temp &= 0xfe;
	if (temp)
		return true;
	else
		return false;
}

static void ov5640_set_virtual_channel(struct ov5640 *sensor, int channel)
{
	u8 channel_id;

	ov5640_read_reg(sensor, 0x4814, &channel_id);
	channel_id &= ~(3 << 6);
	ov5640_write_reg(sensor, 0x4814, channel_id | (channel << 6));
}

/* download ov5640 settings to sensor through i2c */
static int ov5640_download_firmware(struct ov5640 *sensor, const struct reg_value *pModeSetting, s32 ArySize)
{
	register u32 Delay_ms = 0;
	register u16 RegAddr = 0;
	register u8 Mask = 0;
	register u8 Val = 0;
	u8 RegVal = 0;
	int i, retval = 0;

	for (i = 0; i < ArySize; ++i, ++pModeSetting) {
		Delay_ms = pModeSetting->u32Delay_ms;
		RegAddr = pModeSetting->u16RegAddr;
		Val = pModeSetting->u8Val;
		Mask = pModeSetting->u8Mask;

		if (Mask) {
			retval = ov5640_read_reg(sensor, RegAddr, &RegVal);
			if (retval < 0)
				goto err;

			if ((sensor->mclk == OV5640_XCLK_20MHZ) && (RegAddr == 0x3037))
				Val = 0x17;

			RegVal &= ~(u8)Mask;
			Val &= Mask;
			Val |= RegVal;
		}

		retval = ov5640_write_reg(sensor, RegAddr, Val);
		if (retval < 0)
			goto err;

		if (Delay_ms)
			msleep(Delay_ms);
	}
err:
	return retval;
}

/* sensor changes between scaling and subsampling
 * go through exposure calcualtion
 */
static int ov5640_change_mode_exposure_calc(struct ov5640 *sensor,
		enum ov5640_frame_rate frame_rate, enum ov5640_mode mode)
{
	const struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0;
	u8 average;
	int prev_shutter, prev_gain16;
	int cap_shutter, cap_gain16;
	int cap_sysclk, cap_HTS, cap_VTS;
	int light_freq, cap_bandfilt, cap_maxband;
	long cap_gain16_shutter;
	int retval = 0;

	/* check if the input mode and frame rate is valid */
	pModeSetting =
		ov5640_mode_info_data[frame_rate][mode].init_data_ptr;
	ArySize =
		ov5640_mode_info_data[frame_rate][mode].init_data_size;

	sensor->pix.width =
		ov5640_mode_info_data[frame_rate][mode].width;
	sensor->pix.height =
		ov5640_mode_info_data[frame_rate][mode].height;

	if (sensor->pix.width == 0 || sensor->pix.height == 0 ||
		pModeSetting == NULL || ArySize == 0)
		return -EINVAL;

	/* auto focus */
	/* OV5640_auto_focus();//if no af function, just skip it */

	/* turn off AE/AG */
	OV5640_turn_on_AE_AG(sensor, 0);

	/* read preview shutter */
	prev_shutter = OV5640_get_shutter(sensor);
	if ((binning_on(sensor)) && (mode != ov5640_mode_720P_1280_720)
			&& (mode != ov5640_mode_1080P_1920_1080))
		prev_shutter *= 2;

	/* read preview gain */
	prev_gain16 = OV5640_get_gain16(sensor);

	/* get average */
	ov5640_read_reg(sensor, 0x56a1, &average);

	/* turn off night mode for capture */
	OV5640_set_night_mode(sensor);

	/* turn off overlay */
	/* ov5640_write_reg(sensor, 0x3022, 0x06);//if no af function, just skip it */

	OV5640_stream_off(sensor);

	/* Write capture setting */
	retval = ov5640_download_firmware(sensor, pModeSetting, ArySize);
	if (retval < 0)
		goto err;

	/* read capture VTS */
	cap_VTS = OV5640_get_VTS(sensor);
	cap_HTS = OV5640_get_HTS(sensor);
	cap_sysclk = OV5640_get_sysclk(sensor);

	/* calculate capture banding filter */
	light_freq = OV5640_get_light_freq(sensor);
	if (light_freq == 60) {
		/* 60Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_HTS * 100 / 120;
	} else {
		/* 50Hz */
		cap_bandfilt = cap_sysclk * 100 / cap_HTS;
	}
	cap_maxband = (int)((cap_VTS - 4)/cap_bandfilt);

	/* calculate capture shutter/gain16 */
	if (average > sensor->AE_low && average < sensor->AE_high) {
		/* in stable range */
		cap_gain16_shutter =
		  prev_gain16 * prev_shutter * cap_sysclk/sensor->prev_sysclk
		  * sensor->prev_HTS/cap_HTS * sensor->AE_Target / average;
	} else {
		cap_gain16_shutter =
		  prev_gain16 * prev_shutter * cap_sysclk/sensor->prev_sysclk
		  * sensor->prev_HTS/cap_HTS;
	}

	/* gain to shutter */
	if (cap_gain16_shutter < (cap_bandfilt * 16)) {
		/* shutter < 1/100 */
		cap_shutter = cap_gain16_shutter/16;
		if (cap_shutter < 1)
			cap_shutter = 1;

		cap_gain16 = cap_gain16_shutter/cap_shutter;
		if (cap_gain16 < 16)
			cap_gain16 = 16;
	} else {
		if (cap_gain16_shutter >
				(cap_bandfilt * cap_maxband * 16)) {
			/* exposure reach max */
			cap_shutter = cap_bandfilt * cap_maxband;
			cap_gain16 = cap_gain16_shutter / cap_shutter;
		} else {
			/* 1/100 < (cap_shutter = n/100) =< max */
			cap_shutter =
			  ((int) (cap_gain16_shutter/16 / cap_bandfilt))
			  *cap_bandfilt;
			cap_gain16 = cap_gain16_shutter / cap_shutter;
		}
	}

	/* write capture gain */
	OV5640_set_gain16(sensor, cap_gain16);

	/* write capture shutter */
	if (cap_shutter > (cap_VTS - 4)) {
		cap_VTS = cap_shutter + 4;
		OV5640_set_VTS(sensor, cap_VTS);
	}
	OV5640_set_shutter(sensor, cap_shutter);

err:
	return retval;
}

/* if sensor changes inside scaling or subsampling
 * change mode directly
 * */
static int ov5640_change_mode_direct(struct ov5640 *sensor,
		enum ov5640_frame_rate frame_rate, enum ov5640_mode mode)
{
	const struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0;
	int retval = 0;

	/* check if the input mode and frame rate is valid */
	pModeSetting =
		ov5640_mode_info_data[frame_rate][mode].init_data_ptr;
	ArySize =
		ov5640_mode_info_data[frame_rate][mode].init_data_size;

	sensor->pix.width =
		ov5640_mode_info_data[frame_rate][mode].width;
	sensor->pix.height =
		ov5640_mode_info_data[frame_rate][mode].height;

	if (sensor->pix.width == 0 || sensor->pix.height == 0 ||
		pModeSetting == NULL || ArySize == 0)
		return -EINVAL;

	/* turn off AE/AG */
	OV5640_turn_on_AE_AG(sensor, 0);

	OV5640_stream_off(sensor);

	/* Write capture setting */
	retval = ov5640_download_firmware(sensor, pModeSetting, ArySize);
	if (retval < 0)
		goto err;

	OV5640_turn_on_AE_AG(sensor, 1);

err:
	return retval;
}

static int ov5640_init_mode(struct ov5640 *sensor,
		enum ov5640_frame_rate frame_rate,
		enum ov5640_mode mode, enum ov5640_mode orig_mode)
{
	const struct reg_value *pModeSetting = NULL;
	s32 ArySize = 0;
	int retval = 0;
	u32 msec_wait4stable = 0;
	enum ov5640_downsize_mode dn_mode, orig_dn_mode;

	if ((mode > ov5640_mode_MAX || mode < ov5640_mode_MIN)
		&& (mode != ov5640_mode_INIT)) {
		pr_err("Wrong ov5640 mode detected!\n");
		return -1;
	}

	dn_mode = ov5640_mode_info_data[frame_rate][mode].dn_mode;
	orig_dn_mode = ov5640_mode_info_data[frame_rate][orig_mode].dn_mode;
	if (mode == ov5640_mode_INIT) {
		pModeSetting = ov5640_init_setting_30fps_VGA;
		ArySize = ARRAY_SIZE(ov5640_init_setting_30fps_VGA);

		sensor->pix.width = 640;
		sensor->pix.height = 480;
		retval = ov5640_download_firmware(sensor, pModeSetting, ArySize);
		if (retval < 0)
			goto err;

		pModeSetting = ov5640_setting_30fps_VGA_640_480;
		ArySize = ARRAY_SIZE(ov5640_setting_30fps_VGA_640_480);
		retval = ov5640_download_firmware(sensor, pModeSetting, ArySize);
	} else if ((dn_mode == SUBSAMPLING && orig_dn_mode == SCALING) ||
			(dn_mode == SCALING && orig_dn_mode == SUBSAMPLING)) {
		/* change between subsampling and scaling
		 * go through exposure calucation */
		retval = ov5640_change_mode_exposure_calc(sensor, frame_rate, mode);
	} else {
		/* change inside subsampling or scaling
		 * download firmware directly */
		retval = ov5640_change_mode_direct(sensor, frame_rate, mode);
	}

	if (retval < 0)
		goto err;

	OV5640_set_AE_target(sensor, sensor->AE_Target);
	OV5640_get_light_freq(sensor);
	OV5640_set_bandingfilter(sensor);
	ov5640_set_virtual_channel(sensor, sensor->csi);

	/* add delay to wait for sensor stable */
	if (mode == ov5640_mode_QSXGA_2592_1944) {
		/* dump the first two frames: 1/7.5*2
		 * the frame rate of QSXGA is 7.5fps */
		msec_wait4stable = 267;
	} else {
		/* dump the first eighteen frames: 1/30*18 */
		msec_wait4stable = 600;
	}
	msleep(msec_wait4stable);

err:
	return retval;
}

/*!
 * ov5640_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ov5640_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	if (on && !sensor->on) {
		if (sensor->io_regulator)
			if (regulator_enable(sensor->io_regulator) != 0)
				return -EIO;
		if (sensor->core_regulator)
			if (regulator_enable(sensor->core_regulator) != 0)
				return -EIO;
		if (sensor->gpo_regulator)
			if (regulator_enable(sensor->gpo_regulator) != 0)
				return -EIO;
		if (sensor->analog_regulator)
			if (regulator_enable(sensor->analog_regulator) != 0)
				return -EIO;
	} else if (!on && sensor->on) {
		if (sensor->analog_regulator)
			regulator_disable(sensor->analog_regulator);
		if (sensor->core_regulator)
			regulator_disable(sensor->core_regulator);
		if (sensor->io_regulator)
			regulator_disable(sensor->io_regulator);
		if (sensor->gpo_regulator)
			regulator_disable(sensor->gpo_regulator);
	}

	sensor->on = on;

	return 0;
}

/*!
 * ov5640_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ov5640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ov5460_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 sub device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ov5640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum ov5640_frame_rate frame_rate;
	enum ov5640_mode orig_mode;
	int ret = 0;

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps == 15)
			frame_rate = ov5640_15_fps;
		else if (tgt_fps == 30)
			frame_rate = ov5640_30_fps;
		else {
			pr_err(" The camera frame rate is not supported!\n");
			return -EINVAL;
		}

		orig_mode = sensor->streamcap.capturemode;
		ret = ov5640_init_mode(sensor, frame_rate,
				(u32)a->parm.capture.capturemode, orig_mode);
		if (ret < 0)
			return ret;

		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;

		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ov5640_set_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct ov5640_datafmt *fmt = ov5640_find_datafmt(mf->code);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	int capturemode;

	if (!fmt) {
		mf->code	= ov5640_colour_fmts[0].code;
		mf->colorspace	= ov5640_colour_fmts[0].colorspace;
	}

	mf->field	= V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		return 0;

	sensor->fmt = fmt;

	capturemode = get_capturemode(mf->width, mf->height);
	if (capturemode >= 0) {
		sensor->streamcap.capturemode = capturemode;
		sensor->pix.width = mf->width;
		sensor->pix.height = mf->height;
		return 0;
	}

	dev_err(&client->dev, "%s set fail\n", __func__);
	return -EINVAL;
}


static int ov5640_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);
	const struct ov5640_datafmt *fmt = sensor->fmt;

	if (format->pad)
		return -EINVAL;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	mf->width	= sensor->pix.width;
	mf->height	= sensor->pix.height;

	return 0;
}

static int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov5640_colour_fmts))
		return -EINVAL;

	code->code = ov5640_colour_fmts[code->index].code;
	return 0;
}

/*!
 * ov5640_enum_framesizes - V4L2 sensor interface handler for
 *			   VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_framesizes(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index > ov5640_mode_MAX)
		return -EINVAL;

	fse->max_width =
			max(ov5640_mode_info_data[0][fse->index].width,
			    ov5640_mode_info_data[1][fse->index].width);
	fse->min_width = fse->max_width;
	fse->max_height =
			max(ov5640_mode_info_data[0][fse->index].height,
			    ov5640_mode_info_data[1][fse->index].height);
	fse->min_height = fse->max_height;
	return 0;
}

/*!
 * ov5640_enum_frameintervals - V4L2 sensor interface handler for
 *			       VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ov5640_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_subdev_pad_config *cfg,
		struct v4l2_subdev_frame_interval_enum *fie)
{
	int i, j, count = 0;

	if (fie->index < 0 || fie->index > ov5640_mode_MAX)
		return -EINVAL;

	if (fie->width == 0 || fie->height == 0 ||
	    fie->code == 0) {
		pr_warning("Please assign pixel format, width and height.\n");
		return -EINVAL;
	}

	fie->interval.numerator = 1;

	count = 0;
	for (i = 0; i < ARRAY_SIZE(ov5640_mode_info_data); i++) {
		for (j = 0; j < (ov5640_mode_MAX + 1); j++) {
			if (fie->width == ov5640_mode_info_data[i][j].width
			 && fie->height == ov5640_mode_info_data[i][j].height
			 && ov5640_mode_info_data[i][j].init_data_ptr != NULL) {
				count++;
			}
			if (fie->index == (count - 1)) {
				fie->interval.denominator =
						ov5640_framerates[i];
				return 0;
			}
		}
	}

	return -EINVAL;
}

/*!
 * dev_init - V4L2 sensor init
 * @s: pointer to standard V4L2 device structure
 *
 */
static int init_device(struct ov5640 *sensor)
{
	u32 tgt_xclk;	/* target xclk */
	u32 tgt_fps;	/* target frames per secound */
	enum ov5640_frame_rate frame_rate;
	int ret;

	sensor->on = true;

	/* mclk */
	tgt_xclk = sensor->mclk;
	tgt_xclk = min(tgt_xclk, (u32)OV5640_XCLK_MAX);
	tgt_xclk = max(tgt_xclk, (u32)OV5640_XCLK_MIN);
	sensor->mclk = tgt_xclk;

	pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);

	/* Default camera frame rate is set in probe */
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

	if (tgt_fps == 15)
		frame_rate = ov5640_15_fps;
	else if (tgt_fps == 30)
		frame_rate = ov5640_30_fps;
	else
		return -EINVAL; /* Only support 15fps or 30fps now. */

	ret = ov5640_init_mode(sensor, frame_rate, ov5640_mode_INIT, ov5640_mode_INIT);

	return ret;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *sensor = to_ov5640(client);

	if (enable)
		OV5640_stream_on(sensor);
	else
		OV5640_stream_off(sensor);
	return 0;
}

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.g_parm = ov5640_g_parm,
	.s_parm = ov5640_s_parm,
	.s_stream = ov5640_s_stream,
};

static const struct v4l2_subdev_pad_ops ov5640_subdev_pad_ops = {
	.enum_frame_size       = ov5640_enum_framesizes,
	.enum_frame_interval   = ov5640_enum_frameintervals,
	.enum_mbus_code        = ov5640_enum_mbus_code,
	.set_fmt               = ov5640_set_fmt,
	.get_fmt               = ov5640_get_fmt,
};

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_power	= ov5640_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov5640_get_register,
	.s_register	= ov5640_set_register,
#endif
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core	= &ov5640_subdev_core_ops,
	.video	= &ov5640_subdev_video_ops,
	.pad	= &ov5640_subdev_pad_ops,
};

/*!
 * ov5640 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov5640 *sensor;
	struct pinctrl *pinctrl;
	struct device *dev = &client->dev;
	int retval;
	u8 chip_id_high, chip_id_low;

	/* ov5640 pinctrl */
	pinctrl = devm_pinctrl_get_select_default(dev);
	if (IS_ERR(pinctrl))
		dev_warn(dev, "no pin available\n");

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	sensor->AE_Target = 52;

	/* request power down pin */
	sensor->pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(sensor->pwn_gpio))
		dev_warn(dev, "no sensor pwdn pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->pwn_gpio, GPIOF_OUT_INIT_HIGH,
						"ov5640_mipi_pwdn");
		if (retval < 0) {
			dev_warn(dev, "Failed to set power pin\n");
			dev_warn(dev, "retval=%d\n", retval);
			return retval;
		}
	}

	/* request reset pin */
	sensor->rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(sensor->rst_gpio))
		dev_warn(dev, "no sensor reset pin available");
	else {
		retval = devm_gpio_request_one(dev, sensor->rst_gpio, GPIOF_OUT_INIT_HIGH,
						"ov5640_mipi_reset");
		if (retval < 0) {
			dev_warn(dev, "Failed to set reset pin\n");
			return retval;
		}
	}

			/* Set initial values for the sensor struct. */
	sensor->sensor_clk = devm_clk_get(dev, "csi_mclk");
	if (IS_ERR(sensor->sensor_clk)) {
		/* assuming clock enabled by default */
		sensor->sensor_clk = NULL;
		dev_err(dev, "clock-frequency missing or invalid\n");
		return PTR_ERR(sensor->sensor_clk);
	}

	retval = of_property_read_u32(dev->of_node, "mclk",
					&(sensor->mclk));
	if (retval) {
		dev_err(dev, "mclk missing or invalid\n");
		return retval;
	}

	retval = of_property_read_u32(dev->of_node, "csi_id",
					&(sensor->csi));
	if (retval) {
		dev_err(dev, "csi id missing or invalid\n");
		return retval;
	}

	retval = clk_prepare_enable(sensor->sensor_clk);
	if (retval < 0) {
		dev_err(dev, "%s: enable sensor clk fail\n", __func__);
		return -EINVAL;
	}

	sensor->i2c_client = client;
	sensor->pix.pixelformat = V4L2_PIX_FMT_YUYV;
	sensor->pix.width = 640;
	sensor->pix.height = 480;
	sensor->streamcap.capability = V4L2_MODE_HIGHQUALITY |
					   V4L2_CAP_TIMEPERFRAME;
	sensor->streamcap.capturemode = 0;
	sensor->streamcap.timeperframe.denominator = DEFAULT_FPS;
	sensor->streamcap.timeperframe.numerator = 1;

	ov5640_regulator_enable(sensor, dev);

	ov5640_reset(sensor);

	ov5640_power_down(sensor, 0);

	retval = ov5640_read_reg(sensor, OV5640_CHIP_ID_HIGH_BYTE, &chip_id_high);
	if (retval < 0 || chip_id_high != 0x56) {
		pr_warning("camera ov5640_mipi is not found\n");
		clk_disable_unprepare(sensor->sensor_clk);
		return -ENODEV;
	}
	retval = ov5640_read_reg(sensor, OV5640_CHIP_ID_LOW_BYTE, &chip_id_low);
	if (retval < 0 || chip_id_low != 0x40) {
		pr_warning("camera ov5640_mipi is not found\n");
		clk_disable_unprepare(sensor->sensor_clk);
		return -ENODEV;
	}

	retval = init_device(sensor);
	if (retval < 0) {
		clk_disable_unprepare(sensor->sensor_clk);
		pr_warning("camera ov5640 init failed\n");
		ov5640_power_down(sensor, 1);
		return retval;
	}

	v4l2_i2c_subdev_init(&sensor->subdev, client, &ov5640_subdev_ops);

	sensor->subdev.grp_id = 678;
	retval = v4l2_async_register_subdev(&sensor->subdev);
	if (retval < 0)
		dev_err(&client->dev,
					"%s--Async register failed, ret=%d\n", __func__, retval);

	OV5640_stream_off(sensor);
	pr_info("camera ov5640_mipi is found\n");
	return retval;
}

/*!
 * ov5640 I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int ov5640_remove(struct i2c_client *client)
{
	struct ov5640 *sensor = to_ov5640(client);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_async_unregister_subdev(sd);

	clk_disable_unprepare(sensor->sensor_clk);

	ov5640_power_down(sensor, 1);

	if (sensor->gpo_regulator)
		regulator_disable(sensor->gpo_regulator);

	if (sensor->analog_regulator)
		regulator_disable(sensor->analog_regulator);

	if (sensor->core_regulator)
		regulator_disable(sensor->core_regulator);

	if (sensor->io_regulator)
		regulator_disable(sensor->io_regulator);

	return 0;
}

module_i2c_driver(ov5640_i2c_driver);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("OV5640 MIPI Camera Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
