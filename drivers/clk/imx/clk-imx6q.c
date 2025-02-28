/*
 * Copyright (C) 2011-2016 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 * Copyright 2017 NXP.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <soc/imx/revision.h>
#include <dt-bindings/clock/imx6qdl-clock.h>

#include "clk.h"

static const char *step_sels[]	= { "osc", "pll2_pfd2_396m", };
static const char *pll1_sw_sels[]	= { "pll1_sys", "step", };
static const char *periph_pre_sels[]	= { "pll2_bus", "pll2_pfd2_396m", "pll2_pfd0_352m", "pll2_198m", };
static const char *periph_clk2_sels[]	= { "pll3_usb_otg", "osc", "osc", "dummy", };
static const char *periph2_clk2_sels[]	= { "pll3_usb_otg", "pll2_bus", };
static const char *periph_sels[]	= { "periph_pre", "periph_clk2", };
static const char *periph2_sels[]	= { "periph2_pre", "periph2_clk2", };
static const char *axi_alt_sels[]	= { "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *axi_sels[]		= { "periph", "axi_alt_sel", };
static const char *audio_sels[]	= { "pll4_audio_div", "pll3_pfd2_508m", "pll3_pfd3_454m", "pll3_usb_otg", };
static const char *gpu_axi_sels[]	= { "axi", "ahb", };
static const char *pre_axi_sels[]	= { "axi", "ahb", };
static const char *gpu2d_core_sels[]	= { "axi", "pll3_usb_otg", "pll2_pfd0_352m", "pll2_pfd2_396m", };
static const char *gpu2d_core_sels_2[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll3_pfd0_720m",};
static const char *gpu3d_core_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll2_pfd2_396m", };
static const char *gpu3d_shader_sels[] = { "mmdc_ch0_axi", "pll3_usb_otg", "pll2_pfd1_594m", "pll3_pfd0_720m", };
static const char *ipu_sels[]		= { "mmdc_ch0_axi", "pll2_pfd2_396m", "pll3_120m", "pll3_pfd1_540m", };
static const char *ldb_di_sels[]	= { "pll5_video_div", "pll2_pfd0_352m", "pll2_pfd2_396m", "mmdc_ch1_axi", "pll3_usb_otg", };
static const char *ldb_di0_div_sels[]	= { "ldb_di0_div_3_5", "ldb_di0_div_7", };
static const char *ldb_di1_div_sels[]	= { "ldb_di1_div_3_5", "ldb_di1_div_7", };
static const char *ipu_di_pre_sels[]	= { "mmdc_ch0_axi", "pll3_usb_otg", "pll5_video_div", "pll2_pfd0_352m", "pll2_pfd2_396m", "pll3_pfd1_540m", };
static const char *ipu1_di0_sels[]	= { "ipu1_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu1_di1_sels[]	= { "ipu1_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di0_sels[]	= { "ipu2_di0_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu2_di1_sels[]	= { "ipu2_di1_pre", "dummy", "dummy", "ldb_di0", "ldb_di1", };
static const char *ipu1_di0_sels_2[]	= { "ipu1_di0_pre", "dummy", "dummy", "ldb_di0_div_sel", "ldb_di1_div_sel", };
static const char *ipu1_di1_sels_2[]	= { "ipu1_di1_pre", "dummy", "dummy", "ldb_di0_div_sel", "ldb_di1_div_sel", };
static const char *ipu2_di0_sels_2[]	= { "ipu2_di0_pre", "dummy", "dummy", "ldb_di0_div_sel", "ldb_di1_div_sel", };
static const char *ipu2_di1_sels_2[]	= { "ipu2_di1_pre", "dummy", "dummy", "ldb_di0_div_sel", "ldb_di1_div_sel", };
static const char *hsi_tx_sels[]	= { "pll3_120m", "pll2_pfd2_396m", };
static const char *pcie_axi_sels[]	= { "axi", "ahb", };
static const char *ssi_sels[]		= { "pll3_pfd2_508m", "pll3_pfd3_454m", "pll4_audio_div", };
static const char *usdhc_sels[]	= { "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *enfc_sels[]	= { "pll2_pfd0_352m", "pll2_bus", "pll3_usb_otg", "pll2_pfd2_396m", };
static const char *enfc_sels_2[] = {"pll2_pfd0_352m", "pll2_bus", "pll3_usb_otg", "pll2_pfd2_396m", "pll3_pfd3_454m", "dummy", };
static const char *eim_sels[]		= { "pll2_pfd2_396m", "pll3_usb_otg", "axi", "pll2_pfd0_352m", };
static const char *eim_slow_sels[]      = { "axi", "pll3_usb_otg", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *vdo_axi_sels[]	= { "axi", "ahb", };
static const char *vpu_axi_sels[]	= { "axi", "pll2_pfd2_396m", "pll2_pfd0_352m", };
static const char *uart_sels[] = { "pll3_80m", "osc", };
static const char *ipg_per_sels[] = { "ipg", "osc", };
static const char *ecspi_sels[] = { "pll3_60m", "osc", };
static const char *can_sels[] = { "pll3_60m", "osc", "pll3_80m", };
static const char *cko1_sels[]	= { "pll3_usb_otg", "pll2_bus", "pll1_sys", "pll5_video_div",
				    "video_27m", "axi", "enfc", "ipu1_di0", "ipu1_di1", "ipu2_di0",
				    "ipu2_di1", "ahb", "ipg", "ipg_per", "ckil", "pll4_audio_div", };
static const char *cko2_sels[] = {
	"mmdc_ch0_axi", "mmdc_ch1_axi", "usdhc4", "usdhc1",
	"gpu2d_axi", "dummy", "ecspi_root", "gpu3d_axi",
	"usdhc3", "dummy", "arm", "ipu1",
	"ipu2", "vdo_axi", "osc", "gpu2d_core",
	"gpu3d_core", "usdhc2", "ssi1", "ssi2",
	"ssi3", "gpu3d_shader", "vpu_axi", "can_root",
	"ldb_di0", "ldb_di1", "esai_extal", "eim_slow",
	"uart_serial", "spdif", "asrc", "hsi_tx",
};
static const char *cko_sels[] = { "cko1", "cko2", };
static const char *lvds_sels[] = {
	"dummy", "dummy", "dummy", "dummy", "dummy", "dummy",
	"pll4_audio", "pll5_video", "pll8_mlb", "enet_ref",
	"pcie_ref_125m", "sata_ref_100m",  "usbphy1", "usbphy2",
	"dummy", "dummy", "dummy", "dummy", "osc",
};
static const char *pll_bypass_src_sels[] = { "osc", "lvds1_in", "lvds2_in", "dummy", };
static const char *pll1_bypass_sels[] = { "pll1", "pll1_bypass_src", };
static const char *pll2_bypass_sels[] = { "pll2", "pll2_bypass_src", };
static const char *pll3_bypass_sels[] = { "pll3", "pll3_bypass_src", };
static const char *pll4_bypass_sels[] = { "pll4", "pll4_bypass_src", };
static const char *pll5_bypass_sels[] = { "pll5", "pll5_bypass_src", };
static const char *pll6_bypass_sels[] = { "pll6", "pll6_bypass_src", };
static const char *pll7_bypass_sels[] = { "pll7", "pll7_bypass_src", };

static struct clk *clk[IMX6QDL_CLK_END];
static struct clk_onecell_data clk_data;
static void __iomem *ccm_base;

static unsigned int const clks_init_on[] __initconst = {
	IMX6QDL_CLK_MMDC_CH0_AXI,
	IMX6QDL_CLK_ROM,
	IMX6QDL_CLK_ARM,
	IMX6QDL_CLK_OCRAM,
	IMX6QDL_CLK_AXI,
};

static struct clk_div_table clk_enet_ref_table[] = {
	{ .val = 0, .div = 20, },
	{ .val = 1, .div = 10, },
	{ .val = 2, .div = 5, },
	{ .val = 3, .div = 4, },
	{ /* sentinel */ }
};

static struct clk_div_table post_div_table[] = {
	{ .val = 2, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 0, .div = 4, },
	{ /* sentinel */ }
};

static struct clk_div_table video_div_table[] = {
	{ .val = 0, .div = 1, },
	{ .val = 1, .div = 2, },
	{ .val = 2, .div = 1, },
	{ .val = 3, .div = 4, },
	{ /* sentinel */ }
};

static unsigned int share_count_esai;
static unsigned int share_count_asrc;
static unsigned int share_count_ssi1;
static unsigned int share_count_ssi2;
static unsigned int share_count_ssi3;
static unsigned int share_count_mipi_core_cfg;
static unsigned int share_count_spdif;
static unsigned int share_count_prg0;
static unsigned int share_count_prg1;

static inline int clk_on_imx6q(void)
{
	return of_machine_is_compatible("fsl,imx6q");
}

static inline int clk_on_imx6qp(void)
{
	return of_machine_is_compatible("fsl,imx6qp");
}

static inline int clk_on_imx6dl(void)
{
	return of_machine_is_compatible("fsl,imx6dl");
}

static struct clk ** const uart_clks[] __initconst = {
	&clk[IMX6QDL_CLK_UART_IPG],
	&clk[IMX6QDL_CLK_UART_SERIAL],
	NULL
};

static int ldb_di_sel_by_clock_id(int clock_id)
{
	switch (clock_id) {
	case IMX6QDL_CLK_PLL5_VIDEO_DIV:
		if (clk_on_imx6q() &&
		    imx_get_soc_revision() == IMX_CHIP_REVISION_1_0)
			return -ENOENT;
		return 0;
	case IMX6QDL_CLK_PLL2_PFD0_352M:
		return 1;
	case IMX6QDL_CLK_PLL2_PFD2_396M:
		return 2;
	case IMX6QDL_CLK_MMDC_CH1_AXI:
		return 3;
	case IMX6QDL_CLK_PLL3_USB_OTG:
		return 4;
	default:
		return -ENOENT;
	}
}

static void of_assigned_ldb_sels(struct device_node *node,
				 unsigned int *ldb_di0_sel,
				 unsigned int *ldb_di1_sel)
{
	struct of_phandle_args clkspec;
	int index, rc, num_parents;
	int parent, child, sel;

	num_parents = of_count_phandle_with_args(node, "assigned-clock-parents",
						 "#clock-cells");
	for (index = 0; index < num_parents; index++) {
		rc = of_parse_phandle_with_args(node, "assigned-clock-parents",
					"#clock-cells", index, &clkspec);
		if (rc < 0) {
			/* skip empty (null) phandles */
			if (rc == -ENOENT)
				continue;
			else
				return;
		}
		if (clkspec.np != node || clkspec.args[0] >= IMX6QDL_CLK_END) {
			pr_err("ccm: parent clock %d not in ccm\n", index);
			return;
		}
		parent = clkspec.args[0];

		rc = of_parse_phandle_with_args(node, "assigned-clocks",
				"#clock-cells", index, &clkspec);
		if (rc < 0)
			return;
		if (clkspec.np != node || clkspec.args[0] >= IMX6QDL_CLK_END) {
			pr_err("ccm: child clock %d not in ccm\n", index);
			return;
		}
		child = clkspec.args[0];

		if (child != IMX6QDL_CLK_LDB_DI0_SEL &&
		    child != IMX6QDL_CLK_LDB_DI1_SEL)
			continue;

		sel = ldb_di_sel_by_clock_id(parent);
		if (sel < 0) {
			pr_err("ccm: invalid ldb_di%d parent clock: %d\n",
			       child == IMX6QDL_CLK_LDB_DI1_SEL, parent);
			continue;
		}

		if (child == IMX6QDL_CLK_LDB_DI0_SEL)
			*ldb_di0_sel = sel;
		if (child == IMX6QDL_CLK_LDB_DI1_SEL)
			*ldb_di1_sel = sel;
	}
}

#define CCM_CCDR		0x04
#define CCM_CCSR		0x0c
#define CCM_CS2CDR		0x2c
#define CCM_CSCDR3		0x3c
#define CCM_CCGR0		0x68
#define CCM_CCGR3		0x74

#define ANATOP_PLL3_PFD		0xf0


#define CCDR_MMDC_CH1_MASK		BIT(16)
#define CCSR_PLL3_SW_CLK_SEL		BIT(0)

#define CS2CDR_LDB_DI0_CLK_SEL_SHIFT	9
#define CS2CDR_LDB_DI1_CLK_SEL_SHIFT	12

#define OCOTP_CFG3			0x440
#define OCOTP_CFG3_SPEED_SHIFT		16
#define OCOTP_CFG3_SPEED_1P2GHZ		0x3

static void __init imx6q_mmdc_ch1_mask_handshake(void __iomem *ccm_base)
{
	unsigned int reg;

	reg = readl_relaxed(ccm_base + CCM_CCDR);
	reg |= CCDR_MMDC_CH1_MASK;
	writel_relaxed(reg, ccm_base + CCM_CCDR);
}

/*
 * The only way to disable the MMDC_CH1 clock is to move it to pll3_sw_clk
 * via periph2_clk2_sel and then to disable pll3_sw_clk by selecting the
 * bypass clock source, since there is no CG bit for mmdc_ch1.
 */
static void mmdc_ch1_disable(void __iomem *ccm_base)
{
	unsigned int reg;

	clk_set_parent(clk[IMX6QDL_CLK_PERIPH2_CLK2_SEL],
		       clk[IMX6QDL_CLK_PLL3_USB_OTG]);

	/*
	 * Handshake with mmdc_ch1 module must be masked when changing
	 * periph2_clk_sel.
	 */
	clk_set_parent(clk[IMX6QDL_CLK_PERIPH2], clk[IMX6QDL_CLK_PERIPH2_CLK2]);

	/* Disable pll3_sw_clk by selecting the bypass clock source */
	reg = readl_relaxed(ccm_base + CCM_CCSR);
	reg |= CCSR_PLL3_SW_CLK_SEL;
	writel_relaxed(reg, ccm_base + CCM_CCSR);
}

static void mmdc_ch1_reenable(void __iomem *ccm_base)
{
	unsigned int reg;

	/* Enable pll3_sw_clk by disabling the bypass */
	reg = readl_relaxed(ccm_base + CCM_CCSR);
	reg &= ~CCSR_PLL3_SW_CLK_SEL;
	writel_relaxed(reg, ccm_base + CCM_CCSR);

	clk_set_parent(clk[IMX6QDL_CLK_PERIPH2], clk[IMX6QDL_CLK_PERIPH2_PRE]);
}

/*
 * We have to follow a strict procedure when changing the LDB clock source,
 * otherwise we risk introducing a glitch that can lock up the LDB divider.
 * Things to keep in mind:
 *
 * 1. The current and new parent clock inputs to the mux must be disabled.
 * 2. The default clock input for ldb_di0/1_clk_sel is mmdc_ch1_axi, which
 *    has no CG bit.
 * 3. pll2_pfd2_396m can not be gated if it is used as memory clock.
 * 4. In the RTL implementation of the LDB_DI_CLK_SEL muxes the top four
 *    options are in one mux and the PLL3 option along with three unused
 *    inputs is in a second mux. There is a third mux with two inputs used
 *    to decide between the first and second 4-port mux:
 *
 *    pll5_video_div 0 --|\
 *    pll2_pfd0_352m 1 --| |_
 *    pll2_pfd2_396m 2 --| | `-|\
 *    mmdc_ch1_axi   3 --|/    | |
 *                             | |--
 *    pll3_usb_otg   4 --|\    | |
 *                   5 --| |_,-|/
 *                   6 --| |
 *                   7 --|/
 *
 * The ldb_di0/1_clk_sel[1:0] bits control both 4-port muxes at the same time.
 * The ldb_di0/1_clk_sel[2] bit controls the 2-port mux. The code below
 * switches the parent to the bottom mux first and then manipulates the top
 * mux to ensure that no glitch will enter the divider.
 */
static void init_ldb_clks(struct device_node *np, void __iomem *ccm_base)
{
	unsigned int reg;
	unsigned int sel[2][4];
	int i;

	reg = readl_relaxed(ccm_base + CCM_CS2CDR);
	sel[0][0] = (reg >> CS2CDR_LDB_DI0_CLK_SEL_SHIFT) & 7;
	sel[1][0] = (reg >> CS2CDR_LDB_DI1_CLK_SEL_SHIFT) & 7;

	sel[0][3] = sel[0][2] = sel[0][1] = sel[0][0];
	sel[1][3] = sel[1][2] = sel[1][1] = sel[1][0];

	of_assigned_ldb_sels(np, &sel[0][3], &sel[1][3]);

	for (i = 0; i < 2; i++) {
		/* Warn if a glitch might have been introduced already */
		if (sel[i][0] != 3) {
			pr_warn("ccm: ldb_di%d_sel already changed from reset value: %d\n",
				i, sel[i][0]);
		}

		if (sel[i][0] == sel[i][3])
			continue;

		/* Only switch to or from pll2_pfd2_396m if it is disabled */
		if ((sel[i][0] == 2 || sel[i][3] == 2) &&
		    (clk_get_parent(clk[IMX6QDL_CLK_PERIPH_PRE]) ==
		     clk[IMX6QDL_CLK_PLL2_PFD2_396M])) {
			pr_err("ccm: ldb_di%d_sel: couldn't disable pll2_pfd2_396m\n",
			       i);
			sel[i][3] = sel[i][2] = sel[i][1] = sel[i][0];
			continue;
		}

		/* First switch to the bottom mux */
		sel[i][1] = sel[i][0] | 4;

		/* Then configure the top mux before switching back to it */
		sel[i][2] = sel[i][3] | 4;

		pr_debug("ccm: switching ldb_di%d_sel: %d->%d->%d->%d\n", i,
			 sel[i][0], sel[i][1], sel[i][2], sel[i][3]);
	}

	if (sel[0][0] == sel[0][3] && sel[1][0] == sel[1][3])
		return;

	mmdc_ch1_disable(ccm_base);

	for (i = 1; i < 4; i++) {
		reg = readl_relaxed(ccm_base + CCM_CS2CDR);
		reg &= ~((7 << CS2CDR_LDB_DI0_CLK_SEL_SHIFT) |
			 (7 << CS2CDR_LDB_DI1_CLK_SEL_SHIFT));
		reg |= ((sel[0][i] << CS2CDR_LDB_DI0_CLK_SEL_SHIFT) |
			(sel[1][i] << CS2CDR_LDB_DI1_CLK_SEL_SHIFT));
		writel_relaxed(reg, ccm_base + CCM_CS2CDR);
	}

	mmdc_ch1_reenable(ccm_base);
}

#define CCM_ANALOG_PLL_VIDEO	0xa0
#define CCM_ANALOG_PFD_480	0xf0
#define CCM_ANALOG_PFD_528	0x100

#define PLL_ENABLE		BIT(13)

#define PFD0_CLKGATE		BIT(7)
#define PFD1_CLKGATE		BIT(15)
#define PFD2_CLKGATE		BIT(23)
#define PFD3_CLKGATE		BIT(31)

/*
 * workaround for ERR010579, when switching the clock source of IPU clock
 * root in CCM. even setting CCGR3[CG0]=0x0 to gate off clock before
 * switching, IPU may hang due to no IPU clock from CCM.
 */
static void __init init_ipu_clk(void __iomem *anatop_base)
{
	u32 val, origin_podf;

	/* gate off the IPU1_IPU clock */
	val = readl_relaxed(ccm_base + CCM_CCGR3);
	val &= ~0x3;
	writel_relaxed(val, ccm_base + CCM_CCGR3);

	/* gate off IPU DCIC1/2 clocks */
	val = readl_relaxed(ccm_base + CCM_CCGR0);
	val &= ~(0xf << 24);
	writel_relaxed(val, ccm_base + CCM_CCGR0);

	/* set IPU_PODF to 3'b000 */
	val = readl_relaxed(ccm_base + CCM_CSCDR3);
	origin_podf = val & (0x7 << 11);
	val &= ~(0x7 << 11);
	writel_relaxed(val, ccm_base + CCM_CSCDR3);

	/* disable PLL3_PFD1 */
	val = readl_relaxed(anatop_base + ANATOP_PLL3_PFD);
	val &= ~(0x1 << 15);
	writel_relaxed(val, anatop_base + ANATOP_PLL3_PFD);

	/* switch IPU_SEL clock to PLL3_PFD1 */
	val = readl_relaxed(ccm_base + CCM_CSCDR3);
	val |= (0x3 << 9);
	writel_relaxed(val, ccm_base + CCM_CSCDR3);

	 /* restore the IPU PODF*/
	val = readl_relaxed(ccm_base + CCM_CSCDR3);
	val |= origin_podf;
	writel_relaxed(val, ccm_base + CCM_CSCDR3);

	/* enable PLL3_PFD1 */
	val = readl_relaxed(anatop_base + ANATOP_PLL3_PFD);
	val |= (0x1 << 15);
	writel_relaxed(val, anatop_base + ANATOP_PLL3_PFD);

	/* enable IPU1_IPU clock */
	val = readl_relaxed(ccm_base + CCM_CCGR3);
	val |= 0x3;
	writel_relaxed(val, ccm_base + CCM_CCGR3);

	/* enable IPU DCIC1/2 clock */
	val = readl_relaxed(ccm_base + CCM_CCGR0);
	val |= (0xf << 24);
	writel_relaxed(val, ccm_base + CCM_CCGR0);
}

static void disable_anatop_clocks(void __iomem *anatop_base)
{
	unsigned int reg;
	struct clk *parent = clk_get_parent(clk[IMX6QDL_CLK_PERIPH_PRE]);

	/* Make sure PLL2 PFDs 0-2 are gated */
	reg = readl_relaxed(anatop_base + CCM_ANALOG_PFD_528);
	reg |= PFD1_CLKGATE;				/* Disable PFD1 */

	/* Cannot gate PFD2 if pll2_pfd2_396m is the parent of MMDC clock */
	if (parent == clk[IMX6QDL_CLK_PLL2_PFD0_352M]) {
		reg |= PFD2_CLKGATE;			/* Disable PFD2 */
	} else {
		reg |= PFD0_CLKGATE;			/* Disable PFD0 */
		if (parent == clk[IMX6QDL_CLK_PLL2_BUS])
			reg |= PFD2_CLKGATE;		/* Disable PFD2 */
	}
	writel_relaxed(reg, anatop_base + CCM_ANALOG_PFD_528);

	/* Make sure PLL3 PFDs 0-3 are gated */
	reg = readl_relaxed(anatop_base + CCM_ANALOG_PFD_480);
	reg |= PFD0_CLKGATE | PFD1_CLKGATE | PFD2_CLKGATE | PFD3_CLKGATE;
	writel_relaxed(reg, anatop_base + CCM_ANALOG_PFD_480);

	/* Make sure PLL5 is disabled */
	reg = readl_relaxed(anatop_base + CCM_ANALOG_PLL_VIDEO);
	reg &= ~PLL_ENABLE;
	writel_relaxed(reg, anatop_base + CCM_ANALOG_PLL_VIDEO);
}

static void __init imx6q_clocks_init(struct device_node *ccm_node)
{
	struct device_node *np;
	void __iomem *anatop_base, *base;
	int i;
	u32 val;

	clk[IMX6QDL_CLK_DUMMY] = imx_clk_fixed("dummy", 0);
	clk[IMX6QDL_CLK_CKIL] = imx_obtain_fixed_clock("ckil", 0);
	clk[IMX6QDL_CLK_CKIH] = imx_obtain_fixed_clock("ckih1", 0);
	clk[IMX6QDL_CLK_OSC] = imx_obtain_fixed_clock("osc", 0);
	/* Clock source from external clock via CLK1/2 PADs */
	clk[IMX6QDL_CLK_ANACLK1] = imx_obtain_fixed_clock("anaclk1", 0);
	clk[IMX6QDL_CLK_ANACLK2] = imx_obtain_fixed_clock("anaclk2", 0);

	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-anatop");
	anatop_base = base = of_iomap(np, 0);
	WARN_ON(!base);

	/* Audio/video PLL post dividers do not work on i.MX6q revision 1.0 */
	if (clk_on_imx6q() && imx_get_soc_revision() == IMX_CHIP_REVISION_1_0) {
		post_div_table[1].div = 1;
		post_div_table[2].div = 1;
		video_div_table[1].div = 1;
		video_div_table[3].div = 1;
	}

	clk[IMX6QDL_PLL1_BYPASS_SRC] = imx_clk_mux("pll1_bypass_src", base + 0x00, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL2_BYPASS_SRC] = imx_clk_mux("pll2_bypass_src", base + 0x30, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL3_BYPASS_SRC] = imx_clk_mux("pll3_bypass_src", base + 0x10, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL4_BYPASS_SRC] = imx_clk_mux("pll4_bypass_src", base + 0x70, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL5_BYPASS_SRC] = imx_clk_mux("pll5_bypass_src", base + 0xa0, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL6_BYPASS_SRC] = imx_clk_mux("pll6_bypass_src", base + 0xe0, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));
	clk[IMX6QDL_PLL7_BYPASS_SRC] = imx_clk_mux("pll7_bypass_src", base + 0x20, 14, 2, pll_bypass_src_sels, ARRAY_SIZE(pll_bypass_src_sels));

	/*                                    type               name    parent_name        base         div_mask */
	clk[IMX6QDL_CLK_PLL1] = imx_clk_pllv3(IMX_PLLV3_SYS,     "pll1", "osc", base + 0x00, 0x7f);
	clk[IMX6QDL_CLK_PLL2] = imx_clk_pllv3(IMX_PLLV3_PLL2,    "pll2", "osc", base + 0x30, 0x1);
	clk[IMX6QDL_CLK_PLL3] = imx_clk_pllv3(IMX_PLLV3_USB,     "pll3", "osc", base + 0x10, 0x3);
	clk[IMX6QDL_CLK_PLL4] = imx_clk_pllv3(IMX_PLLV3_AV,      "pll4", "osc", base + 0x70, 0x7f);
	clk[IMX6QDL_CLK_PLL5] = imx_clk_pllv3(IMX_PLLV3_AV,      "pll5", "osc", base + 0xa0, 0x7f);
	clk[IMX6QDL_CLK_PLL6] = imx_clk_pllv3(IMX_PLLV3_ENET,    "pll6", "osc", base + 0xe0, 0x3);
	clk[IMX6QDL_CLK_PLL7] = imx_clk_pllv3(IMX_PLLV3_USB,     "pll7", "osc", base + 0x20, 0x3);

	clk[IMX6QDL_PLL1_BYPASS] = imx_clk_mux_flags("pll1_bypass", base + 0x00, 16, 1, pll1_bypass_sels, ARRAY_SIZE(pll1_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL2_BYPASS] = imx_clk_mux_flags("pll2_bypass", base + 0x30, 16, 1, pll2_bypass_sels, ARRAY_SIZE(pll2_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL3_BYPASS] = imx_clk_mux_flags("pll3_bypass", base + 0x10, 16, 1, pll3_bypass_sels, ARRAY_SIZE(pll3_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL4_BYPASS] = imx_clk_mux_flags("pll4_bypass", base + 0x70, 16, 1, pll4_bypass_sels, ARRAY_SIZE(pll4_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL5_BYPASS] = imx_clk_mux_flags("pll5_bypass", base + 0xa0, 16, 1, pll5_bypass_sels, ARRAY_SIZE(pll5_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL6_BYPASS] = imx_clk_mux_flags("pll6_bypass", base + 0xe0, 16, 1, pll6_bypass_sels, ARRAY_SIZE(pll6_bypass_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_PLL7_BYPASS] = imx_clk_mux_flags("pll7_bypass", base + 0x20, 16, 1, pll7_bypass_sels, ARRAY_SIZE(pll7_bypass_sels), CLK_SET_RATE_PARENT);

	/* Do not bypass PLLs initially */
	clk_set_parent(clk[IMX6QDL_PLL1_BYPASS], clk[IMX6QDL_CLK_PLL1]);
	clk_set_parent(clk[IMX6QDL_PLL2_BYPASS], clk[IMX6QDL_CLK_PLL2]);
	clk_set_parent(clk[IMX6QDL_PLL3_BYPASS], clk[IMX6QDL_CLK_PLL3]);
	clk_set_parent(clk[IMX6QDL_PLL4_BYPASS], clk[IMX6QDL_CLK_PLL4]);
	clk_set_parent(clk[IMX6QDL_PLL5_BYPASS], clk[IMX6QDL_CLK_PLL5]);
	clk_set_parent(clk[IMX6QDL_PLL6_BYPASS], clk[IMX6QDL_CLK_PLL6]);
	clk_set_parent(clk[IMX6QDL_PLL7_BYPASS], clk[IMX6QDL_CLK_PLL7]);

	clk[IMX6QDL_CLK_PLL1_SYS]      = imx_clk_fixed_factor("pll1_sys",      "pll1_bypass", 1, 1);
	clk[IMX6QDL_CLK_PLL2_BUS]      = imx_clk_gate("pll2_bus",      "pll2_bypass", base + 0x30, 13);
	clk[IMX6QDL_CLK_PLL3_USB_OTG]  = imx_clk_gate("pll3_usb_otg",  "pll3_bypass", base + 0x10, 13);
	clk[IMX6QDL_CLK_PLL4_AUDIO]    = imx_clk_gate("pll4_audio",    "pll4_bypass", base + 0x70, 13);
	clk[IMX6QDL_CLK_PLL5_VIDEO]    = imx_clk_gate("pll5_video",    "pll5_bypass", base + 0xa0, 13);
	clk[IMX6QDL_CLK_PLL6_ENET]     = imx_clk_gate("pll6_enet",     "pll6_bypass", base + 0xe0, 13);
	clk[IMX6QDL_CLK_PLL7_USB_HOST] = imx_clk_gate("pll7_usb_host", "pll7_bypass", base + 0x20, 13);

	/*
	 * Bit 20 is the reserved and read-only bit, we do this only for:
	 * - Do nothing for usbphy clk_enable/disable
	 * - Keep refcount when do usbphy clk_enable/disable, in that case,
	 * the clk framework may need to enable/disable usbphy's parent
	 */
	clk[IMX6QDL_CLK_USBPHY1] = imx_clk_gate("usbphy1", "pll3_usb_otg", base + 0x10, 20);
	clk[IMX6QDL_CLK_USBPHY2] = imx_clk_gate("usbphy2", "pll7_usb_host", base + 0x20, 20);

	/*
	 * usbphy*_gate needs to be on after system boots up, and software
	 * never needs to control it anymore.
	 */
	clk[IMX6QDL_CLK_USBPHY1_GATE] = imx_clk_gate("usbphy1_gate", "dummy", base + 0x10, 6);
	clk[IMX6QDL_CLK_USBPHY2_GATE] = imx_clk_gate("usbphy2_gate", "dummy", base + 0x20, 6);

	clk[IMX6QDL_CLK_SATA_REF] = imx_clk_fixed_factor("sata_ref", "pll6_enet", 1, 5);
	clk[IMX6QDL_CLK_PCIE_REF] = imx_clk_fixed_factor("pcie_ref", "pll6_enet", 1, 4);

	clk[IMX6QDL_CLK_SATA_REF_100M] = imx_clk_gate("sata_ref_100m", "sata_ref", base + 0xe0, 20);
	clk[IMX6QDL_CLK_PCIE_REF_125M] = imx_clk_gate("pcie_ref_125m", "pcie_ref", base + 0xe0, 19);

	clk[IMX6QDL_CLK_ENET_REF] = clk_register_divider_table(NULL, "enet_ref", "pll6_enet", 0,
			base + 0xe0, 0, 2, 0, clk_enet_ref_table,
			&imx_ccm_lock);

	clk[IMX6QDL_CLK_LVDS1_SEL] = imx_clk_mux("lvds1_sel", base + 0x160, 0, 5, lvds_sels, ARRAY_SIZE(lvds_sels));
	clk[IMX6QDL_CLK_LVDS2_SEL] = imx_clk_mux("lvds2_sel", base + 0x160, 5, 5, lvds_sels, ARRAY_SIZE(lvds_sels));

	/*
	 * lvds1_gate and lvds2_gate are pseudo-gates.  Both can be
	 * independently configured as clock inputs or outputs.  We treat
	 * the "output_enable" bit as a gate, even though it's really just
	 * enabling clock output.
	 */
	clk[IMX6QDL_CLK_LVDS1_GATE] = imx_clk_gate_exclusive("lvds1_gate", "lvds1_sel", base + 0x160, 10, BIT(12));
	clk[IMX6QDL_CLK_LVDS2_GATE] = imx_clk_gate_exclusive("lvds2_gate", "lvds2_sel", base + 0x160, 11, BIT(13));

	clk[IMX6QDL_CLK_LVDS1_IN] = imx_clk_gate_exclusive("lvds1_in", "anaclk1", base + 0x160, 12, BIT(10));
	clk[IMX6QDL_CLK_LVDS2_IN] = imx_clk_gate_exclusive("lvds2_in", "anaclk2", base + 0x160, 13, BIT(11));

	/*                                            name              parent_name        reg       idx */
	clk[IMX6QDL_CLK_PLL2_PFD0_352M] = imx_clk_pfd("pll2_pfd0_352m", "pll2_bus",     base + 0x100, 0);
	clk[IMX6QDL_CLK_PLL2_PFD1_594M] = imx_clk_pfd("pll2_pfd1_594m", "pll2_bus",     base + 0x100, 1);
	clk[IMX6QDL_CLK_PLL2_PFD2_396M] = imx_clk_pfd("pll2_pfd2_396m", "pll2_bus",     base + 0x100, 2);
	clk[IMX6QDL_CLK_PLL3_PFD0_720M] = imx_clk_pfd("pll3_pfd0_720m", "pll3_usb_otg", base + 0xf0,  0);
	clk[IMX6QDL_CLK_PLL3_PFD1_540M] = imx_clk_pfd("pll3_pfd1_540m", "pll3_usb_otg", base + 0xf0,  1);
	clk[IMX6QDL_CLK_PLL3_PFD2_508M] = imx_clk_pfd("pll3_pfd2_508m", "pll3_usb_otg", base + 0xf0,  2);
	clk[IMX6QDL_CLK_PLL3_PFD3_454M] = imx_clk_pfd("pll3_pfd3_454m", "pll3_usb_otg", base + 0xf0,  3);

	/*                                                name         parent_name     mult div */
	clk[IMX6QDL_CLK_PLL2_198M] = imx_clk_fixed_factor("pll2_198m", "pll2_pfd2_396m", 1, 2);
	clk[IMX6QDL_CLK_PLL3_120M] = imx_clk_fixed_factor("pll3_120m", "pll3_usb_otg",   1, 4);
	clk[IMX6QDL_CLK_PLL3_80M]  = imx_clk_fixed_factor("pll3_80m",  "pll3_usb_otg",   1, 6);
	clk[IMX6QDL_CLK_PLL3_60M]  = imx_clk_fixed_factor("pll3_60m",  "pll3_usb_otg",   1, 8);
	clk[IMX6QDL_CLK_TWD]       = imx_clk_fixed_factor("twd",       "arm",            1, 2);
	clk[IMX6QDL_CLK_GPT_3M]    = imx_clk_fixed_factor("gpt_3m",    "osc",            1, 8);
	clk[IMX6QDL_CLK_VIDEO_27M] = imx_clk_fixed_factor("video_27m", "pll3_pfd1_540m", 1, 20);
	if (clk_on_imx6dl() || clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_GPU2D_AXI] = imx_clk_fixed_factor("gpu2d_axi", "mmdc_ch0_axi_podf", 1, 1);
		clk[IMX6QDL_CLK_GPU3D_AXI] = imx_clk_fixed_factor("gpu3d_axi", "mmdc_ch0_axi_podf", 1, 1);
	}

	clk[IMX6QDL_CLK_PLL4_POST_DIV] = clk_register_divider_table(NULL, "pll4_post_div", "pll4_audio", CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE, base + 0x70, 19, 2, 0, post_div_table, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL4_AUDIO_DIV] = clk_register_divider(NULL, "pll4_audio_div", "pll4_post_div", CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE, base + 0x170, 15, 1, 0, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL5_POST_DIV] = clk_register_divider_table(NULL, "pll5_post_div", "pll5_video", CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE, base + 0xa0, 19, 2, 0, post_div_table, &imx_ccm_lock);
	clk[IMX6QDL_CLK_PLL5_VIDEO_DIV] = clk_register_divider_table(NULL, "pll5_video_div", "pll5_post_div", CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE, base + 0x170, 30, 2, 0, video_div_table, &imx_ccm_lock);

	np = ccm_node;
	base = of_iomap(np, 0);
	ccm_base = base;
	WARN_ON(!base);
	imx6q_mmdc_ch1_mask_handshake(base);

	/*                                              name                reg       shift width parent_names     num_parents */
	clk[IMX6QDL_CLK_STEP]             = imx_clk_mux("step",	            base + 0xc,  8,  1, step_sels,	   ARRAY_SIZE(step_sels));
	clk[IMX6QDL_CLK_PLL1_SW]          = imx_clk_mux_glitchless("pll1_sw", base + 0xc, 2, 1, pll1_sw_sels,      ARRAY_SIZE(pll1_sw_sels));
	clk[IMX6QDL_CLK_PERIPH_PRE]       = imx_clk_mux_bus("periph_pre",       base + 0x18, 18, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[IMX6QDL_CLK_PERIPH2_PRE]      = imx_clk_mux("periph2_pre",      base + 0x18, 21, 2, periph_pre_sels,   ARRAY_SIZE(periph_pre_sels));
	clk[IMX6QDL_CLK_PERIPH_CLK2_SEL]  = imx_clk_mux_bus("periph_clk2_sel",  base + 0x18, 12, 2, periph_clk2_sels,  ARRAY_SIZE(periph_clk2_sels));
	clk[IMX6QDL_CLK_PERIPH2_CLK2_SEL] = imx_clk_mux("periph2_clk2_sel", base + 0x18, 20, 1, periph2_clk2_sels, ARRAY_SIZE(periph2_clk2_sels));
	clk[IMX6QDL_CLK_AXI_ALT_SEL]      = imx_clk_mux("axi_alt_sel",      base + 0x14, 7,  1, axi_alt_sels,      ARRAY_SIZE(axi_alt_sels));
	clk[IMX6QDL_CLK_AXI_SEL]          = imx_clk_mux_glitchless("axi_sel", base + 0x14, 6, 1, axi_sels,         ARRAY_SIZE(axi_sels));
	clk[IMX6QDL_CLK_ESAI_SEL]         = imx_clk_mux("esai_sel",         base + 0x20, 19, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[IMX6QDL_CLK_ASRC_SEL]         = imx_clk_mux("asrc_sel",         base + 0x30, 7,  2, audio_sels,        ARRAY_SIZE(audio_sels));
	clk[IMX6QDL_CLK_SPDIF_SEL]        = imx_clk_mux("spdif_sel",        base + 0x30, 20, 2, audio_sels,        ARRAY_SIZE(audio_sels));
	if (clk_on_imx6q()) {
		clk[IMX6QDL_CLK_GPU2D_AXI]        = imx_clk_mux("gpu2d_axi",        base + 0x18, 0,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
		clk[IMX6QDL_CLK_GPU3D_AXI]        = imx_clk_mux("gpu3d_axi",        base + 0x18, 1,  1, gpu_axi_sels,      ARRAY_SIZE(gpu_axi_sels));
	}
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_CAN_SEL]   = imx_clk_mux("can_sel",	base + 0x20, 8,  2, can_sels, ARRAY_SIZE(can_sels));
		clk[IMX6QDL_CLK_ECSPI_SEL] = imx_clk_mux("ecspi_sel",	base + 0x38, 18, 1, ecspi_sels,  ARRAY_SIZE(ecspi_sels));
		clk[IMX6QDL_CLK_IPG_PER_SEL] = imx_clk_mux("ipg_per_sel", base + 0x1c, 6, 1, ipg_per_sels, ARRAY_SIZE(ipg_per_sels));
		clk[IMX6QDL_CLK_UART_SEL] = imx_clk_mux("uart_sel", base + 0x24, 6, 1, uart_sels, ARRAY_SIZE(uart_sels));
		clk[IMX6QDL_CLK_GPU2D_CORE_SEL] = imx_clk_mux("gpu2d_core_sel", base + 0x18, 16, 2, gpu2d_core_sels_2, ARRAY_SIZE(gpu2d_core_sels_2));
	} else if (clk_on_imx6dl()) {
		clk[IMX6QDL_CLK_MLB_SEL] = imx_clk_mux("mlb_sel",   base + 0x18, 16, 2, gpu2d_core_sels,   ARRAY_SIZE(gpu2d_core_sels));
	} else {
		clk[IMX6QDL_CLK_GPU2D_CORE_SEL] = imx_clk_mux("gpu2d_core_sel",   base + 0x18, 16, 2, gpu2d_core_sels,   ARRAY_SIZE(gpu2d_core_sels));
	}
	clk[IMX6QDL_CLK_GPU3D_CORE_SEL]   = imx_clk_mux("gpu3d_core_sel",   base + 0x18, 4,  2, gpu3d_core_sels,   ARRAY_SIZE(gpu3d_core_sels));
	if (clk_on_imx6dl())
		clk[IMX6QDL_CLK_GPU2D_CORE_SEL] = imx_clk_mux("gpu2d_core_sel", base + 0x18, 8,  2, gpu3d_shader_sels, ARRAY_SIZE(gpu3d_shader_sels));
	else
		clk[IMX6QDL_CLK_GPU3D_SHADER_SEL] = imx_clk_mux("gpu3d_shader_sel", base + 0x18, 8,  2, gpu3d_shader_sels, ARRAY_SIZE(gpu3d_shader_sels));
	clk[IMX6QDL_CLK_IPU1_SEL]         = imx_clk_mux("ipu1_sel",         base + 0x3c, 9,  2, ipu_sels,          ARRAY_SIZE(ipu_sels));
	clk[IMX6QDL_CLK_IPU2_SEL]         = imx_clk_mux("ipu2_sel",         base + 0x3c, 14, 2, ipu_sels,          ARRAY_SIZE(ipu_sels));

	if (clk_on_imx6q() && imx_get_soc_revision() >= IMX_CHIP_REVISION_2_0) {
		clk[IMX6QDL_CLK_LDB_DI0_SEL]      = imx_clk_mux_flags("ldb_di0_sel", base + 0x2c, 9,  3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_LDB_DI1_SEL]      = imx_clk_mux_flags("ldb_di1_sel", base + 0x2c, 12, 3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels), CLK_SET_RATE_PARENT);
	} else {
		disable_anatop_clocks(anatop_base);

		/*
		 * The LDB_DI0/1_SEL muxes are registered read-only due to a hardware
		 * bug. Set the muxes to the requested values before registering the
		 * ldb_di_sel clocks.
		 */
		init_ldb_clks(np, base);

		clk[IMX6QDL_CLK_LDB_DI0_SEL]      = imx_clk_mux_ldb("ldb_di0_sel", base + 0x2c, 9,  3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels));
		clk[IMX6QDL_CLK_LDB_DI1_SEL]      = imx_clk_mux_ldb("ldb_di1_sel", base + 0x2c, 12, 3, ldb_di_sels,      ARRAY_SIZE(ldb_di_sels));
	}
	clk[IMX6QDL_CLK_LDB_DI0_DIV_SEL]  = imx_clk_mux_flags("ldb_di0_div_sel", base + 0x20, 10, 1, ldb_di0_div_sels, ARRAY_SIZE(ldb_di0_div_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_LDB_DI1_DIV_SEL]  = imx_clk_mux_flags("ldb_di1_div_sel", base + 0x20, 11, 1, ldb_di1_div_sels, ARRAY_SIZE(ldb_di1_div_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_IPU1_DI0_PRE_SEL] = imx_clk_mux_flags("ipu1_di0_pre_sel", base + 0x34, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_IPU1_DI1_PRE_SEL] = imx_clk_mux_flags("ipu1_di1_pre_sel", base + 0x34, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_IPU2_DI0_PRE_SEL] = imx_clk_mux_flags("ipu2_di0_pre_sel", base + 0x38, 6,  3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_IPU2_DI1_PRE_SEL] = imx_clk_mux_flags("ipu2_di1_pre_sel", base + 0x38, 15, 3, ipu_di_pre_sels,   ARRAY_SIZE(ipu_di_pre_sels), CLK_SET_RATE_PARENT);
	clk[IMX6QDL_CLK_HSI_TX_SEL]       = imx_clk_mux("hsi_tx_sel",       base + 0x30, 28, 1, hsi_tx_sels,       ARRAY_SIZE(hsi_tx_sels));
	clk[IMX6QDL_CLK_PCIE_AXI_SEL]     = imx_clk_mux("pcie_axi_sel",     base + 0x18, 10, 1, pcie_axi_sels,     ARRAY_SIZE(pcie_axi_sels));
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_IPU1_DI0_SEL]     = imx_clk_mux_flags("ipu1_di0_sel",     base + 0x34, 0,  3, ipu1_di0_sels_2,     ARRAY_SIZE(ipu1_di0_sels_2), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU1_DI1_SEL]     = imx_clk_mux_flags("ipu1_di1_sel",     base + 0x34, 9,  3, ipu1_di1_sels_2,     ARRAY_SIZE(ipu1_di1_sels_2), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU2_DI0_SEL]     = imx_clk_mux_flags("ipu2_di0_sel",     base + 0x38, 0,  3, ipu2_di0_sels_2,     ARRAY_SIZE(ipu2_di0_sels_2), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU2_DI1_SEL]     = imx_clk_mux_flags("ipu2_di1_sel",     base + 0x38, 9,  3, ipu2_di1_sels_2,     ARRAY_SIZE(ipu2_di1_sels_2), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_SSI1_SEL]         = imx_clk_mux("ssi1_sel",   base + 0x1c, 10, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
		clk[IMX6QDL_CLK_SSI2_SEL]         = imx_clk_mux("ssi2_sel",   base + 0x1c, 12, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
		clk[IMX6QDL_CLK_SSI3_SEL]         = imx_clk_mux("ssi3_sel",   base + 0x1c, 14, 2, ssi_sels,          ARRAY_SIZE(ssi_sels));
		clk[IMX6QDL_CLK_USDHC1_SEL]       = imx_clk_mux("usdhc1_sel", base + 0x1c, 16, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
		clk[IMX6QDL_CLK_USDHC2_SEL]       = imx_clk_mux("usdhc2_sel", base + 0x1c, 17, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
		clk[IMX6QDL_CLK_USDHC3_SEL]       = imx_clk_mux("usdhc3_sel", base + 0x1c, 18, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
		clk[IMX6QDL_CLK_USDHC4_SEL]       = imx_clk_mux("usdhc4_sel", base + 0x1c, 19, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels));
		clk[IMX6QDL_CLK_ENFC_SEL]         = imx_clk_mux("enfc_sel",         base + 0x2c, 15, 3, enfc_sels_2,         ARRAY_SIZE(enfc_sels_2));
		clk[IMX6QDL_CLK_EIM_SEL]          = imx_clk_mux("eim_sel",      base + 0x1c, 27, 2, eim_sels,        ARRAY_SIZE(eim_sels));
		clk[IMX6QDL_CLK_EIM_SLOW_SEL]     = imx_clk_mux("eim_slow_sel", base + 0x1c, 29, 2, eim_slow_sels,   ARRAY_SIZE(eim_slow_sels));
		clk[IMX6QDL_CLK_PRE_AXI]	  = imx_clk_mux("pre_axi",	base + 0x18, 1,  1, pre_axi_sels,    ARRAY_SIZE(pre_axi_sels));
	} else {
		clk[IMX6QDL_CLK_IPU1_DI0_SEL]     = imx_clk_mux_flags("ipu1_di0_sel",     base + 0x34, 0,  3, ipu1_di0_sels,     ARRAY_SIZE(ipu1_di0_sels), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU1_DI1_SEL]     = imx_clk_mux_flags("ipu1_di1_sel",     base + 0x34, 9,  3, ipu1_di1_sels,     ARRAY_SIZE(ipu1_di1_sels), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU2_DI0_SEL]     = imx_clk_mux_flags("ipu2_di0_sel",     base + 0x38, 0,  3, ipu2_di0_sels,     ARRAY_SIZE(ipu2_di0_sels), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_IPU2_DI1_SEL]     = imx_clk_mux_flags("ipu2_di1_sel",     base + 0x38, 9,  3, ipu2_di1_sels,     ARRAY_SIZE(ipu2_di1_sels), CLK_SET_RATE_PARENT);
		clk[IMX6QDL_CLK_SSI1_SEL]         = imx_clk_fixup_mux("ssi1_sel",   base + 0x1c, 10, 2, ssi_sels,          ARRAY_SIZE(ssi_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_SSI2_SEL]         = imx_clk_fixup_mux("ssi2_sel",   base + 0x1c, 12, 2, ssi_sels,          ARRAY_SIZE(ssi_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_SSI3_SEL]         = imx_clk_fixup_mux("ssi3_sel",   base + 0x1c, 14, 2, ssi_sels,          ARRAY_SIZE(ssi_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_USDHC1_SEL]       = imx_clk_fixup_mux("usdhc1_sel", base + 0x1c, 16, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_USDHC2_SEL]       = imx_clk_fixup_mux("usdhc2_sel", base + 0x1c, 17, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_USDHC3_SEL]       = imx_clk_fixup_mux("usdhc3_sel", base + 0x1c, 18, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_USDHC4_SEL]       = imx_clk_fixup_mux("usdhc4_sel", base + 0x1c, 19, 1, usdhc_sels,        ARRAY_SIZE(usdhc_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_ENFC_SEL]         = imx_clk_mux("enfc_sel",         base + 0x2c, 16, 2, enfc_sels,         ARRAY_SIZE(enfc_sels));
		clk[IMX6QDL_CLK_EIM_SEL]          = imx_clk_fixup_mux("eim_sel",      base + 0x1c, 27, 2, eim_sels,        ARRAY_SIZE(eim_sels), imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_EIM_SLOW_SEL]     = imx_clk_fixup_mux("eim_slow_sel", base + 0x1c, 29, 2, eim_slow_sels,   ARRAY_SIZE(eim_slow_sels), imx_cscmr1_fixup);
	}
	clk[IMX6QDL_CLK_VDO_AXI_SEL]      = imx_clk_mux("vdo_axi_sel",      base + 0x18, 11, 1, vdo_axi_sels,      ARRAY_SIZE(vdo_axi_sels));
	clk[IMX6QDL_CLK_VPU_AXI_SEL]      = imx_clk_mux("vpu_axi_sel",      base + 0x18, 14, 2, vpu_axi_sels,      ARRAY_SIZE(vpu_axi_sels));
	clk[IMX6QDL_CLK_CKO1_SEL]         = imx_clk_mux("cko1_sel",         base + 0x60, 0,  4, cko1_sels,         ARRAY_SIZE(cko1_sels));
	clk[IMX6QDL_CLK_CKO2_SEL]         = imx_clk_mux("cko2_sel",         base + 0x60, 16, 5, cko2_sels,         ARRAY_SIZE(cko2_sels));
	clk[IMX6QDL_CLK_CKO]              = imx_clk_mux("cko",              base + 0x60, 8, 1,  cko_sels,          ARRAY_SIZE(cko_sels));

	/*                                          name         reg      shift width busy: reg, shift parent_names  num_parents */
	clk[IMX6QDL_CLK_PERIPH]  = imx_clk_busy_mux("periph",  base + 0x14, 25,  1,   base + 0x48, 5,  periph_sels,  ARRAY_SIZE(periph_sels));
	clk[IMX6QDL_CLK_PERIPH2] = imx_clk_busy_mux("periph2", base + 0x14, 26,  1,   base + 0x48, 3,  periph2_sels, ARRAY_SIZE(periph2_sels));

	/*                                                  name                parent_name          reg       shift width */
	clk[IMX6QDL_CLK_PERIPH_CLK2]      = imx_clk_divider("periph_clk2",      "periph_clk2_sel",   base + 0x14, 27, 3);
	clk[IMX6QDL_CLK_PERIPH2_CLK2]     = imx_clk_divider("periph2_clk2",     "periph2_clk2_sel",  base + 0x14, 0,  3);
	clk[IMX6QDL_CLK_IPG]              = imx_clk_divider("ipg",              "ahb",               base + 0x14, 8,  2);
	clk[IMX6QDL_CLK_ESAI_PRED]        = imx_clk_divider("esai_pred",        "esai_sel",          base + 0x28, 9,  3);
	clk[IMX6QDL_CLK_ESAI_PODF]        = imx_clk_divider("esai_podf",        "esai_pred",         base + 0x28, 25, 3);
	clk[IMX6QDL_CLK_ASRC_PRED]        = imx_clk_divider("asrc_pred",        "asrc_sel",          base + 0x30, 12, 3);
	clk[IMX6QDL_CLK_ASRC_PODF]        = imx_clk_divider("asrc_podf",        "asrc_pred",         base + 0x30, 9,  3);
	clk[IMX6QDL_CLK_SPDIF_PRED]       = imx_clk_divider("spdif_pred",       "spdif_sel",         base + 0x30, 25, 3);
	clk[IMX6QDL_CLK_SPDIF_PODF]       = imx_clk_divider("spdif_podf",       "spdif_pred",        base + 0x30, 22, 3);
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_IPG_PER] = imx_clk_divider("ipg_per", "ipg_per_sel", base + 0x1c, 0, 6);
		clk[IMX6QDL_CLK_ECSPI_ROOT] = imx_clk_divider("ecspi_root", "ecspi_sel", base + 0x38, 19, 6);
		clk[IMX6QDL_CLK_CAN_ROOT] = imx_clk_divider("can_root", "can_sel", base + 0x20, 2, 6);
		clk[IMX6QDL_CLK_UART_SERIAL_PODF] = imx_clk_divider("uart_serial_podf", "uart_sel", base + 0x24, 0, 6);
		clk[IMX6QDL_CLK_LDB_DI0_DIV_3_5] = imx_clk_fixed_factor("ldb_di0_div_3_5", "ldb_di0", 2, 7);
		clk[IMX6QDL_CLK_LDB_DI1_DIV_3_5] = imx_clk_fixed_factor("ldb_di1_div_3_5", "ldb_di1", 2, 7);
		clk[IMX6QDL_CLK_LDB_DI0_DIV_7] = imx_clk_fixed_factor("ldb_di0_div_7",   "ldb_di0", 1, 7);
		clk[IMX6QDL_CLK_LDB_DI1_DIV_7] = imx_clk_fixed_factor("ldb_di1_div_7",   "ldb_di1", 1, 7);
	} else {
		clk[IMX6QDL_CLK_ECSPI_ROOT] = imx_clk_divider("ecspi_root", "pll3_60m", base + 0x38, 19, 6);
		clk[IMX6QDL_CLK_CAN_ROOT] = imx_clk_divider("can_root", "pll3_60m", base + 0x20, 2, 6);
		clk[IMX6QDL_CLK_IPG_PER] = imx_clk_fixup_divider("ipg_per", "ipg", base + 0x1c, 0, 6, imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_UART_SERIAL_PODF] = imx_clk_divider("uart_serial_podf", "pll3_80m",          base + 0x24, 0,  6);
		clk[IMX6QDL_CLK_LDB_DI0_DIV_3_5] = imx_clk_fixed_factor("ldb_di0_div_3_5", "ldb_di0_sel", 2, 7);
		clk[IMX6QDL_CLK_LDB_DI1_DIV_3_5] = imx_clk_fixed_factor("ldb_di1_div_3_5", "ldb_di1_sel", 2, 7);
		clk[IMX6QDL_CLK_LDB_DI0_DIV_7]    = imx_clk_fixed_factor("ldb_di0_div_7",   "ldb_di0_sel", 1, 7);
		clk[IMX6QDL_CLK_LDB_DI1_DIV_7]    = imx_clk_fixed_factor("ldb_di1_div_7",   "ldb_di1_sel", 1, 7);
	}
	if (clk_on_imx6dl())
		clk[IMX6QDL_CLK_MLB_PODF]  = imx_clk_divider("mlb_podf",  "mlb_sel",    base + 0x18, 23, 3);
	else
		clk[IMX6QDL_CLK_GPU2D_CORE_PODF]  = imx_clk_divider("gpu2d_core_podf",  "gpu2d_core_sel",    base + 0x18, 23, 3);
	clk[IMX6QDL_CLK_GPU3D_CORE_PODF]  = imx_clk_divider("gpu3d_core_podf",  "gpu3d_core_sel",    base + 0x18, 26, 3);
	if (clk_on_imx6dl())
		clk[IMX6QDL_CLK_GPU2D_CORE_PODF]  = imx_clk_divider("gpu2d_core_podf",     "gpu2d_core_sel",  base + 0x18, 29, 3);
	else
		clk[IMX6QDL_CLK_GPU3D_SHADER]     = imx_clk_divider("gpu3d_shader",     "gpu3d_shader_sel",  base + 0x18, 29, 3);
	clk[IMX6QDL_CLK_IPU1_PODF]        = imx_clk_divider("ipu1_podf",        "ipu1_sel",          base + 0x3c, 11, 3);
	clk[IMX6QDL_CLK_IPU2_PODF]        = imx_clk_divider("ipu2_podf",        "ipu2_sel",          base + 0x3c, 16, 3);
	clk[IMX6QDL_CLK_LDB_DI0_PODF]     = imx_clk_divider_flags("ldb_di0_podf", "ldb_di0_div_3_5", base + 0x20, 10, 1, 0);
	clk[IMX6QDL_CLK_LDB_DI1_PODF]     = imx_clk_divider_flags("ldb_di1_podf", "ldb_di1_div_3_5", base + 0x20, 11, 1, 0);
	clk[IMX6QDL_CLK_IPU1_DI0_PRE]     = imx_clk_divider("ipu1_di0_pre",     "ipu1_di0_pre_sel",  base + 0x34, 3,  3);
	clk[IMX6QDL_CLK_IPU1_DI1_PRE]     = imx_clk_divider("ipu1_di1_pre",     "ipu1_di1_pre_sel",  base + 0x34, 12, 3);
	clk[IMX6QDL_CLK_IPU2_DI0_PRE]     = imx_clk_divider("ipu2_di0_pre",     "ipu2_di0_pre_sel",  base + 0x38, 3,  3);
	clk[IMX6QDL_CLK_IPU2_DI1_PRE]     = imx_clk_divider("ipu2_di1_pre",     "ipu2_di1_pre_sel",  base + 0x38, 12, 3);
	clk[IMX6QDL_CLK_HSI_TX_PODF]      = imx_clk_divider("hsi_tx_podf",      "hsi_tx_sel",        base + 0x30, 29, 3);
	clk[IMX6QDL_CLK_SSI1_PRED]        = imx_clk_divider("ssi1_pred",        "ssi1_sel",          base + 0x28, 6,  3);
	clk[IMX6QDL_CLK_SSI1_PODF]        = imx_clk_divider("ssi1_podf",        "ssi1_pred",         base + 0x28, 0,  6);
	clk[IMX6QDL_CLK_SSI2_PRED]        = imx_clk_divider("ssi2_pred",        "ssi2_sel",          base + 0x2c, 6,  3);
	clk[IMX6QDL_CLK_SSI2_PODF]        = imx_clk_divider("ssi2_podf",        "ssi2_pred",         base + 0x2c, 0,  6);
	clk[IMX6QDL_CLK_SSI3_PRED]        = imx_clk_divider("ssi3_pred",        "ssi3_sel",          base + 0x28, 22, 3);
	clk[IMX6QDL_CLK_SSI3_PODF]        = imx_clk_divider("ssi3_podf",        "ssi3_pred",         base + 0x28, 16, 6);
	clk[IMX6QDL_CLK_USDHC1_PODF]      = imx_clk_divider("usdhc1_podf",      "usdhc1_sel",        base + 0x24, 11, 3);
	clk[IMX6QDL_CLK_USDHC2_PODF]      = imx_clk_divider("usdhc2_podf",      "usdhc2_sel",        base + 0x24, 16, 3);
	clk[IMX6QDL_CLK_USDHC3_PODF]      = imx_clk_divider("usdhc3_podf",      "usdhc3_sel",        base + 0x24, 19, 3);
	clk[IMX6QDL_CLK_USDHC4_PODF]      = imx_clk_divider("usdhc4_podf",      "usdhc4_sel",        base + 0x24, 22, 3);
	clk[IMX6QDL_CLK_ENFC_PRED]        = imx_clk_divider("enfc_pred",        "enfc_sel",          base + 0x2c, 18, 3);
	clk[IMX6QDL_CLK_ENFC_PODF]        = imx_clk_divider("enfc_podf",        "enfc_pred",         base + 0x2c, 21, 6);
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_EIM_PODF]         = imx_clk_divider("eim_podf",   "eim_sel",           base + 0x1c, 20, 3);
		clk[IMX6QDL_CLK_EIM_SLOW_PODF]    = imx_clk_divider("eim_slow_podf", "eim_slow_sel",   base + 0x1c, 23, 3);
	} else {
		clk[IMX6QDL_CLK_EIM_PODF]         = imx_clk_fixup_divider("eim_podf",   "eim_sel",           base + 0x1c, 20, 3, imx_cscmr1_fixup);
		clk[IMX6QDL_CLK_EIM_SLOW_PODF]    = imx_clk_fixup_divider("eim_slow_podf", "eim_slow_sel",   base + 0x1c, 23, 3, imx_cscmr1_fixup);
	}
	clk[IMX6QDL_CLK_VPU_AXI_PODF]     = imx_clk_divider("vpu_axi_podf",     "vpu_axi_sel",       base + 0x24, 25, 3);
	clk[IMX6QDL_CLK_CKO1_PODF]        = imx_clk_divider("cko1_podf",        "cko1_sel",          base + 0x60, 4,  3);
	clk[IMX6QDL_CLK_CKO2_PODF]        = imx_clk_divider("cko2_podf",        "cko2_sel",          base + 0x60, 21, 3);

	/*                                                        name                 parent_name    reg        shift width busy: reg, shift */
	clk[IMX6QDL_CLK_AXI]               = imx_clk_busy_divider("axi",               "axi_sel",     base + 0x14, 16,  3,   base + 0x48, 0);
	clk[IMX6QDL_CLK_MMDC_CH0_AXI_PODF] = imx_clk_busy_divider("mmdc_ch0_axi_podf", "periph",      base + 0x14, 19,  3,   base + 0x48, 4);
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_MMDC_CH1_AXI_CG] = imx_clk_gate("mmdc_ch1_axi_cg", "periph2", base + 0x4, 18);
		clk[IMX6QDL_CLK_MMDC_CH1_AXI_PODF] = imx_clk_busy_divider("mmdc_ch1_axi_podf", "mmdc_ch1_axi_cg", base + 0x14, 3, 3, base + 0x48, 2);
	} else {
		clk[IMX6QDL_CLK_MMDC_CH1_AXI_PODF] = imx_clk_busy_divider("mmdc_ch1_axi_podf", "periph2",     base + 0x14, 3,   3,   base + 0x48, 2);
	}
	clk[IMX6QDL_CLK_ARM]               = imx_clk_busy_divider("arm",               "pll1_sw",     base + 0x10, 0,   3,   base + 0x48, 16);
	clk[IMX6QDL_CLK_AHB]               = imx_clk_busy_divider("ahb",               "periph",      base + 0x14, 10,  3,   base + 0x48, 1);

	/*                                            name             parent_name          reg         shift */
	clk[IMX6QDL_CLK_APBH_DMA]     = imx_clk_gate2("apbh_dma",      "usdhc3",            base + 0x68, 4);
	clk[IMX6QDL_CLK_ASRC]         = imx_clk_gate2_shared("asrc",         "asrc_podf",   base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_ASRC_IPG]     = imx_clk_gate2_shared("asrc_ipg",     "ahb",         base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_ASRC_MEM]     = imx_clk_gate2_shared("asrc_mem",     "ahb",         base + 0x68, 6, &share_count_asrc);
	clk[IMX6QDL_CLK_CAAM_MEM]     = imx_clk_gate2("caam_mem",      "ahb",               base + 0x68, 8);
	clk[IMX6QDL_CLK_CAAM_ACLK]    = imx_clk_gate2("caam_aclk",     "ahb",               base + 0x68, 10);
	clk[IMX6QDL_CLK_CAAM_IPG]     = imx_clk_gate2("caam_ipg",      "ipg",               base + 0x68, 12);
	clk[IMX6QDL_CLK_CAN1_IPG]     = imx_clk_gate2("can1_ipg",      "ipg",               base + 0x68, 14);
	clk[IMX6QDL_CLK_CAN1_SERIAL]  = imx_clk_gate2("can1_serial",   "can_root",          base + 0x68, 16);
	clk[IMX6QDL_CLK_CAN2_IPG]     = imx_clk_gate2("can2_ipg",      "ipg",               base + 0x68, 18);
	clk[IMX6QDL_CLK_CAN2_SERIAL]  = imx_clk_gate2("can2_serial",   "can_root",          base + 0x68, 20);
	clk[IMX6QDL_CLK_DCIC1]        = imx_clk_gate2("dcic1",         "ipu1_podf",         base + 0x68, 24);
	clk[IMX6QDL_CLK_DCIC2]        = imx_clk_gate2("dcic2",         "ipu2_podf",         base + 0x68, 26);
	clk[IMX6QDL_CLK_ECSPI1]       = imx_clk_gate2("ecspi1",        "ecspi_root",        base + 0x6c, 0);
	clk[IMX6QDL_CLK_ECSPI2]       = imx_clk_gate2("ecspi2",        "ecspi_root",        base + 0x6c, 2);
	clk[IMX6QDL_CLK_ECSPI3]       = imx_clk_gate2("ecspi3",        "ecspi_root",        base + 0x6c, 4);
	clk[IMX6QDL_CLK_ECSPI4]       = imx_clk_gate2("ecspi4",        "ecspi_root",        base + 0x6c, 6);
	if (clk_on_imx6dl())
		clk[IMX6DL_CLK_I2C4]  = imx_clk_gate2("i2c4",          "ipg_per",           base + 0x6c, 8);
	else
		clk[IMX6Q_CLK_ECSPI5] = imx_clk_gate2("ecspi5",        "ecspi_root",        base + 0x6c, 8);
	clk[IMX6QDL_CLK_ENET]         = imx_clk_gate2("enet",          "ipg",               base + 0x6c, 10);
	clk[IMX6QDL_CLK_ESAI_EXTAL]   = imx_clk_gate2_shared("esai_extal",   "esai_podf",   base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_ESAI_IPG]     = imx_clk_gate2_shared("esai_ipg",   "ahb",           base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_ESAI_MEM]     = imx_clk_gate2_shared("esai_mem", "ahb",             base + 0x6c, 16, &share_count_esai);
	clk[IMX6QDL_CLK_GPT_IPG]      = imx_clk_gate2("gpt_ipg",       "ipg",               base + 0x6c, 20);
	clk[IMX6QDL_CLK_GPT_IPG_PER]  = imx_clk_gate2("gpt_ipg_per",   "ipg_per",           base + 0x6c, 22);
	clk[IMX6QDL_CLK_GPU2D_CORE] = imx_clk_gate2("gpu2d_core", "gpu2d_core_podf", base + 0x6c, 24);
	clk[IMX6QDL_CLK_GPU3D_CORE]   = imx_clk_gate2("gpu3d_core",    "gpu3d_core_podf",   base + 0x6c, 26);
	clk[IMX6QDL_CLK_HDMI_IAHB]    = imx_clk_gate2("hdmi_iahb",     "ahb",               base + 0x70, 0);
	clk[IMX6QDL_CLK_HDMI_ISFR]    = imx_clk_gate2("hdmi_isfr",     "pll3_pfd1_540m",    base + 0x70, 4);
	clk[IMX6QDL_CLK_I2C1]         = imx_clk_gate2("i2c1",          "ipg_per",           base + 0x70, 6);
	clk[IMX6QDL_CLK_I2C2]         = imx_clk_gate2("i2c2",          "ipg_per",           base + 0x70, 8);
	clk[IMX6QDL_CLK_I2C3]         = imx_clk_gate2("i2c3",          "ipg_per",           base + 0x70, 10);
	clk[IMX6QDL_CLK_IIM]          = imx_clk_gate2("iim",           "ipg",               base + 0x70, 12);
	clk[IMX6QDL_CLK_ENFC]         = imx_clk_gate2("enfc",          "enfc_podf",         base + 0x70, 14);
	clk[IMX6QDL_CLK_VDOA]         = imx_clk_gate2("vdoa",          "vdo_axi",           base + 0x70, 26);
	clk[IMX6QDL_CLK_IPU1]         = imx_clk_gate2("ipu1",          "ipu1_podf",         base + 0x74, 0);
	clk[IMX6QDL_CLK_IPU1_DI0]     = imx_clk_gate2("ipu1_di0",      "ipu1_di0_sel",      base + 0x74, 2);
	clk[IMX6QDL_CLK_IPU1_DI1]     = imx_clk_gate2("ipu1_di1",      "ipu1_di1_sel",      base + 0x74, 4);
	clk[IMX6QDL_CLK_IPU2]         = imx_clk_gate2("ipu2",          "ipu2_podf",         base + 0x74, 6);
	clk[IMX6QDL_CLK_IPU2_DI0]     = imx_clk_gate2("ipu2_di0",      "ipu2_di0_sel",      base + 0x74, 8);
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_LDB_DI0]      = imx_clk_gate2("ldb_di0",       "ldb_di0_sel",      base + 0x74, 12);
		clk[IMX6QDL_CLK_LDB_DI1]      = imx_clk_gate2("ldb_di1",       "ldb_di1_sel",      base + 0x74, 14);
	} else {
		clk[IMX6QDL_CLK_LDB_DI0]      = imx_clk_gate2("ldb_di0",       "ldb_di0_podf",      base + 0x74, 12);
		clk[IMX6QDL_CLK_LDB_DI1]      = imx_clk_gate2("ldb_di1",       "ldb_di1_podf",      base + 0x74, 14);
	}
	clk[IMX6QDL_CLK_IPU2_DI1]     = imx_clk_gate2("ipu2_di1",      "ipu2_di1_sel",      base + 0x74, 10);
	clk[IMX6QDL_CLK_HSI_TX]       = imx_clk_gate2_shared("hsi_tx", "hsi_tx_podf",       base + 0x74, 16, &share_count_mipi_core_cfg);
	clk[IMX6QDL_CLK_MIPI_CORE_CFG] = imx_clk_gate2_shared("mipi_core_cfg", "video_27m", base + 0x74, 16, &share_count_mipi_core_cfg);
	clk[IMX6QDL_CLK_MIPI_IPG]     = imx_clk_gate2_shared("mipi_ipg", "ipg",             base + 0x74, 16, &share_count_mipi_core_cfg);
	if (clk_on_imx6dl())
		/*
		 * The multiplexer and divider of the imx6q clock gpu2d get
		 * redefined/reused as mlb_sys_sel and mlb_sys_clk_podf on imx6dl.
		 */
		clk[IMX6QDL_CLK_MLB] = imx_clk_gate2("mlb",            "mlb_podf",   base + 0x74, 18);
	else
		clk[IMX6QDL_CLK_MLB] = imx_clk_gate2("mlb",            "axi",               base + 0x74, 18);
	clk[IMX6QDL_CLK_MMDC_CH0_AXI] = imx_clk_gate2("mmdc_ch0_axi",  "mmdc_ch0_axi_podf", base + 0x74, 20);
	clk[IMX6QDL_CLK_MMDC_CH1_AXI] = imx_clk_gate2("mmdc_ch1_axi",  "mmdc_ch1_axi_podf", base + 0x74, 22);
	clk[IMX6QDL_CLK_OCRAM]        = imx_clk_gate2("ocram",         "ahb",               base + 0x74, 28);
	clk[IMX6QDL_CLK_OPENVG_AXI]   = imx_clk_gate2("openvg_axi",    "axi",               base + 0x74, 30);
	clk[IMX6QDL_CLK_PCIE_AXI]     = imx_clk_gate2("pcie_axi",      "pcie_axi_sel",      base + 0x78, 0);
	clk[IMX6QDL_CLK_PER1_BCH]     = imx_clk_gate2("per1_bch",      "usdhc3",            base + 0x78, 12);
	clk[IMX6QDL_CLK_PWM1]         = imx_clk_gate2("pwm1",          "ipg_per",           base + 0x78, 16);
	clk[IMX6QDL_CLK_PWM2]         = imx_clk_gate2("pwm2",          "ipg_per",           base + 0x78, 18);
	clk[IMX6QDL_CLK_PWM3]         = imx_clk_gate2("pwm3",          "ipg_per",           base + 0x78, 20);
	clk[IMX6QDL_CLK_PWM4]         = imx_clk_gate2("pwm4",          "ipg_per",           base + 0x78, 22);
	clk[IMX6QDL_CLK_GPMI_BCH_APB] = imx_clk_gate2("gpmi_bch_apb",  "usdhc3",            base + 0x78, 24);
	clk[IMX6QDL_CLK_GPMI_BCH]     = imx_clk_gate2("gpmi_bch",      "usdhc4",            base + 0x78, 26);
	clk[IMX6QDL_CLK_GPMI_IO]      = imx_clk_gate2("gpmi_io",       "enfc",              base + 0x78, 28);
	clk[IMX6QDL_CLK_GPMI_APB]     = imx_clk_gate2("gpmi_apb",      "usdhc3",            base + 0x78, 30);
	clk[IMX6QDL_CLK_ROM]          = imx_clk_gate2("rom",           "ahb",               base + 0x7c, 0);
	clk[IMX6QDL_CLK_SATA]         = imx_clk_gate2("sata",          "ahb",               base + 0x7c, 4);
	clk[IMX6QDL_CLK_SDMA]         = imx_clk_gate2("sdma",          "ahb",               base + 0x7c, 6);
	clk[IMX6QDL_CLK_SPBA]         = imx_clk_gate2("spba",          "ipg",               base + 0x7c, 12);
	clk[IMX6QDL_CLK_SPDIF]        = imx_clk_gate2_shared("spdif",     "spdif_podf",     base + 0x7c, 14, &share_count_spdif);
	clk[IMX6QDL_CLK_SPDIF_GCLK]   = imx_clk_gate2_shared("spdif_gclk", "ipg",           base + 0x7c, 14, &share_count_spdif);
	clk[IMX6QDL_CLK_SSI1_IPG]     = imx_clk_gate2_shared("ssi1_ipg",      "ipg",        base + 0x7c, 18, &share_count_ssi1);
	clk[IMX6QDL_CLK_SSI2_IPG]     = imx_clk_gate2_shared("ssi2_ipg",      "ipg",        base + 0x7c, 20, &share_count_ssi2);
	clk[IMX6QDL_CLK_SSI3_IPG]     = imx_clk_gate2_shared("ssi3_ipg",      "ipg",        base + 0x7c, 22, &share_count_ssi3);
	clk[IMX6QDL_CLK_SSI1]         = imx_clk_gate2_shared("ssi1",          "ssi1_podf",  base + 0x7c, 18, &share_count_ssi1);
	clk[IMX6QDL_CLK_SSI2]         = imx_clk_gate2_shared("ssi2",          "ssi2_podf",  base + 0x7c, 20, &share_count_ssi2);
	clk[IMX6QDL_CLK_SSI3]         = imx_clk_gate2_shared("ssi3",          "ssi3_podf",  base + 0x7c, 22, &share_count_ssi3);
	clk[IMX6QDL_CLK_UART_IPG]     = imx_clk_gate2("uart_ipg",      "ipg",               base + 0x7c, 24);
	clk[IMX6QDL_CLK_UART_SERIAL]  = imx_clk_gate2("uart_serial",   "uart_serial_podf",  base + 0x7c, 26);
	clk[IMX6QDL_CLK_USBOH3]       = imx_clk_gate2("usboh3",        "ipg",               base + 0x80, 0);
	clk[IMX6QDL_CLK_USDHC1]       = imx_clk_gate2("usdhc1",        "usdhc1_podf",       base + 0x80, 2);
	clk[IMX6QDL_CLK_USDHC2]       = imx_clk_gate2("usdhc2",        "usdhc2_podf",       base + 0x80, 4);
	clk[IMX6QDL_CLK_USDHC3]       = imx_clk_gate2("usdhc3",        "usdhc3_podf",       base + 0x80, 6);
	clk[IMX6QDL_CLK_USDHC4]       = imx_clk_gate2("usdhc4",        "usdhc4_podf",       base + 0x80, 8);
	clk[IMX6QDL_CLK_EIM_SLOW]     = imx_clk_gate2("eim_slow",      "eim_slow_podf",     base + 0x80, 10);
	clk[IMX6QDL_CLK_VDO_AXI]      = imx_clk_gate2("vdo_axi",       "vdo_axi_sel",       base + 0x80, 12);
	clk[IMX6QDL_CLK_VPU_AXI]      = imx_clk_gate2("vpu_axi",       "vpu_axi_podf",      base + 0x80, 14);
	if (clk_on_imx6qp()) {
		clk[IMX6QDL_CLK_PRE0] = imx_clk_gate2("pre0",	       "pre_axi",	    base + 0x80, 16);
		clk[IMX6QDL_CLK_PRE1] = imx_clk_gate2("pre1",	       "pre_axi",	    base + 0x80, 18);
		clk[IMX6QDL_CLK_PRE2] = imx_clk_gate2("pre2",	       "pre_axi",         base + 0x80, 20);
		clk[IMX6QDL_CLK_PRE3] = imx_clk_gate2("pre3",	       "pre_axi",	    base + 0x80, 22);
		clk[IMX6QDL_CLK_PRG0_AXI] = imx_clk_gate2_shared("prg0_axi",  "ipu1_podf",  base + 0x80, 24, &share_count_prg0);
		clk[IMX6QDL_CLK_PRG1_AXI] = imx_clk_gate2_shared("prg1_axi",  "ipu2_podf",  base + 0x80, 26, &share_count_prg1);
		clk[IMX6QDL_CLK_PRG0_APB] = imx_clk_gate2_shared("prg0_apb",  "ipg",	    base + 0x80, 24, &share_count_prg0);
		clk[IMX6QDL_CLK_PRG1_APB] = imx_clk_gate2_shared("prg1_apb",  "ipg",	    base + 0x80, 26, &share_count_prg1);
	}
	clk[IMX6QDL_CLK_CKO1]         = imx_clk_gate("cko1",           "cko1_podf",         base + 0x60, 7);
	clk[IMX6QDL_CLK_CKO2]         = imx_clk_gate("cko2",           "cko2_podf",         base + 0x60, 24);

	/*
	 * The gpt_3m clock is not available on i.MX6Q TO1.0.  Let's point it
	 * to clock gpt_ipg_per to ease the gpt driver code.
	 */
	if (clk_on_imx6q() && imx_get_soc_revision() == IMX_CHIP_REVISION_1_0)
		clk[IMX6QDL_CLK_GPT_3M] = clk[IMX6QDL_CLK_GPT_IPG_PER];

	imx_check_clocks(clk, ARRAY_SIZE(clk));

	clk_data.clks = clk;
	clk_data.clk_num = ARRAY_SIZE(clk);
	of_clk_add_provider(np, of_clk_src_onecell_get, &clk_data);

	clk_register_clkdev(clk[IMX6QDL_CLK_GPT_IPG], "ipg", "imx-gpt.0");
	clk_register_clkdev(clk[IMX6QDL_CLK_GPT_IPG_PER], "per", "imx-gpt.0");
	clk_register_clkdev(clk[IMX6QDL_CLK_GPT_3M], "gpt_3m", "imx-gpt.0");
	clk_register_clkdev(clk[IMX6QDL_CLK_ENET_REF], "enet_ref", NULL);

	clk_set_rate(clk[IMX6QDL_CLK_PLL3_PFD1_540M], 540000000);
	if (clk_on_imx6dl()) {
		init_ipu_clk(anatop_base);
		clk_set_parent(clk[IMX6QDL_CLK_IPU1_SEL], clk[IMX6QDL_CLK_PLL3_PFD1_540M]);
		clk_set_parent(clk[IMX6QDL_CLK_AXI_ALT_SEL], clk[IMX6QDL_CLK_PLL3_PFD1_540M]);
		clk_set_parent(clk[IMX6QDL_CLK_AXI_SEL], clk[IMX6QDL_CLK_AXI_ALT_SEL]);
		/* set epdc/pxp axi clock to 200Mhz */
		clk_set_parent(clk[IMX6QDL_CLK_IPU2_SEL], clk[IMX6QDL_CLK_PLL2_PFD2_396M]);
		clk_set_rate(clk[IMX6QDL_CLK_IPU2], 200000000);
	} else {
		/* set eim_slow to 132Mhz for i.MX6Q */
		if (clk_on_imx6q())
			clk_set_rate(clk[IMX6QDL_CLK_EIM_SLOW], 132000000);
		clk_set_parent(clk[IMX6QDL_CLK_IPU1_SEL], clk[IMX6QDL_CLK_MMDC_CH0_AXI]);
		clk_set_parent(clk[IMX6QDL_CLK_IPU2_SEL], clk[IMX6QDL_CLK_MMDC_CH0_AXI]);
	}

	clk_set_parent(clk[IMX6QDL_CLK_IPU1_DI0_PRE_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU1_DI1_PRE_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU2_DI0_PRE_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU2_DI1_PRE_SEL], clk[IMX6QDL_CLK_PLL5_VIDEO_DIV]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU1_DI0_SEL], clk[IMX6QDL_CLK_IPU1_DI0_PRE]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU1_DI1_SEL], clk[IMX6QDL_CLK_IPU1_DI1_PRE]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU2_DI0_SEL], clk[IMX6QDL_CLK_IPU2_DI0_PRE]);
	clk_set_parent(clk[IMX6QDL_CLK_IPU2_DI1_SEL], clk[IMX6QDL_CLK_IPU2_DI1_PRE]);


	/*
	 * The gpmi needs 100MHz frequency in the EDO/Sync mode,
	 * We can not get the 100MHz from the pll2_pfd0_352m.
	 * So choose pll2_pfd2_396m as enfc_sel's parent.
	 */
	clk_set_parent(clk[IMX6QDL_CLK_ENFC_SEL], clk[IMX6QDL_CLK_PLL2_PFD2_396M]);

	/* gpu clock initilazation */
	/*
	* On mx6dl, 2d core clock sources(sel, podf) is from 3d
	* shader core clock, but 3d shader clock multiplexer of
	* mx6dl is different. For instance the equivalent of
	* pll2_pfd_594M on mx6q is pll2_pfd_528M on mx6dl.
	* Make a note here.
	*/
	if (clk_on_imx6dl()) {
		clk_set_parent(clk[IMX6QDL_CLK_GPU3D_SHADER_SEL], clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
		imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_SHADER], 528000000);
		/* for mx6dl, change gpu3d_core parent to 594_PFD*/
		clk_set_parent(clk[IMX6QDL_CLK_GPU3D_CORE_SEL], clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
		imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_CORE], 528000000);
	} else if (clk_on_imx6q()) {
		if (imx_get_soc_revision() >= IMX_CHIP_REVISION_2_0) {
			clk_set_parent(clk[IMX6QDL_CLK_GPU3D_SHADER_SEL], clk[IMX6QDL_CLK_PLL3_PFD0_720M]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_SHADER], 720000000);
			clk_set_parent(clk[IMX6QDL_CLK_GPU3D_CORE_SEL], clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_CORE], 594000000);
			clk_set_parent(clk[IMX6QDL_CLK_GPU2D_CORE_SEL], clk[IMX6QDL_CLK_PLL3_PFD0_720M]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU2D_CORE], 720000000);
		} else {
			clk_set_parent(clk[IMX6QDL_CLK_GPU3D_SHADER_SEL], clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_SHADER], 594000000);
			clk_set_parent(clk[IMX6QDL_CLK_GPU3D_CORE_SEL], clk[IMX6QDL_CLK_MMDC_CH0_AXI]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU3D_CORE], 528000000);
			clk_set_parent(clk[IMX6QDL_CLK_GPU2D_CORE_SEL], clk[IMX6QDL_CLK_PLL3_USB_OTG]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_GPU2D_CORE], 480000000);
		}
	}

	/*
	 * Let's initially set up CLKO with OSC24M, since this configuration
	 * is widely used by imx6q board designs to clock audio codec.
	 */
	imx_clk_set_parent(clk[IMX6QDL_CLK_CKO2_SEL], clk[IMX6QDL_CLK_OSC]);
	imx_clk_set_parent(clk[IMX6QDL_CLK_CKO], clk[IMX6QDL_CLK_CKO2]);

	/* Audio-related clocks configuration */
	clk_set_parent(clk[IMX6QDL_CLK_SPDIF_SEL], clk[IMX6QDL_CLK_PLL3_PFD3_454M]);

	/* All existing boards with PCIe use LVDS1 */
	if (IS_ENABLED(CONFIG_PCI_IMX6)) {
		clk_set_parent(clk[IMX6QDL_CLK_LVDS1_SEL], clk[IMX6QDL_CLK_SATA_REF_100M]);
		np = of_find_compatible_node(NULL, NULL, "snps,dw-pcie");
		 /* external oscillator is used or not. */
		if (of_property_read_u32(np, "ext_osc", &val) < 0)
			val = 0;
		/*
		 * imx6qp sabresd revb board has the external osc used by pcie
		 * - pll6 should be set bypass mode later in driver.
		 * - lvds_clk1 should be selected as pll6 bypass src, set here.
		 */
		if (clk_on_imx6qp() && val == 1)
			clk_set_parent(clk[IMX6QDL_PLL6_BYPASS_SRC], clk[IMX6QDL_CLK_LVDS1_IN]);
	}

	/*
	 * Enable clocks only after both parent and rate are all initialized
	 * as needed
	 */
	for (i = 0; i < ARRAY_SIZE(clks_init_on); i++)
		imx_clk_prepare_enable(clk[clks_init_on[i]]);

	if (IS_ENABLED(CONFIG_USB_MXS_PHY)) {
		imx_clk_prepare_enable(clk[IMX6QDL_CLK_USBPHY1_GATE]);
		imx_clk_prepare_enable(clk[IMX6QDL_CLK_USBPHY2_GATE]);
	}

	/*Set enet_ref clock to 125M to supply for RGMII tx_clk */
	clk_set_rate(clk[IMX6QDL_CLK_ENET_REF], 125000000);

#ifdef CONFIG_MX6_VPU_352M
	/*
	 * If VPU 352M is enabled, then PLL2_PDF2 need to be
	 * set to 352M, cpufreq will be disabled as VDDSOC/PU
	 * need to be at highest voltage, scaling cpu freq is
	 * not saving any power, and busfreq will be also disabled
	 * as the PLL2_PFD2 is not at default freq, in a word,
	 * all modules that sourceing clk from PLL2_PFD2 will
	 * be impacted.
	 */
	imx_clk_set_rate(clk[IMX6QDL_CLK_PLL2_PFD2_396M], 352000000);
	clk_set_parent(clk[IMX6QDL_CLK_VPU_AXI_SEL], clk[IMX6QDL_CLK_PLL2_PFD2_396M]);
	pr_info("VPU 352M is enabled!\n");
#endif

	/*
	 * Initialize the GPU clock muxes, so that the maximum specified clock
	 * rates for the respective SoC are not exceeded.
	 */
	if (clk_on_imx6dl()) {
		clk_set_parent(clk[IMX6QDL_CLK_GPU3D_CORE_SEL],
			       clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
		clk_set_parent(clk[IMX6QDL_CLK_GPU2D_CORE_SEL],
			       clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
	} else if (clk_on_imx6q()) {
		clk_set_parent(clk[IMX6QDL_CLK_GPU3D_CORE_SEL],
			       clk[IMX6QDL_CLK_MMDC_CH0_AXI]);
		clk_set_parent(clk[IMX6QDL_CLK_GPU3D_SHADER_SEL],
			       clk[IMX6QDL_CLK_PLL2_PFD1_594M]);
		clk_set_parent(clk[IMX6QDL_CLK_GPU2D_CORE_SEL],
			       clk[IMX6QDL_CLK_PLL3_USB_OTG]);
	}

	imx_register_uart_clocks(uart_clks);

	/*
	 * for i.MX6QP with speeding grading set to 1.2GHz,
	 * VPU should run at 396MHz.
	 */
	if (clk_on_imx6q() && imx_get_soc_revision() >= IMX_CHIP_REVISION_2_0) {
		np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-ocotp");
		WARN_ON(!np);

		base = of_iomap(np, 0);
		WARN_ON(!base);

		/*
		 * SPEED_GRADING[1:0] defines the max speed of ARM:
		 * 2b'11: 1200000000Hz;
		 * 2b'10: 996000000Hz;
		 * 2b'01: 852000000Hz; -- i.MX6Q Only, exclusive with 996MHz.
		 * 2b'00: 792000000Hz;
		 * We need to set the max speed of ARM according to fuse map.
		 */
		val = readl_relaxed(base + OCOTP_CFG3);
		val >>= OCOTP_CFG3_SPEED_SHIFT;
		val &= 0x3;
		if (val == OCOTP_CFG3_SPEED_1P2GHZ) {
			imx_clk_set_parent(clk[IMX6QDL_CLK_VPU_AXI_SEL], clk[IMX6QDL_CLK_PLL2_PFD2_396M]);
			imx_clk_set_rate(clk[IMX6QDL_CLK_VPU_AXI_PODF], 396000000);
			pr_info("VPU frequency set to 396MHz!\n");
		}
		iounmap(base);
	}
}
CLK_OF_DECLARE(imx6q, "fsl,imx6q-ccm", imx6q_clocks_init);
