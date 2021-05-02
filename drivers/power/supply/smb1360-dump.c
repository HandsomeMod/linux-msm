// SPDX-License-Identifier: GPL-2.0-or-later
#define pr_fmt(fmt) "smb1360-dump: " fmt

#include <linux/i2c.h>

enum smb1360_reg_type {
	LITTLE_ENDIAN,
	BIG_ENDIAN,
	RAW
};

struct smb1360_reg {
	u8 addr;
	const char *name;
	u8 size;
	enum smb1360_reg_type type;
};

struct smb1360_regset {
	const char *name;
	u8 start, end;
	struct smb1360_reg regs[];
};

static const struct smb1360_regset smb1360_cfg = {
	.name	= "configuration",
	.start	= 0x00,
	.end	= 0x1C,
	.regs	= {
		{ 0x00, "CFG_BATT_CHG_REG", 1 },
		{ 0x05, "CFG_BATT_CHG_ICL_REG", 1 },
		{ 0x06, "CFG_GLITCH_FLT_REG", 1 },
		{ 0x07, "CFG_CHG_MISC_REG", 1 },
		{ 0x08, "CFG_CHG_FUNC_CTRL_REG", 1 },
		{ 0x09, "CFG_STAT_CTRL_REG", 1 },
		{ 0x0A, "CFG_SFY_TIMER_CTRL_REG", 1 },
		{ 0x0D, "CFG_BATT_MISSING_REG", 1 },
		{ 0x0E, "CFG_FG_BATT_CTRL_REG", 1 },
		{ 0x0F, "IRQ_CFG_REG", 1 },
		{ 0x10, "IRQ2_CFG_REG", 1 },
		{ 0x11, "IRQ3_CFG_REG", 1 },
		{ 0x12, "PRE_TO_FAST_REG", 1 },
		{ 0x13, "CHG_CURRENT_REG", 1 },
		{ 0x14, "CHG_CMP_CFG", 1 },
		{ 0x15, "BATT_CHG_FLT_VTG_REG", 1 },
		{ 0x16, "CFG_FVC_REG", 1 },
		{ 0x1A, "SHDN_CTRL_REG", 1 },
		{ 0x1C, "TRIM_1C_REG", 1 },
		{ }
	}
};

static const struct smb1360_regset smb1360_fg_cfg = {
	.name	= "fg configuration",
	.start	= 0x20,
	.end	= 0x2F,
	.regs	= {
		{ 0x20, "SHDW_FG_ESR_ACTUAL", 2 },
		{ 0x24, "SOC_MAX_REG", 1 },
		{ 0x25, "SOC_MIN_REG", 1 },
		{ 0x26, "VTG_EMPTY_REG", 1 },
		{ 0x27, "Temp_external", 1 },
		{ 0x28, "SOC_DELTA_REG", 1 },
		{ 0x29, "JEITA_SOFT_COLD_REG", 1 },
		{ 0x2A, "JEITA_SOFT_HOT_REG", 1 },
		{ 0x2B, "VTG_MIN_REG", 1 },
		{ 0x2E, "ESR_sys_replace", 2 },
		{ }
	}
};

static const struct smb1360_regset smb1360_cmd = {
	.name	= "command",
	.start	= 0x40,
	.end	= 0x42,
	.regs	= {
		{ 0x40, "CMD_I2C_REG", 1 },
		{ 0x41, "CMD_IL_REG", 1 },
		{ 0x42, "CMD_CHG_REG", 1 },
		{ }
	}
};

static const struct smb1360_regset smb1360_status = {
	.name	= "status",
	.start	= 0x48,
	.end	= 0x4F,
	.regs	= {
		{ 0x48, "STATUS_1_REG", 1 },
		{ 0x4B, "STATUS_3_REG", 1 },
		{ 0x4C, "STATUS_4_REG", 1 },
		{ 0x4F, "REVISION_CTRL_REG", 1 },
		{ }
	}
};

static const struct smb1360_regset smb1360_irq_status = {
	.name	= "irq status",
	.start	= 0x50,
	.end	= 0x58,
	.regs	= {
		{ 0x50, "IRQ_A_REG", 1 },
		{ 0x51, "IRQ_B_REG", 1 },
		{ 0x52, "IRQ_C_REG", 1 },
		{ 0x53, "IRQ_D_REG", 1 },
		{ 0x54, "IRQ_E_REG", 1 },
		{ 0x55, "IRQ_F_REG", 1 },
		{ 0x56, "IRQ_G_REG", 1 },
		{ 0x57, "IRQ_H_REG", 1 },
		{ 0x58, "IRQ_I_REG", 1 },
		{ }
	}
};

static const struct smb1360_regset smb1360_fg_shdw = {
	.name	= "fg shadow",
	.start	= 0x60,
	.end	= 0x6F,
	.regs	= {
		{ 0x60, "SHDW_FG_BATT_STATUS", 1 },
		{ 0x61, "SHDW_FG_MSYS_SOC", 1 },
		{ 0x62, "SHDW_FG_CAPACITY", 2 },
		{ 0x64, "Rslow_drop", 2 },
		{ 0x66, "Latest_SOC", 1 },
		{ 0x67, "Latest_Cutoff_SOC", 1 },
		{ 0x68, "Latest_full_SOC", 1 },
		{ 0x69, "SHDW_FG_VTG_NOW", 2 },
		{ 0x6B, "SHDW_FG_CURR_NOW", 2 },
		{ 0x6D, "SHDW_FG_BATT_TEMP", 2 },
		{ 0x6F, "Latest_system_sbits", 1 },
		{ }
	}
};

static const struct smb1360_regset smb1360_fg_scratch = {
	.name	= "fg scratch pad",
	.start	= 0x80,
	.end	= 0xDC,
	.regs	= {
		{ 0x80, "VOLTAGE_PREDICTED_REG", 2 },
		{ 0x82, "v_cutoff_predicted", 2 },
		{ 0x84, "v_full_predicted", 2 },
		{ 0x86, "ocv_estimate", 2 },
		{ 0x88, "rslow_drop", 2 },
		{ 0x8A, "voltage_old", 2 },
		{ 0x8C, "current_old", 2 },
		{ 0x8E, "current_average_full", 4 },
		{ 0x92, "temperature", 2 },
		{ 0x94, "temp_last_track", 2 },
		{ 0x96, "ESR_nominal", 2 },
		{ 0x9A, "Rslow", 2 },
		{ 0x9C, "counter_imptr", 2 },
		{ 0x9E, "counter_pulse", 2 },
		{ 0xA0, "IRQ_delta_prev", 1 },
		{ 0xA1, "cap_learning_counter", 1 },
		{ 0xA2, "Vact_int_error", 4 },
		{ 0xA6, "SOC_cutoff", 3 },
		{ 0xA9, "SOC_full", 3 },
		{ 0xAC, "SOC_auto_rechrge_temp", 3 },
		{ 0xAF, "Battery_SOC", 3 },
		{ 0xB2, "CC_SOC", 4 },
		{ 0xB6, "SOC_filtered", 2 },
		{ 0xB8, "SOC_Monotonic", 2 },
		{ 0xBA, "CC_TO_SOC_COEFF", 2 },
		{ 0xBC, "NOMINAL_CAPACITY_REG", 2 },
		{ 0xBE, "ACTUAL_CAPACITY_REG", 2 },
		{ 0xC4, "temperature_counter", 1 },
		{ 0xC5, "Vbatt_filtered", 3 },
		{ 0xC8, "Ibatt_filtered", 3 },
		{ 0xCB, "Current_CC_shadow", 2 },
		{ 0xCF, "FG_IBATT_STANDBY_REG", 2 },
		{ 0xD2, "FG_AUTO_RECHARGE_SOC", 1 },
		{ 0xD3, "FG_SYS_CUTOFF_V_REG", 2 },
		{ 0xD5, "FG_CC_TO_CV_V_REG", 2 },
		{ 0xD7, "System_term_current", 2 },
		{ 0xD9, "System_fake_term_current", 2 },
		{ 0xDB, "FG_THERM_C1_COEFF_REG", 2 },
		{ }
	}
};

/* Note: Only very few OTP registers are known, there may be many more! */
static const struct smb1360_regset smb1360_fg_otp = {
	.name	= "fg otp",
	.start	= 0x12,
	.end	= 0x1E,
	.regs	= {
		{ 0x12, "JEITA_HARD_COLD_REG", 1 },
		{ 0x13, "JEITA_HARD_HOT_REG", 1 },
		{ 0x1D, "CURRENT_GAIN_REG", 2 },
		{ }
	}
};

static const struct smb1360_regset smb1360_fg_otp_backup = {
	.name	= "fg otp backup",
	.start	= 0xE0,
	.end	= 0xF1,
	.regs	= {
		{ 0xE0, "OTP_BACKUP_REG", 16, RAW },
		{ 0xF0, "OTP_BACKUP_WA_ALG", 2, BIG_ENDIAN },
		{ }
	}
};

static void smb1360_reg_dump(const struct smb1360_reg *reg, const u8 val[])
{
	u32 num = 0;
	int i;

	switch (reg->type) {
	case RAW:
		pr_info("\t<%#x> %s %*ph\n", reg->addr, reg->name, reg->size, val);
		return;
	case LITTLE_ENDIAN:
		for (i = 0; i < reg->size; ++i) {
			num |= val[i] << (8 * i);
		}
		break;
	case BIG_ENDIAN:
		for (i = 0; i < reg->size; ++i) {
			num <<= 8;
			num |= val[i];
		}
		break;
	}

	pr_info("\t<%#x> %s %0*x\n", reg->addr, reg->name, 2 * reg->size, num);
}

static void smb1360_regset_dump(struct i2c_client *client, const struct smb1360_regset *set)
{
	const struct smb1360_reg *reg = set->regs;
	u8 val[U8_MAX];
	u8 r, next;
	int ret;

	pr_info("%s:\n", set->name);

	for (r = set->start; r <= set->end; r += ret) {
		u8 len = set->end - r + 1;
		int i = r - set->start;

		ret = i2c_smbus_read_i2c_block_data(client, r, len, &val[i]);
		pr_info("\tread %s at %#x len %d, ret %d\n", set->name, r, len, ret);
		if (ret < 0) {
			pr_err("\tFailed to read %d %s registers at %#x: %d\n",
			       len, set->name, r, ret);
			return;
		}
	}

	for (r = set->start; r <= set->end;) {
		int i = r - set->start;

		if (reg->name) {
			if (r == reg->addr) {
				smb1360_reg_dump(reg, &val[i]);
				r += reg->size;
				++reg;
				continue;
			}

			/* Dump until next documented reg */
			next = reg->addr;
		} else {
			/* No more documented regs, dump remaining regs */
			next = set->end + 1;
		}

		pr_info("\t<%#x> %*ph\n", r, next - r, &val[i]);
		r = next;
	}
}

void smb1360_dump(struct i2c_client *client)
{
	smb1360_regset_dump(client, &smb1360_cfg);
	smb1360_regset_dump(client, &smb1360_fg_cfg);
	smb1360_regset_dump(client, &smb1360_cmd);
	smb1360_regset_dump(client, &smb1360_status);
	smb1360_regset_dump(client, &smb1360_irq_status);
	smb1360_regset_dump(client, &smb1360_fg_shdw);
}

/* Should be called with smb1360_enable_fg_access() */
void smb1360_dump_fg_scratch(struct i2c_client *client)
{
	smb1360_regset_dump(client, &smb1360_fg_scratch);
}

/* Should be called with smb1360_enable_fg_access() and FG I2C client */
void smb1360_dump_fg(struct i2c_client *fg_client)
{
	smb1360_regset_dump(fg_client, &smb1360_fg_otp);
	smb1360_regset_dump(fg_client, &smb1360_fg_otp_backup);
	pr_info("\n");
}
