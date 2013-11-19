/*
 * leds-max77660.c -- MAXIM MAX77660 led driver.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/pm.h>
#include <linux/mfd/max77660/max77660-core.h>

//static int gOn_time;
struct max77660_leds {
	struct device		*dev;
	struct device		*parent;
};

struct max77660_ledblnk_map {
	unsigned long bits_val;
	unsigned long time_ms;
};

struct max77660_ledblnk_map max77660_ledblnkp[] = {
	{ 0x0, 1000  },
	{ 0x1, 1500  },
	{ 0x2, 2000  },
	{ 0x3, 2500  },
	{ 0x4, 3000  },
	{ 0x5, 3500  },
	{ 0x6, 4000  },
	{ 0x7, 4500  },
	{ 0x8, 5000  },
	{ 0x9, 5500  },
	{ 0xA, 6000  },
	{ 0xB, 7000  },
	{ 0xC, 8000  },
	{ 0xD, 10000 },
	{ 0xE, 12000 },
	{ 0xF, 0     },
};

struct max77660_ledblnk_map max77660_ledblnkd[] = {
	{ 0x0, 0    },
	{ 0x1, 50   },
	{ 0x2, 100  },
	{ 0x3, 150  },
	{ 0x4, 200  },
	{ 0x5, 300  },
	{ 0x6, 350  },
	{ 0x7, 400  },
	{ 0x8, 450  },
	{ 0x9, 500  },
	{ 0xA, 550  },
	{ 0xB, 600  },
	{ 0xC, 700  },
	{ 0xD, 800  },
	{ 0xE, 900  },
	{ 0xF, 1000 },
};

static int max77660_add_attributes(struct device *dev,
				 struct device_attribute *attrs)
{
	int error = 0;
	int i;

	if (attrs) {
		for (i = 0; attr_name(attrs[i]); i++) {
			error = device_create_file(dev, &attrs[i]);
			if (error)
				break;
		}
		if (error)
			while (--i >= 0)
				device_remove_file(dev, &attrs[i]);
	}
	return error;
}

static void max77660_remove_attributes(struct device *dev,
				     struct device_attribute *attrs)
{
	int i;

	if (attrs)
		for (i = 0; attr_name(attrs[i]); i++)
			device_remove_file(dev, &attrs[i]);
}

static int max77660_disable_leds(struct max77660_leds *leds)
{
	int ret;

	/* Disable LED driver by default */
	ret = max77660_reg_write(leds->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDEN, 0x80);
	if (ret < 0) {
		dev_err(leds->dev, "LED write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(leds->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDBLNK, 0x0);
	if (ret < 0) {
		dev_err(leds->dev, "LEDBLNK write failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static ssize_t max77660_leds_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long leds_en = 0;
	printk("Ivan max77660_leds_enable_show \n");

	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDEN, &leds_en);
	if (ret < 0) {
		dev_err(dev, "LEDEN read failed: %d\n", ret);
		return ret;
	}
	return sprintf(buf, "%u\n", (leds_en & 0xF));
}

static ssize_t max77660_leds_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	char *after;
	unsigned long leds_en = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (isspace(*after))
		count++;
	printk("Ivan max77660_leds_enable_store leds_en = %x\n",leds_en);

	if (count == size) {
	    
		ret = count;
		leds_en &= 0x0F;
		/* switch the led0 control to LED0EN bit */
		leds_en |= 0x80;
		/* enable leds */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LEDEN, leds_en);
		if (ret < 0) {
			dev_err(dev, "LEDEN write failed: %d\n", ret);
			goto out;
		}
	    printk("Ivan max77660_leds_enable_store OK!\n");
		
	} else {
		ret = -EINVAL;
		goto out;
	}

out:
	return count;
}

static ssize_t max77660_leds_brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	unsigned long val = 0;
	unsigned long leds_brt = 0;
	printk("Ivan max77660_leds_brightness_show \n");

	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LED0BRT, &val);
	if (ret < 0) {
		dev_err(dev, "LED0BRT read failed: %d\n", ret);
		return ret;
	}
	leds_brt |= (val & 0xFF) << 1;
	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LED0BRT, &val);
	if (ret < 0) {
		dev_err(dev, "LED1BRT read failed: %d\n", ret);
		return ret;
	}
	leds_brt |= (val & 0xFF) << 9;
	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LED0BRT, &val);
	if (ret < 0) {
		dev_err(dev, "LED2BRT read failed: %d\n", ret);
		return ret;
	}
	leds_brt |= (val & 0xFF) << 17;
	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LED0BRT, &val);
	if (ret < 0) {
		dev_err(dev, "LED3BRT read failed: %d\n", ret);
		return ret;
	}
	leds_brt |= (val & 0xFF) << 25;

	return sprintf(buf, "%u\n", leds_brt);
}

static ssize_t max77660_leds_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	char *after;
	unsigned long leds_brt = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;
	printk("Ivan max77660_leds_brightness_store leds_brt = %d \n",leds_brt);

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;
//Ivan led0 and led3 connect to button backlight, led1 connect to RED, led2 connect to GREEN

		/* led0 */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LED0BRT, (leds_brt & 0xFF) >> 1);
		if (ret < 0) {
			dev_err(dev, "LED0BRT write failed: %d\n", ret);
			goto out;
		}
		/* led1 */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LED1BRT, ((leds_brt >> 8) & 0xFF) >> 1);
		if (ret < 0){
			dev_err(dev, "LED1BRT write failed: %d\n", ret);
			goto out;
		}
		/* led2 */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LED2BRT, ((leds_brt >> 16) & 0xFF) >> 1);
		if (ret < 0){
			dev_err(dev, "LED2BRT write failed: %d\n", ret);
			goto out;
		}
		/* led3 */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LED3BRT, ((leds_brt >> 24) & 0xFF) >> 1);
		if (ret < 0){
			dev_err(dev, "LED3BRT write failed: %d\n", ret);
			goto out;
		}
	    printk("Ivan max77660_leds_brightness_store OK! \n");
		
	} else {
		ret = -EINVAL;
		goto out;
	}

out:
	return count;

}

static ssize_t max77660_leds_onms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int i;
	unsigned long val = 0;
	unsigned long leds_blnkd = 0;
	printk("Ivan max77660_leds_onms_show \n");

	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDBLNK, &val);
	if (ret < 0) {
		dev_err(dev, "LEDBLNK read failed: %d\n", ret);
		return ret;
	}
	leds_blnkd = (val >> 4) & 0xF;
	for (i = 0; i < ARRAY_SIZE(max77660_ledblnkd); i++) {
		if (max77660_ledblnkd[i].bits_val == leds_blnkd)
			break;
	}
	if (i >= ARRAY_SIZE(max77660_ledblnkd))
		return -EINVAL;
	else {
		return sprintf(buf, "%u\n", max77660_ledblnkd[i].time_ms);
	}
}

static ssize_t max77660_leds_onms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int i;
	char *after;
	unsigned long leds_onms = simple_strtoul(buf, &after, 10);
	unsigned long val = 0;
	size_t count = after - buf;
	printk("Ivan max77660_leds_onms_store leds_onms = %d\n",leds_onms);

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

//		gOn_time = leds_onms;

		for (i = 0; i < ARRAY_SIZE(max77660_ledblnkd); i++) {
			if (leds_onms <= max77660_ledblnkd[i].time_ms)
				break;
		}
		if (i >= ARRAY_SIZE(max77660_ledblnkd)) {
			dev_err(dev, "the time of blinking duration is too long, %d ms\n", leds_onms);
			return -EINVAL;
		}

		ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LEDBLNK, &val);
		if (ret < 0) {
			dev_err(dev, "LEDBLNK read failed: %d\n", ret);
			return ret;
		}

		val &= 0xF;
		val |= max77660_ledblnkd[i].bits_val << 4;

		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LEDBLNK, val);
		if (ret < 0) {
			dev_err(dev, "LEDBLNK write failed: %d\n", ret);
			goto out;
		}
	    printk("Ivan max77660_leds_onms_store OK!!\n");
		
	} else {
		ret = -EINVAL;
		goto out;
	}

out:
	return count;
}

static ssize_t max77660_leds_offms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	int i;
	unsigned long val = 0;
	unsigned long leds_blnk = 0;
	unsigned long offms = 0;
	printk("Ivan max77660_leds_offms_show \n");

	ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_LEDBLNK, &val);
	if (ret < 0) {
		dev_err(dev, "LEDBLNK read failed: %d\n", ret);
		return ret;
	}

	leds_blnk = val & 0xF;
	for (i = 0; i < ARRAY_SIZE(max77660_ledblnkp); i++) {
		if (max77660_ledblnkp[i].bits_val == leds_blnk)
			break;
	}
	if (i >= ARRAY_SIZE(max77660_ledblnkd))
		return -EINVAL;

	offms = max77660_ledblnkp[i].time_ms;

	leds_blnk = (val >> 4) & 0xF;
	for (i = 0; i < ARRAY_SIZE(max77660_ledblnkd); i++) {
		if (max77660_ledblnkd[i].bits_val == leds_blnk)
			break;
	}
	if (i >= ARRAY_SIZE(max77660_ledblnkd))
		return -EINVAL;

	offms -= max77660_ledblnkd[i].time_ms;

	return sprintf(buf, "%u\n", offms);
}

static ssize_t max77660_leds_offms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int i;
	char *after;
	unsigned long leds_offms = simple_strtoul(buf, &after, 10);
	unsigned long leds_blnkp_ms = 0;
	unsigned long leds_blnkd = 0;
	unsigned long val = 0;
	size_t count = after - buf;
	printk("Ivan max77660_leds_offms_store leds_offms = %d\n",leds_offms);

	if (isspace(*after))
		count++;

	if (count == size) {
		ret = count;

		/* read max77660 ledblnk register */
		ret = max77660_reg_read(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LEDBLNK, &val);
		if (ret < 0) {
			dev_err(dev, "LEDBLNK read failed: %d\n", ret);
			return ret;
		}
		leds_blnkd = (val >> 4) & 0xF;

		/* look up duration time_ms of max77660 leds */
		for (i = 0; i < ARRAY_SIZE(max77660_ledblnkd); i++) {
			if (max77660_ledblnkd[i].bits_val == leds_blnkd)
				break;
		}
		if (i >= ARRAY_SIZE(max77660_ledblnkd))
			return -EINVAL;

		/* calculate the period time_ms of max77660 leds blinking */
		leds_blnkp_ms = leds_offms + max77660_ledblnkd[i].time_ms;

		/* look up the value need setting,
		 * matching with blinking period time_ms. */
		for (i = 0; i < ARRAY_SIZE(max77660_ledblnkp); i++) {
			if (leds_blnkp_ms <= max77660_ledblnkp[i].time_ms)
				break;
		}
		if (i >= ARRAY_SIZE(max77660_ledblnkp)) {
			dev_err(dev, "the time of blinking period is too long, %d ms\n", leds_blnkp_ms);
			return -EINVAL;
		}


		val &= 0xF0;
		val |= max77660_ledblnkp[i].bits_val;
		    
		/* write max77660 ledblnk register */
		ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_LEDBLNK, val);
//Ivan added
/*		
		if (gOn_time == 0 && leds_offms == 0)
		{
		    ret = max77660_reg_write(dev->parent, MAX77660_PWR_SLAVE,
				    MAX77660_REG_LEDBLNK, 0xF0);		//Ivan always on
	    	    printk("Ivan max77660_leds_offms_store ALWAYS ON!!\n");
		}
*/
		if (ret < 0) {
			dev_err(dev, "LEDBLNK write failed: %d\n", ret);
			goto out;
		}
	    printk("Ivan max77660_leds_offms_store OK!!\n");
		
	} else {
		ret = -EINVAL;
		goto out;
	}

out:
	return count;
}

static struct device_attribute max77660_leds_attrs[] = {
	__ATTR(enable, 0644, max77660_leds_enable_show, max77660_leds_enable_store),
	__ATTR(brightness, 0644, max77660_leds_brightness_show, max77660_leds_brightness_store),
	__ATTR(onms, 0644, max77660_leds_onms_show, max77660_leds_onms_store),
	__ATTR(offms, 0644, max77660_leds_offms_show, max77660_leds_offms_store),
	__ATTR_NULL,
};

static int __devinit max77660_leds_probe(struct platform_device *pdev)
{
	struct max77660_leds *leds;
	struct max77660_platform_data *pdata;
	int ret = 0;
	
	printk("Ivan max77660_leds_probe \n");
	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -EINVAL;
	}

	leds = devm_kzalloc(&pdev->dev, sizeof(*leds), GFP_KERNEL);
	if (!leds) {
		dev_err(&pdev->dev, "Memory allocation failed for leds\n");
		return -ENOMEM;
	}

	leds->dev = &pdev->dev;
	leds->parent = pdev->dev.parent;
	dev_set_drvdata(&pdev->dev, leds);

	if (pdata->led_disable)
		ret = max77660_disable_leds(leds);

	ret = max77660_add_attributes(leds->dev, max77660_leds_attrs);
	if (ret < 0) {
		dev_err(&pdev->dev, "Create max77660 leds attributes failed, %d!\n", ret);
		return ret;
	}

	return ret;
}

static int __devexit max77660_leds_remove(struct platform_device *pdev)
{
	struct max77660_leds *leds = platform_get_drvdata(pdev);

	if (!leds)
		return 0;
	max77660_remove_attributes(&pdev->dev, max77660_leds_attrs);
	devm_kfree(&pdev->dev, leds);
	return 0;
}

static struct platform_driver max77660_leds_driver = {
	.driver = {
		.name = "max77660-leds",
		.owner = THIS_MODULE,
	},
	.probe = max77660_leds_probe,
	.remove = __devexit_p(max77660_leds_remove),
};

module_platform_driver(max77660_leds_driver);

MODULE_DESCRIPTION("max77660 LEDs driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-leds");
MODULE_LICENSE("GPL v2");
