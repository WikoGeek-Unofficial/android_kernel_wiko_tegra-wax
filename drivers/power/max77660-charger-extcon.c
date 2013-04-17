/*
 * max77660-charger-extcon.c -- MAXIM MAX77660 VBUS detection.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
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
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/extcon.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/power_supply.h>

static int max77660_chrg_wdt[] = {16, 32, 64, 128};

struct max77660_charger {
	struct max77660_charger_platform_data *pdata;
	struct device *dev;
	struct power_supply usb_psy;
	struct power_supply dc_psy;
	int irq;
	int usb_online;
	int ac_online;
	int is_100ma;
	int status;
	int in_current_lim;
	void (*update_status)(int, int);
	int wdt_timeout;
};

struct max77660_chg_extcon {
	struct device			*dev;
	struct device			*parent;
	struct extcon_dev		*edev;
	struct regulator_dev		*chg_rdev;
	struct regulator_dev		*rdev;
	struct max77660_charger		*charger;
	int				irq;
	int				wdt_irq;
	struct regulator_desc		chg_reg_desc;
	struct regulator_init_data	chg_reg_init_data;
};

struct max77660_chg_extcon *max77660_ext;

const char *max77660_excon_cable[] = {
	[0] = "USB",
	NULL,
};

static inline int fchg_current(int x)
{
	if (x >= 1620)
		return (x+60)/60;
	else if (x >= 250)
		return (x-200)/50;
	else
		return 0;
}

static int max77660_battery_detect(struct max77660_chg_extcon *chip)
{
	int ret = 0;
	u8 status;

	ret = max77660_reg_read(chip->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_DETAILS1, &status);
	if (ret < 0) {
		dev_err(chip->dev, "CHARGER_CHGINT read failed: %d\n", ret);
		return ret;
	}
	if ((status & MAX77660_BATDET_DTLS) == MAX77660_BATDET_DTLS_NO_BAT)
		return -EPERM;
	return 0;
}

static int max77660_charger_init(struct max77660_chg_extcon *chip, int enable)
{
	struct max77660_charger *charger = chip->charger;
	u8 reg_val = 0;
	int ret;
	u8 read_val;

	charger->in_current_lim = fchg_current(charger->in_current_lim);

	/* unlock charger protection */
	reg_val = MAX77660_CHGPROT_UNLOCKED<<MAX77660_CHGPROT_SHIFT;
	ret = max77660_reg_write(chip->parent, MAX77660_CHG_SLAVE,
		MAX77660_CHARGER_CHGCTRL1,
		MAX77660_CHARGER_CHGPROT_MASK |
		MAX77660_CHARGER_BUCK_EN_MASK |
		MAX77660_CHARGER_JEITA_EN_MASK);
	if (ret < 0)
		return ret;

	if (enable) {
		/* enable charger */
		/* Set DCILMT to 1A */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_DCCRNT,
				MAX77660_DCLIMIT_1A |
				MAX77660_DC_WC_CNTL_DC);
		if (ret < 0)
			return ret;

		/* Fast charge to 5 hours, fast charge current to 1.1A */
		ret = max77660_reg_update(chip->parent,
				MAX77660_CHG_SLAVE, MAX77660_CHARGER_FCHGCRNT,
				chip->charger->in_current_lim,
				MAX77660_CHGCC_MASK);
		if (ret < 0)
			return ret;

		ret = max77660_reg_read(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_FCHGCRNT, &read_val);
		if (ret < 0)
			return ret;

		/* Set TOPOFF to 10 min */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_TOPOFF,
				MAX77660_ITOPOFF_200MA |
				MAX77660_TOPOFFT_10MIN);
		if (ret < 0)
			return ret;
		/* MBATREG to 4.2V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_BATREGCTRL,
				MAX77660_MBATREG_4200MV);
		if (ret < 0)
			return ret;
		/* MBATREGMAX to 4.2V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_MBATREGMAX,
				MAX77660_MBATREG_4200MV);
		if (ret < 0)
			return ret;

		/* DSILIM_EN = 1; CEN= 1; QBATEN = 0; VSYSREG = 3.6V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_CHGCTRL2,
				MAX77660_VSYSREG_3600MV |
				MAX77660_CEN_MASK |
				MAX77660_PREQ_CUR_MASK |
				MAX77660_DCILIM_EN_MASK);
		if (ret < 0)
			return ret;
		/* Enable top level charging */
		ret = max77660_reg_set_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
		if (ret < 0)
			return ret;
	} else {
		/* disable charge */
		/* Clear top level charge */
		ret = max77660_reg_set_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
		if (ret < 0)
			return ret;
		/* Clear CEN */
		ret = max77660_reg_clr_bits(chip->parent,
			MAX77660_CHG_SLAVE, MAX77660_CHARGER_CHGCTRL2,
			MAX77660_CEN_MASK);
		if (ret < 0)
			return ret;
	}
	dev_info(chip->dev, "%s\n", (enable) ? "Enable charger" :
			"Disable charger");
	return 0;
}

static int max77660_set_charging_current(struct regulator_dev *rdev,
		int min_uA, int max_uA)
{
	struct max77660_chg_extcon *chip = rdev_get_drvdata(rdev);
	struct max77660_charger *charger = chip->charger;
	int ret;
	u8 status;

	ret = max77660_battery_detect(chip);
	if (ret < 0) {
		dev_err(chip->dev,
			"Battery detection failed\n");
		goto error;
	}

	charger->in_current_lim = max_uA/1000;
	charger->status = 0;

	ret = max77660_reg_read(chip->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGSTAT, &status);
	if (ret < 0) {
		dev_err(chip->dev, "CHSTAT read failed: %d\n", ret);
		return ret;
	}
	if (charger->in_current_lim == 0 &&
			!(status & MAX77660_CHG_CHGINT_DC_UVP)) {
		charger->is_100ma = 0;
		return 0;
	}

	if (charger->in_current_lim == 0) {
		charger->is_100ma = 0;
		charger->usb_online = 0;
		charger->ac_online = 0;
		ret = max77660_charger_init(chip, false);
		if (ret < 0)
			goto error;
		charger->status = 2;
		if (charger->update_status)
			charger->update_status
				(charger->status, 2);
	} else {
		if (charger->in_current_lim == 100) {
			charger->is_100ma = 1;
			charger->usb_online = 1;
			charger->ac_online = 0;
		}

		charger->status = 1;
		if (charger->in_current_lim == 500) {
			if (!charger->is_100ma) {
				charger->ac_online = 1;
				charger->usb_online = 0;
			}
		} else if (charger->in_current_lim > 500) {
			charger->ac_online = 1;
			charger->usb_online = 0;
		}

		ret = max77660_charger_init(chip, true);
		if (ret < 0)
			goto error;

		if (charger->update_status)
			charger->update_status
				(charger->status, 1);
	}

	power_supply_changed(&charger->usb_psy);
	power_supply_changed(&charger->dc_psy);
	return 0;
error:
	return ret;
}

static struct regulator_ops max77660_charger_ops = {
	.set_current_limit = max77660_set_charging_current,
};

static int max77660_init_charger_regulator(struct max77660_chg_extcon *chip,
	struct max77660_charger_platform_data *pdata)
{
	int ret = 0;

	if (!pdata) {
		dev_err(chip->dev, "No charger platform data\n");
		return 0;
	}

	chip->chg_reg_desc.name  = "max77660-charger";
	chip->chg_reg_desc.ops   = &max77660_charger_ops;
	chip->chg_reg_desc.type  = REGULATOR_CURRENT;
	chip->chg_reg_desc.owner = THIS_MODULE;

	chip->chg_reg_init_data.supply_regulator     = NULL;
	chip->chg_reg_init_data.regulator_init	= NULL;
	chip->chg_reg_init_data.num_consumer_supplies =
				pdata->num_consumer_supplies;
	chip->chg_reg_init_data.consumer_supplies    =
				pdata->consumer_supplies;
	chip->chg_reg_init_data.driver_data	   = chip->charger;
	chip->chg_reg_init_data.constraints.name     = "max77660-charger";
	chip->chg_reg_init_data.constraints.min_uA   = 0;
	chip->chg_reg_init_data.constraints.max_uA   =
			pdata->max_charge_current_mA * 1000;

	 chip->chg_reg_init_data.constraints.valid_modes_mask =
						REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_STANDBY;

	chip->chg_reg_init_data.constraints.valid_ops_mask =
						REGULATOR_CHANGE_MODE |
						REGULATOR_CHANGE_STATUS |
						REGULATOR_CHANGE_CURRENT;

	chip->chg_rdev = regulator_register(&chip->chg_reg_desc,
				chip->dev, &chip->chg_reg_init_data,
				chip, NULL);
	if (IS_ERR(chip->chg_rdev)) {
		ret = PTR_ERR(chip->chg_rdev);
		dev_err(chip->dev,
			"vbus-charger regulator register failed %d\n", ret);
	}
	return ret;
}

static int max77660_usb_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	 struct max77660_charger *chip;

	 chip = container_of(psy, struct max77660_charger, usb_psy);

	 if (psp == POWER_SUPPLY_PROP_ONLINE)
		val->intval = chip->usb_online;
	 else
		return -EINVAL;
	 return 0;
}

static int max77660_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct max77660_charger *chip;

	chip = container_of(psy, struct max77660_charger, dc_psy);

	if (psp == POWER_SUPPLY_PROP_ONLINE)
		val->intval = chip->ac_online;
	else
		return -EINVAL;
	return 0;
}

static int max77660_chg_extcon_cable_update(
		struct max77660_chg_extcon *chg_extcon)
{
	int ret;
	u8 status;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGSTAT, &status);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "CHSTAT read failed: %d\n", ret);
		return ret;
	}
	if (status & MAX77660_CHG_CHGINT_DC_UVP)
		extcon_set_cable_state(chg_extcon->edev, "USB", false);
	else
		extcon_set_cable_state(chg_extcon->edev, "USB", true);

	dev_info(chg_extcon->dev, "VBUS %s status: 0x%02x\n",
		(status & MAX77660_CHG_CHGINT_DC_UVP) ? "Invalid" : "Valid",
		status);

	return 0;
}

static irqreturn_t max77660_chg_extcon_irq(int irq, void *data)
{
	struct max77660_chg_extcon *chg_extcon = data;
	u8 status;
	int ret;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGINT, &status);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "CHGSTAT read failed: %d\n", ret);
		goto out;
	}

	if (status & MAX77660_CHG_CHGINT_DC_UVP)
		max77660_chg_extcon_cable_update(chg_extcon);
	else
		dev_err(chg_extcon->dev, "CHG-IRQ for unknown reason, 0x%02x\n",
			status);
out:
	return IRQ_HANDLED;
}

static int max77660_charger_wdt(struct max77660_chg_extcon *chip)
{
	struct max77660_charger *charger = chip->charger;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(max77660_chrg_wdt); ++i) {
		if (max77660_chrg_wdt[i] >= charger->wdt_timeout)
			break;
	}

	ret = max77660_reg_update(chip->parent, MAX77660_PWR_SLAVE,
			  MAX77660_REG_GLOBAL_CFG2,
			  MAX77660_GLBLCNFG2_TWD_CHG_MASK,
			  MAX77660_GLBLCNFG2_TWD_CHG(i));
	if (ret < 0) {
		dev_err(chip->dev,
			"GLOBAL_CFG2 update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_set_bits(chip->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
	if (ret < 0) {
		dev_err(chip->dev,
			"GLBLCNFG1_ENCHGTL update failed: %d\n", ret);
		 return ret;
	}
	return ret;
}

static irqreturn_t max77660_chg_wdt_irq(int irq, void *data)
{
	 struct max77660_chg_extcon *chip = (struct max77660_chg_extcon *)data;
	 int ret;

	ret = max77660_reg_write(chip->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_GLOBAL_CFG6,
		MAX77660_GLBLCNFG4_WDTC_SYS_CLR);
	if (ret < 0)
		dev_err(chip->dev, "GLOBAL_CFG4 update failed: %d\n", ret);

	 return IRQ_HANDLED;
}

static int max77660_vbus_enable_time(struct regulator_dev *rdev)
{
	 return 500000;
}

static int max77660_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_RBOOST, &val);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "RBOOST read failed: %d\n", ret);
		return ret;
	}
	return !!(val & MAX77660_RBOOST_RBOOSTEN);
}

static int max77660_vbus_enable(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	int ret;

	ret = max77660_reg_update(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST,
			MAX77660_RBOOST_RBOUT_VOUT(0x6),
			MAX77660_RBOOST_RBOUT_MASK);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "RBOOST update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_set_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST, MAX77660_RBOOST_RBOOSTEN);
	if (ret < 0)
		dev_err(chg_extcon->dev, "RBOOST setbits failed: %d\n", ret);
	return ret;
}

static int max77660_vbus_disable(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	 int ret;

	ret = max77660_reg_clr_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST, MAX77660_RBOOST_RBOOSTEN);
	if (ret < 0)
		dev_err(chg_extcon->dev, "RBOOST clrbits failed: %d\n", ret);
	 return ret;
}

static struct regulator_ops max77660_vbus_ops = {
	.enable		= max77660_vbus_enable,
	.disable	= max77660_vbus_disable,
	.is_enabled	= max77660_vbus_is_enabled,
	.enable_time	= max77660_vbus_enable_time,
};

static struct regulator_desc max77660_vbus_desc = {
	 .name = "max77660-vbus",
	 .ops = &max77660_vbus_ops,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
};

static enum power_supply_property charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int max77660_psy_init(struct max77660_chg_extcon *chg_extcon)
{
	struct max77660_charger *charger = chg_extcon->charger;
	 int ret = 0;

	charger->dc_psy.name		= "ac";
	charger->dc_psy.type		= POWER_SUPPLY_TYPE_MAINS;
	charger->dc_psy.get_property	= max77660_ac_get_property;
	charger->dc_psy.properties	= charger_props;
	charger->dc_psy.num_properties	= ARRAY_SIZE(charger_props);

	ret = power_supply_register(chg_extcon->dev, &charger->dc_psy);
	if (ret < 0) {
		dev_err(chg_extcon->dev,
			"power_supply_register dc failed rc = %d\n", ret);
		 goto out;
	}

	charger->usb_psy.name		= "usb";
	charger->usb_psy.type		= POWER_SUPPLY_TYPE_USB;
	charger->usb_psy.get_property	= max77660_usb_get_property;
	charger->usb_psy.properties	= charger_props;
	charger->usb_psy.num_properties	= ARRAY_SIZE(charger_props);

	ret = power_supply_register(chg_extcon->dev, &charger->usb_psy);

	if (ret < 0) {
		dev_err(chg_extcon->dev,
			"power_supply_register usb failed rc = %d\n", ret);
		goto ac_psy_err;
	}

	return 0;

ac_psy_err:
	power_supply_unregister(&charger->dc_psy);
out:
	return ret;
}

static int __devinit max77660_chg_extcon_probe(struct platform_device *pdev)
{
	struct max77660_chg_extcon *chg_extcon;
	struct max77660_platform_data *pdata;
	struct max77660_charger_platform_data *chg_pdata;
	struct extcon_dev *edev;
	struct max77660_charger *charger;
	int ret;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata || !pdata->charger_pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	chg_pdata = pdata->charger_pdata;

	chg_extcon = devm_kzalloc(&pdev->dev, sizeof(*chg_extcon), GFP_KERNEL);
	if (!chg_extcon) {
		dev_err(&pdev->dev, "Memory allocation failed for chg_extcon\n");
		return -ENOMEM;
	}

	edev = devm_kzalloc(&pdev->dev, sizeof(*edev), GFP_KERNEL);
	if (!edev) {
		dev_err(&pdev->dev, "Memory allocation failed for edev\n");
		return -ENOMEM;
	}

	chg_extcon->charger = devm_kzalloc(&pdev->dev,
				sizeof(*(chg_extcon->charger)), GFP_KERNEL);
	if (!chg_extcon->charger) {
		dev_err(&pdev->dev, "Memory allocation failed for charger\n");
		return -ENOMEM;
	}

	charger = chg_extcon->charger;
	charger->ac_online = 0;
	charger->usb_online = 0;
	charger->status = 0;
	charger->is_100ma = 0;

	chg_extcon->edev = edev;
	chg_extcon->edev->name = (chg_pdata->ext_conn_name) ?
					chg_pdata->ext_conn_name :
					dev_name(&pdev->dev);
	chg_extcon->edev->supported_cable = max77660_excon_cable;

	charger->update_status =  chg_pdata->update_status;
	charger->wdt_timeout = chg_pdata->wdt_timeout;

	chg_extcon->dev = &pdev->dev;
	chg_extcon->parent = pdev->dev.parent;
	dev_set_drvdata(&pdev->dev, chg_extcon);

	chg_extcon->irq = platform_get_irq(pdev, 0);

	chg_extcon->wdt_irq = platform_get_irq(pdev, 1);
	max77660_ext = chg_extcon;

	ret = extcon_dev_register(chg_extcon->edev, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		return ret;
	}

	/* Set initial state */
	ret = max77660_chg_extcon_cable_update(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cable init failed: %d\n", ret);
		goto out;
	}

	ret = request_threaded_irq(chg_extcon->irq, NULL,
		max77660_chg_extcon_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(chg_extcon->dev),
		chg_extcon);
	if (ret < 0) {
		dev_err(chg_extcon->dev,
			"request irq %d failed: %dn", chg_extcon->irq, ret);
		goto out;
	}

	ret = request_threaded_irq(chg_extcon->wdt_irq, NULL,
		max77660_chg_wdt_irq,
		IRQF_ONESHOT, dev_name(chg_extcon->dev),
		chg_extcon);
	if (ret < 0) {
		dev_err(chg_extcon->dev,
			"request irq %d failed: %dn", chg_extcon->irq, ret);
		goto chg_irq_free;
	}

	ret = max77660_reg_clr_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGINTM, MAX77660_CHG_CHGINT_DC_UVP);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "CHGINTM update failed: %d\n", ret);
		goto wdt_irq_free;
	}

	chg_extcon->rdev = regulator_register(&max77660_vbus_desc, &pdev->dev,
				chg_pdata->vbus_reg_init_data, chg_extcon, NULL);
	if (IS_ERR(chg_extcon->rdev)) {
		ret = PTR_ERR(chg_extcon->rdev);
		dev_err(&pdev->dev, "Failed to register VBUS regulator: %d\n",
					ret);
		goto wdt_irq_free;
	}

	ret = max77660_init_charger_regulator(chg_extcon, chg_pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register charger regulator: %d\n",
					ret);
		goto vbus_reg_err;
	}

	ret = max77660_psy_init(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Charger power supply init failed %d\n", ret);
		goto chg_reg_err;
	}

	ret = max77660_charger_wdt(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Charger watchdog timer init failed %d\n", ret);
		goto chg_reg_err;
	}


	device_set_wakeup_capable(&pdev->dev, 1);
	return 0;

chg_reg_err:
	regulator_unregister(chg_extcon->chg_rdev);
vbus_reg_err:
	regulator_unregister(chg_extcon->rdev);
wdt_irq_free:
	free_irq(chg_extcon->wdt_irq, chg_extcon);
chg_irq_free:
	free_irq(chg_extcon->irq, chg_extcon);
out:
	extcon_dev_unregister(chg_extcon->edev);
	return ret;
}

static int __devexit max77660_chg_extcon_remove(struct platform_device *pdev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(&pdev->dev);

	extcon_dev_unregister(chg_extcon->edev);
	free_irq(chg_extcon->irq, chg_extcon);
	regulator_unregister(chg_extcon->rdev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77660_chg_extcon_suspend(struct device *dev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(chg_extcon->irq);
	return 0;
}

static int max77660_chg_extcon_resume(struct device *dev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(chg_extcon->irq);
	return 0;
};
#endif

static const struct dev_pm_ops max77660_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77660_chg_extcon_suspend,
				max77660_chg_extcon_resume)
};

static struct platform_driver max77660_chg_extcon_driver = {
	.probe = max77660_chg_extcon_probe,
	.remove = __devexit_p(max77660_chg_extcon_remove),
	.driver = {
		.name = "max77660-charger-extcon",
		.owner = THIS_MODULE,
		.pm = &max77660_pm_ops,
	},
};

static int __init max77660_chg_extcon_driver_init(void)
{
	return platform_driver_register(&max77660_chg_extcon_driver);
}
subsys_initcall_sync(max77660_chg_extcon_driver_init);

static void __exit max77660_chg_extcon_driver_exit(void)
{
	platform_driver_unregister(&max77660_chg_extcon_driver);
}
module_exit(max77660_chg_extcon_driver_exit);

MODULE_DESCRIPTION("max77660 charger-extcon driver");
MODULE_AUTHOR("Syed Rafiuddin<srafiuddin@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-charger-extcon");
MODULE_LICENSE("GPL v2");
