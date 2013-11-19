/*
 * arch/arm/mach-tegra/panel-l-720p-5.c
 *
  * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mach/dc.h>
#include <mach/iomap.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm_backlight.h>
#include <linux/tegra_pwm_bl.h>
#include "devices.h"
#include <linux/mfd/max8831.h>
#include <linux/max8831_backlight.h>
#include <linux/leds.h>
#include <linux/ioport.h>
#include <linux/lm3528.h>

#include "gpio-names.h"
#include "board-panel.h"
#include "board.h"

#define DSI_PANEL_RESET         1

#define DC_CTRL_MODE            TEGRA_DC_OUT_CONTINUOUS_MODE

static struct regulator *vdd_lcd_s_1v8;
static struct regulator *vdd_sys_bl_3v7;
static struct regulator *avdd_lcd_3v0_2v8;

static bool dsi_otm1283a_720p_reg_requested;
static bool dsi_otm1283a_720p_gpio_requested = 0;
static bool is_bl_powered;
static struct platform_device *disp_device;

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu dsi_otm1283a_720p_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0x10D, 0x3F3, 0x000, /* 1.05036053  -0.05066457 0.00030404 */
		0x000, 0x0FC, 0x003, /* -0.00012137 0.98659651  0.01352485 */
		0x002, 0x001, 0x0FC, /* 0.00722989  0.00559134  0.98717878 */
	},
	/* lut2 maps linear space to sRGB */
	{
		0,    1,    2,    2,    3,    4,    5,    6,
		6,    7,    8,    9,    10,   10,   11,   12,
		13,   13,   14,   15,   15,   16,   16,   17,
		18,   18,   19,   19,   20,   20,   21,   21,
		22,   22,   23,   23,   23,   24,   24,   25,
		25,   25,   26,   26,   27,   27,   27,   28,
		28,   29,   29,   29,   30,   30,   30,   31,
		31,   31,   32,   32,   32,   33,   33,   33,
		34,   34,   34,   34,   35,   35,   35,   36,
		36,   36,   37,   37,   37,   37,   38,   38,
		38,   38,   39,   39,   39,   40,   40,   40,
		40,   41,   41,   41,   41,   42,   42,   42,
		42,   43,   43,   43,   43,   43,   44,   44,
		44,   44,   45,   45,   45,   45,   46,   46,
		46,   46,   46,   47,   47,   47,   47,   48,
		48,   48,   48,   48,   49,   49,   49,   49,
		49,   50,   50,   50,   50,   50,   51,   51,
		51,   51,   51,   52,   52,   52,   52,   52,
		53,   53,   53,   53,   53,   54,   54,   54,
		54,   54,   55,   55,   55,   55,   55,   55,
		56,   56,   56,   56,   56,   57,   57,   57,
		57,   57,   57,   58,   58,   58,   58,   58,
		58,   59,   59,   59,   59,   59,   59,   60,
		60,   60,   60,   60,   60,   61,   61,   61,
		61,   61,   61,   62,   62,   62,   62,   62,
		62,   63,   63,   63,   63,   63,   63,   64,
		64,   64,   64,   64,   64,   64,   65,   65,
		65,   65,   65,   65,   66,   66,   66,   66,
		66,   66,   66,   67,   67,   67,   67,   67,
		67,   67,   68,   68,   68,   68,   68,   68,
		68,   69,   69,   69,   69,   69,   69,   69,
		70,   70,   70,   70,   70,   70,   70,   71,
		71,   71,   71,   71,   71,   71,   72,   72,
		72,   72,   72,   72,   72,   72,   73,   73,
		73,   73,   73,   73,   73,   74,   74,   74,
		74,   74,   74,   74,   74,   75,   75,   75,
		75,   75,   75,   75,   75,   76,   76,   76,
		76,   76,   76,   76,   77,   77,   77,   77,
		77,   77,   77,   77,   78,   78,   78,   78,
		78,   78,   78,   78,   78,   79,   79,   79,
		79,   79,   79,   79,   79,   80,   80,   80,
		80,   80,   80,   80,   80,   81,   81,   81,
		81,   81,   81,   81,   81,   81,   82,   82,
		82,   82,   82,   82,   82,   82,   83,   83,
		83,   83,   83,   83,   83,   83,   83,   84,
		84,   84,   84,   84,   84,   84,   84,   84,
		85,   85,   85,   85,   85,   85,   85,   85,
		85,   86,   86,   86,   86,   86,   86,   86,
		86,   86,   87,   87,   87,   87,   87,   87,
		87,   87,   87,   88,   88,   88,   88,   88,
		88,   88,   88,   88,   88,   89,   89,   89,
		89,   89,   89,   89,   89,   89,   90,   90,
		90,   90,   90,   90,   90,   90,   90,   90,
		91,   91,   91,   91,   91,   91,   91,   91,
		91,   91,   92,   92,   92,   92,   92,   92,
		92,   92,   92,   92,   93,   93,   93,   93,
		93,   93,   93,   93,   93,   93,   94,   94,
		94,   94,   94,   94,   94,   94,   94,   94,
		95,   95,   95,   95,   95,   95,   95,   95,
		95,   95,   96,   96,   96,   96,   96,   96,
		96,   96,   96,   96,   96,   97,   97,   97,
		97,   97,   97,   97,   97,   97,   97,   98,
		98,   98,   98,   98,   98,   98,   98,   98,
		98,   98,   99,   99,   99,   99,   99,   99,
		99,   100,  101,  101,  102,  103,  103,  104,
		105,  105,  106,  107,  107,  108,  109,  109,
		110,  111,  111,  112,  113,  113,  114,  115,
		115,  116,  116,  117,  118,  118,  119,  119,
		120,  120,  121,  122,  122,  123,  123,  124,
		124,  125,  126,  126,  127,  127,  128,  128,
		129,  129,  130,  130,  131,  131,  132,  132,
		133,  133,  134,  134,  135,  135,  136,  136,
		137,  137,  138,  138,  139,  139,  140,  140,
		141,  141,  142,  142,  143,  143,  144,  144,
		145,  145,  145,  146,  146,  147,  147,  148,
		148,  149,  149,  150,  150,  150,  151,  151,
		152,  152,  153,  153,  153,  154,  154,  155,
		155,  156,  156,  156,  157,  157,  158,  158,
		158,  159,  159,  160,  160,  160,  161,  161,
		162,  162,  162,  163,  163,  164,  164,  164,
		165,  165,  166,  166,  166,  167,  167,  167,
		168,  168,  169,  169,  169,  170,  170,  170,
		171,  171,  172,  172,  172,  173,  173,  173,
		174,  174,  174,  175,  175,  176,  176,  176,
		177,  177,  177,  178,  178,  178,  179,  179,
		179,  180,  180,  180,  181,  181,  182,  182,
		182,  183,  183,  183,  184,  184,  184,  185,
		185,  185,  186,  186,  186,  187,  187,  187,
		188,  188,  188,  189,  189,  189,  189,  190,
		190,  190,  191,  191,  191,  192,  192,  192,
		193,  193,  193,  194,  194,  194,  195,  195,
		195,  196,  196,  196,  196,  197,  197,  197,
		198,  198,  198,  199,  199,  199,  200,  200,
		200,  200,  201,  201,  201,  202,  202,  202,
		202,  203,  203,  203,  204,  204,  204,  205,
		205,  205,  205,  206,  206,  206,  207,  207,
		207,  207,  208,  208,  208,  209,  209,  209,
		209,  210,  210,  210,  211,  211,  211,  211,
		212,  212,  212,  213,  213,  213,  213,  214,
		214,  214,  214,  215,  215,  215,  216,  216,
		216,  216,  217,  217,  217,  217,  218,  218,
		218,  219,  219,  219,  219,  220,  220,  220,
		220,  221,  221,  221,  221,  222,  222,  222,
		223,  223,  223,  223,  224,  224,  224,  224,
		225,  225,  225,  225,  226,  226,  226,  226,
		227,  227,  227,  227,  228,  228,  228,  228,
		229,  229,  229,  229,  230,  230,  230,  230,
		231,  231,  231,  231,  232,  232,  232,  232,
		233,  233,  233,  233,  234,  234,  234,  234,
		235,  235,  235,  235,  236,  236,  236,  236,
		237,  237,  237,  237,  238,  238,  238,  238,
		239,  239,  239,  239,  240,  240,  240,  240,
		240,  241,  241,  241,  241,  242,  242,  242,
		242,  243,  243,  243,  243,  244,  244,  244,
		244,  244,  245,  245,  245,  245,  246,  246,
		246,  246,  247,  247,  247,  247,  247,  248,
		248,  248,  248,  249,  249,  249,  249,  249,
		250,  250,  250,  250,  251,  251,  251,  251,
		251,  252,  252,  252,  252,  253,  253,  253,
		253,  253,  254,  254,  254,  254,  255,  255,
	},
};
#endif

static tegra_dc_bl_output dsi_otm1283a_720p_max8831_bl_response_curve = {
	0, 1, 3, 5, 7, 9, 11, 13,
	15, 17, 19, 21, 22, 23, 25, 26,
	28, 29, 30, 32, 33, 34, 36, 37,
	39, 40, 42, 43, 45, 46, 48, 49,
	50, 51, 52, 53, 54, 55, 56, 57,
	58, 59, 60, 61, 62, 63, 64, 65,
	66, 67, 68, 70, 71, 72, 73, 74,
	75, 77, 78, 79, 80, 81, 82, 83,
	84, 85, 86, 87, 88, 89, 90, 91,
	92, 93, 94, 95, 96, 97, 98, 99,
	100, 101, 101, 102, 102, 103, 103, 104,
	105, 105, 106, 107, 108, 108, 109, 110,
	111, 112, 113, 114, 115, 116, 117, 118,
	119, 120, 121, 121, 122, 123, 124, 125,
	126, 127, 128, 129, 130, 131, 132, 133,
	134, 135, 135, 136, 137, 138, 139, 140,
	141, 142, 143, 144, 145, 146, 147, 148,
	149, 150, 151, 152, 153, 154, 155, 156,
	156, 157, 158, 159, 160, 161, 162, 162,
	163, 163, 164, 164, 165, 165, 166, 167,
	167, 168, 169, 170, 171, 172, 173, 173,
	174, 175, 176, 177, 178, 179, 180, 181,
	182, 183, 184, 185, 186, 187, 188, 188,
	189, 190, 191, 192, 193, 194, 194, 195,
	196, 197, 198, 199, 200, 201, 202, 203,
	204, 204, 205, 206, 206, 207, 207, 208,
	209, 209, 210, 211, 212, 213, 214, 215,
	216, 217, 218, 219, 220, 221, 222, 223,
	223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 242, 243, 244, 245, 246,
	247, 247, 248, 250, 251, 252, 253, 255
};

static tegra_dc_bl_output dsi_otm1283a_720p_lm3528_bl_response_curve = {
	1, 33, 61, 77, 88, 97, 105, 111,
	116, 121, 125, 129, 132, 136, 139, 141,
	144, 146, 149, 151, 153, 155, 157, 158,
	160, 162, 163, 165, 166, 168, 169, 170,
	172, 173, 174, 175, 176, 177, 178, 180,
	181, 182, 182, 183, 184, 185, 186, 187,
	188, 189, 189, 190, 191, 192, 193, 193,
	194, 195, 195, 196, 197, 197, 198, 199,
	199, 200, 201, 201, 202, 202, 203, 203,
	204, 205, 205, 206, 206, 207, 207, 208,
	208, 209, 209, 210, 210, 211, 211, 212,
	212, 212, 213, 213, 214, 214, 215, 215,
	216, 216, 216, 217, 217, 218, 218, 218,
	219, 219, 219, 220, 220, 221, 221, 221,
	222, 222, 222, 223, 223, 223, 224, 224,
	224, 225, 225, 225, 226, 226, 226, 227,
	227, 227, 228, 228, 228, 229, 229, 229,
	229, 230, 230, 230, 231, 231, 231, 231,
	232, 232, 232, 233, 233, 233, 233, 234,
	234, 234, 234, 235, 235, 235, 235, 236,
	236, 236, 236, 237, 237, 237, 237, 238,
	238, 238, 238, 239, 239, 239, 239, 240,
	240, 240, 240, 240, 241, 241, 241, 241,
	242, 242, 242, 242, 242, 243, 243, 243,
	243, 243, 244, 244, 244, 244, 244, 245,
	245, 245, 245, 245, 246, 246, 246, 246,
	246, 247, 247, 247, 247, 247, 248, 248,
	248, 248, 248, 248, 249, 249, 249, 249,
	249, 250, 250, 250, 250, 250, 250, 251,
	251, 251, 251, 251, 251, 252, 252, 252,
	252, 252, 252, 253, 253, 253, 253, 253,
	253, 254, 254, 254, 254, 254, 254, 255
};

static tegra_dc_bl_output temp_bl_output_measured = {
	0, 1, 2, 4, 5, 6, 8, 9,
	10, 12, 13, 14, 15, 16, 16, 17,
	18, 19, 20, 20, 21, 22, 24, 25,
	26, 27, 28, 29, 30, 31, 33, 34,
	35, 36, 37, 38, 39, 41, 42, 43,
	44, 45, 46, 46, 47, 48, 49, 50,
	50, 51, 52, 53, 53, 54, 55, 55,
	56, 57, 57, 58, 58, 59, 60, 61,
	62, 63, 64, 65, 65, 66, 67, 68,
	68, 69, 70, 70, 71, 72, 73, 73,
	74, 75, 76, 77, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 87,
	88, 89, 90, 91, 92, 93, 94, 94,
	95, 95, 96, 97, 97, 98, 99, 99,
	100, 101, 101, 102, 103, 103, 104, 105,
	105, 106, 107, 108, 108, 109, 110, 111,
	111, 112, 113, 114, 115, 115, 116, 117,
	118, 119, 120, 121, 121, 122, 123, 124,
	125, 126, 126, 127, 128, 129, 130, 131,
	132, 133, 134, 134, 135, 136, 137, 138,
	139, 140, 141, 143, 144, 145, 146, 147,
	148, 149, 151, 152, 153, 154, 155, 156,
	157, 158, 159, 160, 161, 163, 164, 165,
	166, 167, 169, 170, 171, 172, 173, 175,
	176, 177, 179, 180, 182, 183, 185, 186,
	187, 189, 190, 191, 193, 194, 195, 197,
	198, 199, 200, 202, 203, 204, 205, 207,
	208, 209, 210, 212, 213, 214, 215, 216,
	218, 219, 220, 221, 222, 223, 225, 226,
	227, 228, 229, 231, 232, 233, 234, 235,
	237, 238, 239, 241, 242, 244, 245, 246,
	248, 249, 250, 251, 252, 253, 254, 255,
};

static p_tegra_dc_bl_output bl_output;

static p_tegra_dc_bl_output dsi_otm1283a_720p_bl_response_curve;

static int __maybe_unused dsi_otm1283a_720p_bl_notify(struct device *unused,
							int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = dsi_otm1283a_720p_bl_response_curve[brightness];

	return brightness;
}
static bool __maybe_unused dsi_otm1283a_720p_check_bl_power(void)
{
	return is_bl_powered;
}

/*
	LG uses I2C max8831 blacklight device
*/
static struct led_info dsi_otm1283a_720p_max8831_leds[] = {
	[MAX8831_ID_LED3] = {
		.name = "max8831:red:pluto",
	},
	[MAX8831_ID_LED4] = {
		.name = "max8831:green:pluto",
	},
	[MAX8831_ID_LED5] = {
		.name = "max8831:blue:pluto",
	},
};

static struct platform_max8831_backlight_data dsi_otm1283a_720p_max8831_bl_data = {
	.id	= -1,
	.name	= "pluto_display_bl",
	.max_brightness	= MAX8831_BL_LEDS_MAX_CURR,
	.dft_brightness	= 100,
	.notify	= dsi_otm1283a_720p_bl_notify,
	.is_powered = dsi_otm1283a_720p_check_bl_power,
};

static struct max8831_subdev_info dsi_otm1283a_720p_max8831_subdevs[] = {
	{
		.id = MAX8831_ID_LED3,
		.name = "max8831_led_bl",
		.platform_data = &dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED3],
		.pdata_size = sizeof(
				dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED3]),
	}, {
		.id = MAX8831_ID_LED4,
		.name = "max8831_led_bl",
		.platform_data = &dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED4],
		.pdata_size = sizeof(
				dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED4]),
	}, {
		.id = MAX8831_ID_LED5,
		.name = "max8831_led_bl",
		.platform_data = &dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED5],
		.pdata_size = sizeof(
				dsi_otm1283a_720p_max8831_leds[MAX8831_ID_LED5]),
	}, {
		.id = MAX8831_BL_LEDS,
		.name = "max8831_display_bl",
		.platform_data = &dsi_otm1283a_720p_max8831_bl_data,
		.pdata_size = sizeof(dsi_otm1283a_720p_max8831_bl_data),
	},
};

static struct max8831_platform_data dsi_otm1283a_720p_max8831 = {
	.num_subdevs = ARRAY_SIZE(dsi_otm1283a_720p_max8831_subdevs),
	.subdevs = dsi_otm1283a_720p_max8831_subdevs,
};

static struct i2c_board_info dsi_otm1283a_720p_i2c_led_info = {
	.type		= "max8831",
	.addr		= 0x4d,
	.platform_data	= &dsi_otm1283a_720p_max8831,
};

static struct lm3528_platform_data lm3528_pdata = {
	.dft_brightness	= 100,
	.is_powered = dsi_otm1283a_720p_check_bl_power,
	.notify	= dsi_otm1283a_720p_bl_notify,
};

static struct i2c_board_info lm3528_dsi_otm1283a_720p_i2c_led_info = {
	.type		= "lm3528_display_bl",
	.addr		= 0x36,
	.platform_data	= &lm3528_pdata,
};

static unsigned int dsi_l_pluto_edp_states[] = {
	1130, 1017, 904, 791, 678, 565, 452, 339, 226, 113, 0
};
static unsigned int dsi_l_pluto_edp_brightness[] = {
	255, 230, 204, 179, 153, 128, 102, 77, 51, 26, 0
};
static unsigned int dsi_l_ceres_edp_states[] = {
	720, 644, 523, 490, 442, 427, 395, 363, 330, 299, 0
};
static unsigned int dsi_l_ceres_edp_brightness[] = {
	255, 230, 204, 170, 140, 128, 102, 77, 51, 26, 0
};
static unsigned int dsi_l_atlantis_edp_states[] = {
	720, 644, 523, 490, 442, 427, 395, 363, 330, 299, 0
};
static unsigned int dsi_l_atlantis_edp_brightness[] = {
	255, 230, 204, 170, 140, 128, 102, 77, 51, 26, 0
};

#if 1
static int otm1283a_backlight_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage, 8-bit value. */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = bl_output[brightness];

	return brightness;
}

static int __maybe_unused otm1283a_check_fb(struct device *dev,
					     struct fb_info *info)
{
	return info->device == &disp_device->dev;
}


static struct platform_pwm_backlight_data external_pwm_disp1_backlight_data = {
	.pwm_id		= 0,
	.max_brightness = 255,
//	.dft_brightness = 77,
//	.pwm_period_ns  = 1000000,
	.dft_brightness	= 224,
	.pwm_period_ns	= 100000,	
	.notify		= otm1283a_backlight_notify,
	.pwm_gpio	= TEGRA_GPIO_PG2,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= otm1283a_check_fb,
};

static struct platform_device external_pwm_disp1_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &external_pwm_disp1_backlight_data,
	},
};

static struct platform_device *otm1283a_bl_devices[]  = {
	&external_pwm_disp1_backlight_device,
};

#endif
	
/*
 * In case which_pwm is TEGRA_PWM_PM0,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW0: set LCD_CS1_N pin to SFIO
 * In case which_pwm is TEGRA_PWM_PM1,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW1: set LCD_M1 pin to SFIO
 */
#if 0
static struct platform_tegra_pwm_backlight_data otm1283a_disp1_backlight_data = {
	.which_dc = 0,
	.which_pwm = TEGRA_PWM_PM0,
	.max_brightness	= 256,
	.dft_brightness	= 77,
	.gpio_conf_to_sfio	= TEGRA_GPIO_PW0,		//Ivan FIXME
	.period	= 0x1F,
	.clk_div = 3,
	.clk_select = 2,
};

static struct platform_device otm1283a_disp1_backlight_device = {
	.name	= "tegra-pwm-bl",
	.id	= -1,
	.dev	= {
		.platform_data = &otm1283a_disp1_backlight_data,
	},
};

static struct platform_device *otm1283a_trgra_pwm_bl_devices[] __initdata = {
	&otm1283a_disp1_backlight_device,
};
#endif



static struct platform_device *otm1283a_gfx_devices[] __initdata = {
	&tegra_pwfm0_device,
};

static int __init dsi_otm1283a_720p_register_bl_dev(void)
{
	struct i2c_board_info *bl_info;
	struct board_info board_info;
#ifdef TINNO_PHONE_CONFIG	
	dsi_otm1283a_720p_max8831_bl_data.edp_states =
		dsi_l_ceres_edp_states;
	dsi_otm1283a_720p_max8831_bl_data.edp_brightness =
			dsi_l_ceres_edp_brightness;
		dsi_otm1283a_720p_bl_response_curve =
				dsi_otm1283a_720p_max8831_bl_response_curve;	
	bl_output = temp_bl_output_measured;

	platform_add_devices(otm1283a_gfx_devices,
		ARRAY_SIZE(otm1283a_gfx_devices));
		
	platform_add_devices(otm1283a_bl_devices,
			ARRAY_SIZE(otm1283a_bl_devices));

/*
	platform_add_devices(otm1283a_trgra_pwm_bl_devices,
		ARRAY_SIZE(otm1283a_trgra_pwm_bl_devices));		
*/
	return 0;	
#endif
			
	tegra_get_board_info(&board_info);

	switch (board_info.board_id) {
	case BOARD_E1670: /* Atlantis ERS */
	case BOARD_E1671: /* Atlantis POP Socket */
		lm3528_pdata.edp_states = dsi_l_atlantis_edp_states;
		lm3528_pdata.edp_brightness = dsi_l_atlantis_edp_brightness;
		bl_info = &lm3528_dsi_otm1283a_720p_i2c_led_info;
		dsi_otm1283a_720p_bl_response_curve =
			dsi_otm1283a_720p_lm3528_bl_response_curve;
		break;
	case BOARD_E1680: /* Ceres ERS */
	case BOARD_E1681: /* Ceres DSC Socket */
		dsi_otm1283a_720p_max8831_bl_data.edp_states =
			dsi_l_ceres_edp_states;
		dsi_otm1283a_720p_max8831_bl_data.edp_brightness =
				dsi_l_ceres_edp_brightness;
		bl_info = &dsi_otm1283a_720p_i2c_led_info;
		dsi_otm1283a_720p_bl_response_curve =
				dsi_otm1283a_720p_max8831_bl_response_curve;
		break;
	case BOARD_E1580: /* Pluto */
	/* fall through */
	default:
		dsi_otm1283a_720p_max8831_bl_data.edp_states =
			dsi_l_pluto_edp_states;
		dsi_otm1283a_720p_max8831_bl_data.edp_brightness =
				dsi_l_pluto_edp_brightness;
		bl_info = &dsi_otm1283a_720p_i2c_led_info;
		dsi_otm1283a_720p_bl_response_curve =
				dsi_otm1283a_720p_max8831_bl_response_curve;
		break;
	}

	return i2c_register_board_info(1, bl_info, 1);
}

//1366 (V) 1062 (H) 65281140
struct tegra_dc_mode dsi_otm1283a_720p_modes[] = {
	{
		.pclk = 67900000,//86750000,//74180000,
//		.pclk = 66700000,
		.h_ref_to_sync = 2,
		.v_ref_to_sync = 2,
		.h_sync_width = 6,//20,
		.v_sync_width =  10,// 4,
		.h_back_porch = 50,//160,
		.v_back_porch = 40,
		.h_active = 720,
		.v_active = 1280,
		.h_front_porch = 50,//160,
		.v_front_porch = 40,
	},
};
static int dsi_otm1283a_720p_reg_get(void)
{
	int err = 0;

	if (dsi_otm1283a_720p_reg_requested)
		return 0;

	avdd_lcd_3v0_2v8 = regulator_get(NULL, "avdd_lcd");

	if (IS_ERR_OR_NULL(avdd_lcd_3v0_2v8)) {
		printk("avdd_lcd regulator get failed\n");
		err = PTR_ERR(avdd_lcd_3v0_2v8);
		avdd_lcd_3v0_2v8 = NULL;
		goto fail;
	}

	vdd_lcd_s_1v8 = regulator_get(NULL, "vdd_lcd_1v8_s");
	if (IS_ERR_OR_NULL(vdd_lcd_s_1v8)) {
		printk("vdd_lcd_1v8_s regulator get failed\n");
		err = PTR_ERR(vdd_lcd_s_1v8);
		vdd_lcd_s_1v8 = NULL;
		goto fail;
	}

	vdd_sys_bl_3v7 = regulator_get(NULL, "vdd_sys_bl");
	if (IS_ERR_OR_NULL(vdd_sys_bl_3v7)) {
		printk("vdd_sys_bl regulator get failed\n");
		err = PTR_ERR(vdd_sys_bl_3v7);
		vdd_sys_bl_3v7 = NULL;
		goto fail;
	}

	dsi_otm1283a_720p_reg_requested = true;
	return 0;
fail:
	return err;
}

static struct tegra_dsi_out dsi_otm1283a_720p_pdata;
static int dsi_otm1283a_720p_gpio_get(void)
{
	int err = 0;

	if (dsi_otm1283a_720p_gpio_requested)
		return 0;

	err = gpio_request(dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio,
		"panel rst");
	if (err < 0) {
		printk("panel reset gpio request failed\n");
//Ivan		goto fail;
	}
	printk("Ivan otm1283a dsi_panel_rst_gpio = %d \n", dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio);

	err = gpio_request(dsi_otm1283a_720p_pdata.dsi_panel_bl_en_gpio,
		"panel backlight");
	if (err < 0) {
		printk("panel backlight gpio request failed\n");
//Ivan		goto fail;
	}
	printk("Ivan otm1283a dsi_panel_bl_en_gpio = %d \n", dsi_otm1283a_720p_pdata.dsi_panel_bl_en_gpio);
	
/*
	err = gpio_request(dsi_otm1283a_720p_pdata.dsi_panel_bl_pwm_gpio,
		"panel pwm");
	if (err < 0) {
		printk("panel backlight pwm gpio request failed\n");

//Ivan		goto fail;
	}
*/
	dsi_otm1283a_720p_gpio_requested = true;
	return 0;
//fail:
//	return err;
}

static int dsi_otm1283a_720p_enable(struct device *dev)
{
	int err = 0;

	printk("Ivan dsi_otm1283a_720p_enable\n");

	err = dsi_otm1283a_720p_reg_get();
	if (err < 0) {
		printk("dsi regulator get failed\n");
		goto fail;
	}

#if 0
	err = tegra_panel_gpio_get_dt("otm,720p-5", &panel_of);
	if (err < 0) {
		/* try to request gpios from board file */
		err = dsi_otm1283a_720p_gpio_get();
		if (err < 0) {
			printk("dsi gpio request failed\n");
			goto fail;
		}
	}
#endif

	err = dsi_otm1283a_720p_gpio_get();
	if (err < 0) {
		printk("dsi gpio request failed\n");
//		goto fail;
	}
		
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_RESET], 0);
	else
	{
		gpio_direction_output(
			dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 0);
	    printk("Ivan dsi_otm1283a_720p_enable 1\n");
	}
	
	if (avdd_lcd_3v0_2v8) {
		err = regulator_enable(avdd_lcd_3v0_2v8);
		if (err < 0) {
			printk("avdd_lcd regulator enable failed\n");
			goto fail;
		}
		regulator_set_voltage(avdd_lcd_3v0_2v8, 2800000, 2800000);
	    printk("Ivan dsi_otm1283a_720p_enable 2\n");
		
	}

	usleep_range(3000, 5000);

	if (vdd_lcd_s_1v8) {
		err = regulator_enable(vdd_lcd_s_1v8);
		if (err < 0) {
			printk("vdd_lcd_1v8_s regulator enable failed\n");
			goto fail;
		}
	    printk("Ivan dsi_otm1283a_720p_enable 3\n");		
	}
	usleep_range(3000, 5000);

	if (vdd_sys_bl_3v7) {
		err = regulator_enable(vdd_sys_bl_3v7);
		if (err < 0) {
			printk("vdd_sys_bl regulator enable failed\n");
			goto fail;
		}
	    printk("Ivan dsi_otm1283a_720p_enable 4\n");		
	}
	usleep_range(3000, 5000);

#if 0
	err = tegra_panel_reset(&panel_of, 20);
	if (err < 0) {
		/* use platform data */
		gpio_set_value(dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 1);
		usleep_range(1000, 5000);
		gpio_set_value(dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 0);
		usleep_range(1000, 5000);
		gpio_set_value(dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 1);
		msleep(20);
	}
#endif
	gpio_direction_output(
		dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 1);
	usleep_range(1000, 5000);

	gpio_direction_output(
		dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 0);	
	usleep_range(1000, 5000);

	gpio_direction_output(
		dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 1);	
	msleep(20);
	
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE], 1);
	else
		gpio_direction_output(
			dsi_otm1283a_720p_pdata.dsi_panel_bl_en_gpio, 1);

	is_bl_powered = true;
	return 0;
fail:
	return err;
}

#if 0
static u8 panel_dsi_config[] = {0xe0, 0x43, 0x0, 0x80, 0x0, 0x0};
static u8 panel_disp_ctrl1[] = {0xb5, 0x34, 0x20, 0x40, 0x0, 0x20};
static u8 panel_disp_ctrl2[] = {0xb6, 0x04, 0x74, 0x0f, 0x16, 0x13};
static u8 panel_internal_clk[] = {0xc0, 0x01, 0x08};
static u8 panel_pwr_ctrl3[] = {
	0xc3, 0x0, 0x09, 0x10, 0x02, 0x0, 0x66, 0x20, 0x13, 0x0};
static u8 panel_pwr_ctrl4[] = {0xc4, 0x23, 0x24, 0x17, 0x17, 0x59};
static u8 panel_positive_gamma_red[] = {
	0xd0, 0x21, 0x13, 0x67, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_red[] = {
	0xd1, 0x32, 0x13, 0x66, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_green[] = {
	0xd2, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_green[] = {
	0xd3, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_positive_gamma_blue[] =	{
	0xd4, 0x41, 0x14, 0x56, 0x37, 0x0c, 0x06, 0x62, 0x23, 0x03};
static u8 panel_negetive_gamma_blue[] =	{
	0xd5, 0x52, 0x14, 0x55, 0x37, 0x02, 0x06, 0x62, 0x23, 0x03};
static u8 panel_ce2[] = {0x71, 0x0, 0x0, 0x01, 0x01};
static u8 panel_ce3[] = {0x72, 0x01, 0x0e};
static u8 panel_ce4[] = {0x73, 0x34, 0x52, 0x0};
static u8 panel_ce5[] = {0x74, 0x05, 0x0, 0x06};
static u8 panel_ce6[] = {0x75, 0x03, 0x0, 0x07};
static u8 panel_ce7[] = {0x76, 0x07, 0x0, 0x06};
static u8 panel_ce8[] = {0x77, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f};
static u8 panel_ce9[] = {0x78, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce10[] = {
	0x79, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40};
static u8 panel_ce11[] = {0x7a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce12[] = {0x7b, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
static u8 panel_ce13[] = {0x7c, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
#endif

//Ivan added
       //Payload[D4 32] 
//Ivan static u8 s_ParaEMI[] = {0xd4, 0x32};
	//PacketHeader[05 11 00 xx] // Sleep Out 
//Ivan static u8 s_ParaSleepOut[] = {0x11};
       //PacketHeader[05 29 00 xx] // Display On 
//Ivan static u8 s_ParaDisplayOn[] = {0x29};

static u8 s_Para1[] = {0xff,0x12,0x83,0x01};
static u8 s_Para2[] = {0xff,0x12,0x83};
static u8 s_Para3[] = {0xC1,0x02};
static u8 s_Para4[] = {0xC0,0x00,0x64,0x00,0x0F,0x11,0x00,0x64,0x0F,0x11};
static u8 s_Para5[] = {0xc0,0x00,0x55,0x00,0x01,0x00,0x04};
static u8 s_Para6[] = {0xc0,0x00};
static u8 s_Para7[] = {0xc4,0x18};
static u8 s_Para8[] = {0xc0,0x00,0x55};
static u8 s_Para9[] = {0xc1,0x66};
static u8 s_Para10[] = {0xc4,0x84};
static u8 s_Para11[] = {0xc4,0x02};
static u8 s_Para12[] = {0xc4,0x49};
static u8 s_Para13[] = {0xb0,0x51};
static u8 s_Para14[] = {0xb0,0x03};
static u8 s_Para15[] = {0xf5,0x02,0x11,0x02,0x11};
static u8 s_Para16[] = {0xc5,0x50};
static u8 s_Para17[] = {0xc5,0x66};
static u8 s_Para18[] = {0xf5,0x00,0x00};
static u8 s_Para19[] = {0xf5,0x00,0x00};
static u8 s_Para20[] = {0xf5,0x00,0x00};
static u8 s_Para21[] = {0xf5,0x02};
static u8 s_Para22[] = {0xf5,0x03};
static u8 s_Para23[] = {0xc5,0xc0};
static u8 s_Para24[] = {0xc4,0x05,0x10,0x04,0x02,0x05,0x15,0x11,0x05,0x10,0x07,0x02,0x05,0x15,0x11};
static u8 s_Para25[] = {0xc4,0x66,0x66};
static u8 s_Para26[] = {0xc5,0x37,0x52};
static u8 s_Para27[] = {0xc5,0x04,0x58};



static u8 s_Para28[] = {0xc5,0x03,0xE8,0x40,0x03,0xE8,0x40};
static u8 s_Para29[] = {0xc5,0x80};
static u8 s_Para30[] = {0xd0,0x40};
static u8 s_Para31[] = {0xd1,0x00,0x00};
static u8 s_Para32[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_Para33[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0x00};
static u8 s_Para34[] = {0xcb,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_Para35[] = {0xcb,0x00,0x00,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00};
static u8 s_Para36[] = {0xcb,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x05,0x05};
static u8 s_Para37[] = {0xcb,0x05,0x05,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_Para38[] = {0xcb,0x00,0x00,0x00,0x05,0x00,0x05,0x05,0x05,0x05,0x05,0x00,0x00,0x00,0x00};
static u8 s_Para39[] = {0xcb,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static u8 s_Para40[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x00,0x10,0x0E};
static u8 s_Para41[] = {0xcc,0x0C,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static u8 s_Para42[] = {0xcc,0x00,0x00,0x00,0x09,0x00,0x0F,0x0D,0x0B,0x01,0x03,0x00,0x00,0x00,0x00};
static u8 s_Para43[] = {0xcc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,0x00,0x10,0x0E};
static u8 s_Para44[] = {0xcc,0x0C,0x02,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

static u8 s_Para45[] = {0xcc,0x00,0x00,0x00,0x09,0x00,0x0F,0x0D,0x0B,0x01,0x03,0x00,0x00,0x00,0x00};
static u8 s_Para46[] = {0xce,0x87,0x03,0x06,0x86,0x03,0x06,0x85,0x03,0x06,0x84,0x03,0x06};
static u8 s_Para47[] = {0xce,0xF0,0x00,0x00,0xF0,0x00,0x00,0xF0,0x00,0x00,0xF0,0x00,0x00,0x00,0x00};
static u8 s_Para48[] = {0xce,0x38,0x05,0x84,0xFE,0x00,0x06,0x00,0x38,0x04,0x84,0xFF,0x00,0x06,0x00};
static u8 s_Para49[] = {0xce,0x38,0x03,0x85,0x00,0x00,0x06,0x00,0x38,0x02,0x85,0x01,0x00,0x06,0x00};
static u8 s_Para50[] = {0xce,0x38,0x01,0x85,0x02,0x00,0x06,0x00,0x38,0x00,0x85,0x03,0x00,0x06,0x00};
static u8 s_Para51[] = {0xce,0x30,0x00,0x85,0x04,0x00,0x06,0x00,0x30,0x01,0x85,0x05,0x00,0x06,0x00};
static u8 s_Para52[] = {0xcf,0x70,0x00,0x00,0x10,0x00,0x00,0x00,0x70,0x00,0x00,0x10,0x00,0x00,0x00};
static u8 s_Para53[] = {0xcf,0x70,0x00,0x00,0x10,0x00,0x00,0x00,0x70,0x00,0x00,0x10,0x00,0x00,0x00};
static u8 s_Para54[] = {0xcf,0x70,0x00,0x00,0x10,0x00,0x00,0x00,0x70,0x00,0x00,0x10,0x00,0x00,0x00};
static u8 s_Para55[] = {0xcf,0x70,0x00,0x00,0x10,0x00,0x00,0x00,0x70,0x00,0x00,0x10,0x00,0x00,0x00};
static u8 s_Para56[] = {0xcf,0x01,0x01,0x20,0x20,0x00,0x00,0x01,0x80,0x01,0x03,0x08};
static u8 s_Para57[] = {0xd8,0xAC,0xAC};
static u8 s_Para58[] = {0xd9,0xA2};
static u8 s_Para59[] = {0xe1,0x04,0x09,0x0C,0x0C,0x05,0x0D,0x0A,0x08,0x05,0x08,0x0E,0x07,0x0E,0x16,0x12,0x0A};
static u8 s_Para60[] = {0xe2,0x04,0x08,0x0D,0x0D,0x04,0x0D,0x09,0x09,0x05,0x08,0x0E,0x07,0x0E,0x16,0x11,0x0A};
static u8 s_Para61[] = {0xc1,0xF0};
static u8 s_Para62[] = {0xc4,0x81};
static u8 s_Para63[] = {0xc1,0x90};
static u8 s_Para64[] = {0xc4,0x81};
static u8 s_Para65[] = {0xc4,0x80};
static u8 s_Para66[] = {0xc4,0x30};
static u8 s_Para67[] = {0xc4,0x40};
static u8 s_Para68[] = {0xff,0xFF,0xFF,0xFF};
//static u8 s_Para69[] = {0xc4,0x40};



//static u8 s_Para70[] = {0x35, 0x00};
//static u8 s_ParaNormalOn[] = {0x13};
//static u8 s_ParaSleepOut[] = {0x11};
//static u8 s_ParaDisplayOn[] = {0x29};


static struct tegra_dsi_cmd dsi_otm1283a_720p_init_cmd[] = {
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),    
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para1),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para2),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para3),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para4),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para5),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa4),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para6),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x87),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para7),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb3),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para8),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x81),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para9),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x81),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para10),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x82),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para11),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para12),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb9),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para13),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xc6),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para14),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para15),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),	
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para16),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x94),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para17),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb2),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para18),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb4),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para19),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb6),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para20),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x94),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para21),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb4),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para22),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb4),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para23),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para24),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para25),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x91),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para26),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para27),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb5),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para28),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xbb),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para29),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para30),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para31),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para32),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para33),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para34),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para35),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xc0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para36),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xd0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para37),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xe0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para38),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xf0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para39),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para40),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para41),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para42),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para43),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xc0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para44),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xd0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para45),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para46),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para47),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para48),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para49),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xc0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para50),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xd0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para51),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para52),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x90),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para53),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para54),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para55),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xc0),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para56),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para57),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para58),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para59),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para60),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xa4),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para61),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb2),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para62),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x93),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para63),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb2),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para64),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0xb5),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para65),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x80),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para66),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x8b),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para67),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x00),
	
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para68),
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x00, 0x8b),	
	//DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, s_Para69),
	//DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x35, 0x00),
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, 0x13, 0x0),	
	DSI_DLY_MS(20),	
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(150),	
	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
	DSI_DLY_MS(10),	
#if 0
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_dsi_config),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl1),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_disp_ctrl2),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_internal_clk),

	/*  panel power control 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc1, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_pwr_ctrl4),

	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_red),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_green),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_positive_gamma_blue),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_negetive_gamma_blue),

	DSI_CMD_SHORT(DSI_DCS_WRITE_1_PARAM, DSI_DCS_SET_ADDR_MODE, 0x08),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x0),

	/* panel CE 1 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0x70, 0x0),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce2),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce3),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce4),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce5),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce6),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce7),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce8),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce9),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce10),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce11),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce12),
	DSI_CMD_LONG(DSI_GENERIC_LONG_WRITE, panel_ce13),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x02),
	DSI_DLY_MS(15),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x06),
	DSI_DLY_MS(15),

	/* panel power control 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xc2, 0x4e),
	DSI_DLY_MS(85),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_EXIT_SLEEP_MODE, 0x0),
	DSI_DLY_MS(15),

	/* panel OTP 2 */
	DSI_CMD_SHORT(DSI_GENERIC_SHORT_WRITE_2_PARAMS, 0xf9, 0x80),
	DSI_DLY_MS(15),

	DSI_CMD_SHORT(DSI_DCS_WRITE_0_PARAM, DSI_DCS_SET_DISPLAY_ON, 0x0),
#endif
};

static struct tegra_dsi_out dsi_otm1283a_720p_pdata = {
	.n_data_lanes = 4,

	.refresh_rate = 60,
	.video_data_type = TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE,
	.video_clock_mode = TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS,
//	.video_burst_mode = TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED,
//	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END,
	.video_burst_mode = TEGRA_DSI_VIDEO_NONE_BURST_MODE,	
	.controller_vs = DSI_VS_1,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.dsi_init_cmd = dsi_otm1283a_720p_init_cmd,
	.n_init_cmd = ARRAY_SIZE(dsi_otm1283a_720p_init_cmd),
};

static int dsi_otm1283a_720p_disable(void)
{
	printk("Ivan dsi_otm1283a_720p_disable\n");
    
	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE]))
	{
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_BL_ENABLE], 0);
	    printk("Ivan dsi_otm1283a_720p_disable 1 \n");
		
	}
	else
	{
		gpio_direction_output(
			dsi_otm1283a_720p_pdata.dsi_panel_bl_en_gpio, 0);
	    printk("Ivan dsi_otm1283a_720p_disable 2 \n");
		
	}
	is_bl_powered = false;

	if (gpio_is_valid(panel_of.panel_gpio[TEGRA_GPIO_RESET]))
		gpio_direction_output(
			panel_of.panel_gpio[TEGRA_GPIO_RESET], 0);
	else
		gpio_direction_output(
			dsi_otm1283a_720p_pdata.dsi_panel_rst_gpio, 0);

	if (vdd_sys_bl_3v7)
		regulator_disable(vdd_sys_bl_3v7);

	if (vdd_lcd_s_1v8)
		regulator_disable(vdd_lcd_s_1v8);

	if (avdd_lcd_3v0_2v8)
		regulator_disable(avdd_lcd_3v0_2v8);

	return 0;
}

static void dsi_otm1283a_720p_dc_out_init(struct tegra_dc_out *dc)
{
	dc->dsi = &dsi_otm1283a_720p_pdata;
	dc->parent_clk = "pll_d_out0";
	dc->modes = dsi_otm1283a_720p_modes;
	dc->n_modes = ARRAY_SIZE(dsi_otm1283a_720p_modes);
	dc->enable = dsi_otm1283a_720p_enable;
	dc->disable = dsi_otm1283a_720p_disable;
	dc->width = 62;
	dc->height = 110;
	dc->flags = DC_CTRL_MODE;
}
static void dsi_otm1283a_720p_fb_data_init(struct tegra_fb_data *fb)
{
	fb->xres = dsi_otm1283a_720p_modes[0].h_active;
	fb->yres = dsi_otm1283a_720p_modes[0].v_active;
}

static void dsi_otm1283a_720p_sd_settings_init(struct tegra_dc_sd_settings *settings)
{
	struct board_info bi;
	tegra_get_display_board_info(&bi);
#ifdef TINNO_PHONE_CONFIG
	settings->bl_device_name = "pwm-backlight";

#else
	if (bi.board_id == BOARD_E1563)
		settings->bl_device_name = "lm3528_display_bl";
	else
		settings->bl_device_name = "max8831_display_bl";
#endif
}

static void dsi_otm1283a_720p_set_disp_device(
	struct platform_device *ceres_display_device)
{
	disp_device = ceres_display_device;
}

#ifdef CONFIG_TEGRA_DC_CMU
static void dsi_otm1283a_720p_cmu_init(struct tegra_dc_platform_data *pdata)
{
	pdata->cmu = &dsi_otm1283a_720p_cmu;
}
#endif

struct tegra_panel __initdata dsi_otm1283a_720p = {
	.init_sd_settings = dsi_otm1283a_720p_sd_settings_init,
	.init_dc_out = dsi_otm1283a_720p_dc_out_init,
	.init_fb_data = dsi_otm1283a_720p_fb_data_init,
	.register_bl_dev = dsi_otm1283a_720p_register_bl_dev,
	.set_disp_device = dsi_otm1283a_720p_set_disp_device,	
#ifdef CONFIG_TEGRA_DC_CMU
	.init_cmu_data = dsi_otm1283a_720p_cmu_init,
#endif
};
EXPORT_SYMBOL(dsi_otm1283a_720p);
