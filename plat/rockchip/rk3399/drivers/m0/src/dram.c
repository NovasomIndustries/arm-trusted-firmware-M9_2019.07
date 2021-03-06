/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <m0_param.h>
#include "rk3399_mcu.h"

/* PMU */
#define PMU_PWRDN_ST		0x18
#define PMU_BUS_IDLE_REQ	0x60
#define PMU_BUS_IDLE_ST		0x64
#define PMU_NOC_AUTO_ENA	0xd8

/* PMU_BUS_IDLE_REQ */
#define IDLE_REQ_MSCH1		(1 << 19)
#define IDLE_REQ_MSCH0		(1 << 18)

/* #define PMU_BUS_IDLE_ST */
#define IDLE_MSCH1		(1 << 19)
#define IDLE_MSCH0		(1 << 18)

#define PD_VOP_PWR_STAT		(1 << 20)

/* CRU */
#define CRU_DPLL_CON0		0x40
#define CRU_DPLL_CON1		0x44
#define CRU_DPLL_CON2		0x48
#define CRU_DPLL_CON3		0x4c
#define CRU_DPLL_CON4		0x50
#define CRU_DPLL_CON5		0x54

#define CRU_DPLL_CON2		0x48
#define CRU_DPLL_CON3		0x4c
#define CRU_CLKGATE10_CON	0x328
#define CRU_CLKGATE28_CON	0x370

/* CRU_CLKGATE10_CON */
#define ACLK_VOP0_PRE_SRC_EN	(1 << 8)
#define HCLK_VOP0_PRE_EN	(1 << 9)
#define ACLK_VOP1_PRE_SRC_EN	(1 << 10)
#define HCLK_VOP1_PRE_EN	(1 << 11)
#define DCLK_VOP0_SRC_EN	(1 << 12)
#define DCLK_VOP1_SRC_EN	(1 << 13)

/* CRU_CLKGATE28_CON */
#define HCLK_VOP0_EN		(1 << 2)
#define ACLK_VOP0_EN		(1 << 3)
#define HCLK_VOP1_EN		(1 << 6)
#define ACLK_VOP1_EN		(1 << 7)

/* CRU_PLL_CON3 */
#define PLL_SLOW_MODE		0
#define PLL_NORMAL_MODE		1
#define PLL_MODE(n)		((0x3 << (8 + 16)) | ((n) << 8))
#define PLL_POWER_DOWN(n)	((0x1 << (0 + 16)) | ((n) << 0))

/* PMU CRU */
#define PMU_CRU_GATEDIS_CON0	0x130

/* VOP */
#define VOP_SYS_CTRL		0x8
#define VOP_SYS_CTRL1		0xc
#define	VOP_INTR_CLEAR0		0x284
#define VOP_INTR_RAW_STATUS0	0x28c

/* VOP_SYS_CTRL */
#define VOP_STANDBY_EN		(1 << 22)

/* VOP_INTR_CLEAR0 */
#define	INT_CLR_DMA_FINISH	(1 << 15)
#define INT_CLR_LINE_FLAG1	(1 << 4)
#define INT_CLR_LINE_FLAG0	(1 << 3)

/* VOP_INTR_RAW_STATUS0 */
#define	INT_RAW_STATUS_DMA_FINISH	(1 << 15)
#define INT_RAW_STATUS_LINE_FLAG1	(1 << 4)
#define INT_RAW_STATUS_LINE_FLAG0	(1 << 3)

/* CIC */
#define CIC_CTRL0		0
#define CIC_CTRL1		0x4
#define CIC_STATUS0		0x10

uint32_t gatedis_con0;

struct ddr_freq_param {
	uint32_t vop_big_en;
	uint32_t vop_lit_en;
	uint32_t dclk0_div;
	uint32_t dclk1_div;
};

static struct ddr_freq_param rk3399_ddr_arg;

static void get_vop_status(void)
{
	rk3399_ddr_arg.vop_big_en = 0;
	rk3399_ddr_arg.vop_lit_en = 0;

	if ((mmio_read_32(PMU_BASE + PMU_PWRDN_ST) & PD_VOP_PWR_STAT) == 0) {
		/* get vop0 status */
		if ((mmio_read_32(CRU_BASE_ADDR + CRU_CLKGATE10_CON) &
				  (DCLK_VOP0_SRC_EN | ACLK_VOP0_PRE_SRC_EN |
				   HCLK_VOP0_PRE_EN)) == 0)
			if ((mmio_read_32(CRU_BASE_ADDR + CRU_CLKGATE28_CON) &
					 (HCLK_VOP0_EN | ACLK_VOP0_EN)) == 0)
				if ((mmio_read_32(VOP_BIG_BASE_ADDR +
						  VOP_SYS_CTRL) &
						  VOP_STANDBY_EN) == 0)
					rk3399_ddr_arg.vop_big_en = 1;

		/* get vop1 satus */
		if ((mmio_read_32(CRU_BASE_ADDR + CRU_CLKGATE10_CON) &
				  (DCLK_VOP1_SRC_EN | ACLK_VOP1_PRE_SRC_EN |
				   HCLK_VOP1_PRE_EN)) == 0)
			if ((mmio_read_32(CRU_BASE_ADDR + CRU_CLKGATE28_CON) &
					  (HCLK_VOP1_EN | ACLK_VOP1_EN)) == 0)
				if ((mmio_read_32(VOP_LITE_BASE_ADDR +
						  VOP_SYS_CTRL) &
						  VOP_STANDBY_EN) == 0)
					rk3399_ddr_arg.vop_lit_en = 1;
	}
}

static void wait_vop_dma_finish(void)
{
	uint32_t vop_adr;

	get_vop_status();

	if (rk3399_ddr_arg.vop_big_en)
		vop_adr = VOP_BIG_BASE_ADDR;
	else if (rk3399_ddr_arg.vop_lit_en)
		vop_adr = VOP_LITE_BASE_ADDR;
	else
		return;

	/* clean Line flag 0 irq and wait for it */
	mmio_write_32(vop_adr + VOP_INTR_CLEAR0,
		      INT_CLR_LINE_FLAG0 | (INT_CLR_LINE_FLAG0 << 16));

	while ((mmio_read_32(vop_adr + VOP_INTR_RAW_STATUS0) &
		INT_RAW_STATUS_LINE_FLAG0) == 0)
		;
}

static void idle_port(void)
{
	gatedis_con0 = mmio_read_32(PMU_CRU_BASE_ADDR + PMU_CRU_GATEDIS_CON0);
	mmio_write_32(PMU_CRU_BASE_ADDR + PMU_CRU_GATEDIS_CON0, 0x3fffffff);
	mmio_setbits_32(PMU_BASE + PMU_BUS_IDLE_REQ,
			IDLE_REQ_MSCH0 | IDLE_REQ_MSCH1);
	while ((mmio_read_32(PMU_BASE + PMU_BUS_IDLE_ST) &
		(IDLE_MSCH1 | IDLE_MSCH0)) != (IDLE_MSCH1 | IDLE_MSCH0))
		continue;

	/* document is wrong, PMU_CRU_GATEDIS_CON0 do not need set MASK BIT */
	mmio_write_32(PMU_CRU_BASE_ADDR + PMU_CRU_GATEDIS_CON0, gatedis_con0);
}

static void deidle_port(void)
{
	mmio_clrbits_32(PMU_BASE + PMU_BUS_IDLE_REQ,
			IDLE_REQ_MSCH0 | IDLE_REQ_MSCH1);
	while (mmio_read_32(PMU_BASE + PMU_BUS_IDLE_ST) &
	       (IDLE_MSCH1 | IDLE_MSCH0))
		continue;
}

static void ddr_set_pll(void)
{
	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON3, PLL_MODE(PLL_SLOW_MODE));

	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON3, PLL_POWER_DOWN(1));
	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON0,
		      mmio_read_32(PARAM_ADDR + PARAM_DPLL_CON0));
	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON1,
		      mmio_read_32(PARAM_ADDR + PARAM_DPLL_CON1));
	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON3, PLL_POWER_DOWN(0));

	while ((mmio_read_32(CRU_BASE_ADDR + CRU_DPLL_CON2) & (1u << 31)) == 0)
		continue;

	mmio_write_32(CRU_BASE_ADDR + CRU_DPLL_CON3, PLL_MODE(PLL_NORMAL_MODE));
}

void handle_dram(void)
{
	wait_vop_dma_finish();

	idle_port();

	mmio_write_32(CIC_BASE_ADDR + CIC_CTRL0,
		      (((0x3 << 4) | (1 << 2) | 1) << 16) |
		      (1 << 2) | 1 |
		      mmio_read_32(PARAM_ADDR + PARAM_FREQ_SELECT));
	while ((mmio_read_32(CIC_BASE_ADDR + CIC_STATUS0) & (1 << 2)) == 0)
		continue;

	ddr_set_pll();
	mmio_write_32(CIC_BASE_ADDR + CIC_CTRL0, 0x20002);
	while ((mmio_read_32(CIC_BASE_ADDR + CIC_STATUS0) & (1 << 0)) == 0)
		continue;

	deidle_port();
}
