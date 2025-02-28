/* * CAAM control-plane driver backend
 * Controller-level driver, kernel property detection, initialization
 *
 * Copyright 2008-2016 Freescale Semiconductor, Inc.
 * Copyright 2017 NXP
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include "compat.h"
#include "regs.h"
#include "intern.h"
#include "jr.h"
#include "desc_constr.h"
#include "error.h"
#include "ctrl.h"
#include "sm.h"

bool caam_little_end;
EXPORT_SYMBOL(caam_little_end);

/*
 * i.MX targets tend to have clock control subsystems that can
 * enable/disable clocking to our device.
 */
#ifdef CONFIG_CRYPTO_DEV_FSL_CAAM_IMX
static inline struct clk *caam_drv_identify_clk(struct device *dev,
						char *clk_name)
{
	return devm_clk_get(dev, clk_name);
}
#else
static inline struct clk *caam_drv_identify_clk(struct device *dev,
						char *clk_name)
{
	return NULL;
}
#endif

/*
 * Descriptor to instantiate RNG State Handle 0 in normal mode and
 * load the JDKEK, TDKEK and TDSK registers
 */
static void build_instantiation_desc(u32 *desc, int handle, int do_sk)
{
	u32 *jump_cmd, op_flags;

	init_job_desc(desc, 0);

	op_flags = OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
			(handle << OP_ALG_AAI_SHIFT) | OP_ALG_AS_INIT;

	/* INIT RNG in non-test mode */
	append_operation(desc, op_flags);

	if (!handle && do_sk) {
		/*
		 * For SH0, Secure Keys must be generated as well
		 */

		/* wait for done */
		jump_cmd = append_jump(desc, JUMP_CLASS_CLASS1);
		set_jump_tgt_here(desc, jump_cmd);

		/*
		 * load 1 to clear written reg:
		 * resets the done interrrupt and returns the RNG to idle.
		 */
		append_load_imm_u32(desc, 1, LDST_SRCDST_WORD_CLRW);

		/* Initialize State Handle  */
		append_operation(desc, OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
				 OP_ALG_AAI_RNG4_SK);
	}

	append_jump(desc, JUMP_CLASS_CLASS1 | JUMP_TYPE_HALT);
}

/* Descriptor for deinstantiation of State Handle 0 of the RNG block. */
static void build_deinstantiation_desc(u32 *desc, int handle)
{
	init_job_desc(desc, 0);

	/* Uninstantiate State Handle 0 */
	append_operation(desc, OP_TYPE_CLASS1_ALG | OP_ALG_ALGSEL_RNG |
			 (handle << OP_ALG_AAI_SHIFT) | OP_ALG_AS_INITFINAL);

	append_jump(desc, JUMP_CLASS_CLASS1 | JUMP_TYPE_HALT);
}

/*
 * run_descriptor_deco0 - runs a descriptor on DECO0, under direct control of
 *			  the software (no JR/QI used).
 * @ctrldev - pointer to device
 * @status - descriptor status, after being run
 *
 * Return: - 0 if no error occurred
 *	   - -ENODEV if the DECO couldn't be acquired
 *	   - -EAGAIN if an error occurred while executing the descriptor
 */
static inline int run_descriptor_deco0(struct device *ctrldev, u32 *desc,
					u32 *status)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	struct caam_ctrl __iomem *ctrl = ctrlpriv->ctrl;
	struct caam_deco __iomem *deco = ctrlpriv->deco;
	unsigned int timeout = 100000;
	u32 deco_dbg_reg, flags;
	int i;


	if (ctrlpriv->virt_en == 1) {
		clrsetbits_32(&ctrl->deco_rsr, 0, DECORSR_JR0);

		while (!(rd_reg32(&ctrl->deco_rsr) & DECORSR_VALID) &&
		       --timeout)
			cpu_relax();

		timeout = 100000;
	}

	clrsetbits_32(&ctrl->deco_rq, 0, DECORR_RQD0ENABLE);

	while (!(rd_reg32(&ctrl->deco_rq) & DECORR_DEN0) &&
								 --timeout)
		cpu_relax();

	if (!timeout) {
		dev_err(ctrldev, "failed to acquire DECO 0\n");
		clrsetbits_32(&ctrl->deco_rq, DECORR_RQD0ENABLE, 0);
		return -ENODEV;
	}

	for (i = 0; i < desc_len(desc); i++)
		wr_reg32(&deco->descbuf[i], caam32_to_cpu(*(desc + i)));

	flags = DECO_JQCR_WHL;
	/*
	 * If the descriptor length is longer than 4 words, then the
	 * FOUR bit in JRCTRL register must be set.
	 */
	if (desc_len(desc) >= 4)
		flags |= DECO_JQCR_FOUR;

	/* Instruct the DECO to execute it */
	clrsetbits_32(&deco->jr_ctl_hi, 0, flags);

	timeout = 10000000;
	do {
		deco_dbg_reg = rd_reg32(&deco->desc_dbg);
		/*
		 * If an error occured in the descriptor, then
		 * the DECO status field will be set to 0x0D
		 */
		if ((deco_dbg_reg & DESC_DBG_DECO_STAT_MASK) ==
		    DESC_DBG_DECO_STAT_HOST_ERR)
			break;
		cpu_relax();
	} while ((deco_dbg_reg & DESC_DBG_DECO_STAT_VALID) && --timeout);

	*status = rd_reg32(&deco->op_status_hi) &
		  DECO_OP_STATUS_HI_ERR_MASK;

	if (ctrlpriv->virt_en == 1)
		clrsetbits_32(&ctrl->deco_rsr, DECORSR_JR0, 0);

	/* Mark the DECO as free */
	clrsetbits_32(&ctrl->deco_rq, DECORR_RQD0ENABLE, 0);

	if (!timeout)
		return -EAGAIN;

	return 0;
}

/*
 * instantiate_rng - builds and executes a descriptor on DECO0,
 *		     which initializes the RNG block.
 * @ctrldev - pointer to device
 * @state_handle_mask - bitmask containing the instantiation status
 *			for the RNG4 state handles which exist in
 *			the RNG4 block: 1 if it's been instantiated
 *			by an external entry, 0 otherwise.
 * @gen_sk  - generate data to be loaded into the JDKEK, TDKEK and TDSK;
 *	      Caution: this can be done only once; if the keys need to be
 *	      regenerated, a POR is required
 *
 * Return: - 0 if no error occurred
 *	   - -ENOMEM if there isn't enough memory to allocate the descriptor
 *	   - -ENODEV if DECO0 couldn't be acquired
 *	   - -EAGAIN if an error occurred when executing the descriptor
 *	      f.i. there was a RNG hardware error due to not "good enough"
 *	      entropy being aquired.
 */
static int instantiate_rng(struct device *ctrldev, int state_handle_mask,
			   int gen_sk)
{
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	struct caam_ctrl __iomem *ctrl;
	u32 *desc, status = 0, rdsta_val;
	int ret = 0, sh_idx;

	ctrl = (struct caam_ctrl __iomem *)ctrlpriv->ctrl;
	desc = kmalloc(CAAM_CMD_SZ * 7, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	for (sh_idx = 0; sh_idx < RNG4_MAX_HANDLES; sh_idx++) {
		/*
		 * If the corresponding bit is set, this state handle
		 * was initialized by somebody else, so it's left alone.
		 */
		if ((1 << sh_idx) & state_handle_mask)
			continue;

		/* Create the descriptor for instantiating RNG State Handle */
		build_instantiation_desc(desc, sh_idx, gen_sk);

		/* Try to run it through DECO0 */
		ret = run_descriptor_deco0(ctrldev, desc, &status);

		/*
		 * If ret is not 0, or descriptor status is not 0, then
		 * something went wrong. No need to try the next state
		 * handle (if available), bail out here.
		 * Also, if for some reason, the State Handle didn't get
		 * instantiated although the descriptor has finished
		 * without any error (HW optimizations for later
		 * CAAM eras), then try again.
		 */
		if (ret)
			break;

		rdsta_val = rd_reg32(&ctrl->r4tst[0].rdsta) & RDSTA_IFMASK;
		if ((status && status != JRSTA_SSRC_JUMP_HALT_CC) ||
		    !(rdsta_val & (1 << sh_idx))) {
			ret = -EAGAIN;
			break;
		}

		dev_info(ctrldev, "Instantiated RNG4 SH%d\n", sh_idx);
		/* Clear the contents before recreating the descriptor */
		memset(desc, 0x00, CAAM_CMD_SZ * 7);
	}

	kfree(desc);

	return ret;
}

/*
 * deinstantiate_rng - builds and executes a descriptor on DECO0,
 *		       which deinitializes the RNG block.
 * @ctrldev - pointer to device
 * @state_handle_mask - bitmask containing the instantiation status
 *			for the RNG4 state handles which exist in
 *			the RNG4 block: 1 if it's been instantiated
 *
 * Return: - 0 if no error occurred
 *	   - -ENOMEM if there isn't enough memory to allocate the descriptor
 *	   - -ENODEV if DECO0 couldn't be acquired
 *	   - -EAGAIN if an error occurred when executing the descriptor
 */
static int deinstantiate_rng(struct device *ctrldev, int state_handle_mask)
{
	u32 *desc, status;
	int sh_idx, ret = 0;

	desc = kmalloc(CAAM_CMD_SZ * 3, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	for (sh_idx = 0; sh_idx < RNG4_MAX_HANDLES; sh_idx++) {
		/*
		 * If the corresponding bit is set, then it means the state
		 * handle was initialized by us, and thus it needs to be
		 * deintialized as well
		 */
		if ((1 << sh_idx) & state_handle_mask) {
			/*
			 * Create the descriptor for deinstantating this state
			 * handle
			 */
			build_deinstantiation_desc(desc, sh_idx);

			/* Try to run it through DECO0 */
			ret = run_descriptor_deco0(ctrldev, desc, &status);

			if (ret ||
			    (status && status != JRSTA_SSRC_JUMP_HALT_CC)) {
				dev_err(ctrldev,
					"Failed to deinstantiate RNG4 SH%d\n",
					sh_idx);
				break;
			}
			dev_info(ctrldev, "Deinstantiated RNG4 SH%d\n", sh_idx);
		}
	}

	kfree(desc);

	return ret;
}

static int caam_remove(struct platform_device *pdev)
{
	struct device *ctrldev;
	struct caam_drv_private *ctrlpriv;
	struct caam_ctrl __iomem *ctrl;
	int ring;

	ctrldev = &pdev->dev;
	ctrlpriv = dev_get_drvdata(ctrldev);
	ctrl = (struct caam_ctrl __iomem *)ctrlpriv->ctrl;

	/* Remove platform devices for JobRs */
	for (ring = 0; ring < ctrlpriv->total_jobrs; ring++) {
		if (ctrlpriv->jrpdev[ring])
			of_device_unregister(ctrlpriv->jrpdev[ring]);
	}

	/* De-initialize RNG state handles initialized by this driver. */
	if (ctrlpriv->rng4_sh_init)
		deinstantiate_rng(ctrldev, ctrlpriv->rng4_sh_init);

	/* Shut down debug views */
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(ctrlpriv->dfs_root);
#endif

	/* Unmap controller region */
	iounmap(ctrl);

	/* shut clocks off before finalizing shutdown */
	clk_disable_unprepare(ctrlpriv->caam_ipg);
	clk_disable_unprepare(ctrlpriv->caam_mem);
	clk_disable_unprepare(ctrlpriv->caam_aclk);
	clk_disable_unprepare(ctrlpriv->caam_emi_slow);

	return 0;
}

/*
 * kick_trng - sets the various parameters for enabling the initialization
 *	       of the RNG4 block in CAAM
 * @pdev - pointer to the platform device
 * @ent_delay - Defines the length (in system clocks) of each entropy sample.
 */
static void kick_trng(struct platform_device *pdev, int ent_delay)
{
	struct device *ctrldev = &pdev->dev;
	struct caam_drv_private *ctrlpriv = dev_get_drvdata(ctrldev);
	struct caam_ctrl __iomem *ctrl;
	struct rng4tst __iomem *r4tst;
	u32 val;

	ctrl = (struct caam_ctrl __iomem *)ctrlpriv->ctrl;
	r4tst = &ctrl->r4tst[0];

	/* put RNG4 into program mode */
	clrsetbits_32(&r4tst->rtmctl, 0, RTMCTL_PRGM);

	/*
	 * Performance-wise, it does not make sense to
	 * set the delay to a value that is lower
	 * than the last one that worked (i.e. the state handles
	 * were instantiated properly. Thus, instead of wasting
	 * time trying to set the values controlling the sample
	 * frequency, the function simply returns.
	 */
	val = (rd_reg32(&r4tst->rtsdctl) & RTSDCTL_ENT_DLY_MASK)
	      >> RTSDCTL_ENT_DLY_SHIFT;
	if (ent_delay <= val) {
		/* put RNG4 into run mode */
		clrsetbits_32(&r4tst->rtmctl, RTMCTL_PRGM, 0);
		return;
	}

	val = rd_reg32(&r4tst->rtsdctl);
	val = (val & ~RTSDCTL_ENT_DLY_MASK) |
	      (ent_delay << RTSDCTL_ENT_DLY_SHIFT);
	wr_reg32(&r4tst->rtsdctl, val);
	/* min. freq. count, equal to 1/4 of the entropy sample length */
	wr_reg32(&r4tst->rtfrqmin, ent_delay >> 2);
	/* max. freq. count, equal to 16 times the entropy sample length */
	wr_reg32(&r4tst->rtfrqmax, ent_delay << 4);
	/* read the control register */
	val = rd_reg32(&r4tst->rtmctl);
	/*
	 * select raw sampling in both entropy shifter
	 * and statistical checker
	 */
	clrsetbits_32(&val, 0, RTMCTL_SAMP_MODE_RAW_ES_SC);
	/* put RNG4 into run mode */
	clrsetbits_32(&val, RTMCTL_PRGM, 0);
	/* write back the control register */
	wr_reg32(&r4tst->rtmctl, val);
}

static void detect_era(struct caam_drv_private *ctrlpriv)
{
	int ret, i;
	u32 caam_era;
	u32 caam_id_ms;
	char *era_source;
	struct device_node *caam_node;
	struct sec_vid sec_vid;
	static const struct {
		u16 ip_id;
		u8 maj_rev;
		u8 era;
	} caam_eras[] = {
		{0x0A10, 1, 1},
		{0x0A10, 2, 2},
		{0x0A12, 1, 3},
		{0x0A14, 1, 3},
		{0x0A10, 3, 4},
		{0x0A11, 1, 4},
		{0x0A14, 2, 4},
		{0x0A16, 1, 4},
		{0x0A18, 1, 4},
		{0x0A11, 2, 5},
		{0x0A12, 2, 5},
		{0x0A13, 1, 5},
		{0x0A1C, 1, 5},
		{0x0A12, 4, 6},
		{0x0A13, 2, 6},
		{0x0A16, 2, 6},
		{0x0A17, 1, 6},
		{0x0A18, 2, 6},
		{0x0A1A, 1, 6},
		{0x0A1C, 2, 6},
		{0x0A14, 3, 7},
		{0x0A10, 4, 8},
		{0x0A11, 3, 8},
		{0x0A11, 4, 8},
		{0x0A12, 5, 8},
		{0x0A16, 3, 8},
	};

	/* If the user or bootloader has set the property we'll use that */
	caam_node = of_find_compatible_node(NULL, NULL, "fsl,sec-v4.0");
	ret = of_property_read_u32(caam_node, "fsl,sec-era", &caam_era);
	of_node_put(caam_node);

	if (!ret) {
		era_source = "device tree";
		goto era_found;
	}

	/* If ccbvid has the era, use that (era 6 and onwards) */
	caam_era = rd_reg32(&ctrlpriv->ctrl->perfmon.ccb_id);
	caam_era = caam_era >> CCB_VID_ERA_SHIFT & CCB_VID_ERA_MASK;

	if (caam_era) {
		era_source = "CCBVID";
		goto era_found;
	}

	/* If we can match caamvid to known versions, use that */
	caam_id_ms = rd_reg32(&ctrlpriv->ctrl->perfmon.caam_id_ms);
	sec_vid.ip_id = caam_id_ms >> SEC_VID_IPID_SHIFT;
	sec_vid.maj_rev = (caam_id_ms & SEC_VID_MAJ_MASK) >> SEC_VID_MAJ_SHIFT;

	for (i = 0; i < ARRAY_SIZE(caam_eras); i++)
		if (caam_eras[i].ip_id == sec_vid.ip_id &&
		    caam_eras[i].maj_rev == sec_vid.maj_rev) {
			caam_era = caam_eras[i].era;
			era_source = "CAAMVID";
			goto era_found;
		}

	ctrlpriv->era = -ENOTSUPP;
	return;

era_found:
	ctrlpriv->era = caam_era;
	dev_info(&ctrlpriv->pdev->dev, "ERA source: %s.\n", era_source);
}

static void handle_imx6_err005766(struct caam_drv_private *ctrlpriv)
{
	/*
	 * ERRATA:  mx6 devices have an issue wherein AXI bus transactions
	 * may not occur in the correct order. This isn't a problem running
	 * single descriptors, but can be if running multiple concurrent
	 * descriptors. Reworking the driver to throttle to single requests
	 * is impractical, thus the workaround is to limit the AXI pipeline
	 * to a depth of 1 (from it's default of 4) to preclude this situation
	 * from occurring.
	 */

	u32 mcr_val;

	if (ctrlpriv->era != IMX_ERR005766_ERA)
		return;

	if (of_machine_is_compatible("fsl,imx6q") ||
	    of_machine_is_compatible("fsl,imx6dl") ||
	    of_machine_is_compatible("fsl,imx6qp")) {
		dev_info(&ctrlpriv->pdev->dev,
			 "AXI pipeline throttling enabled.\n");
		mcr_val = rd_reg32(&ctrlpriv->ctrl->mcr);
		wr_reg32(&ctrlpriv->ctrl->mcr,
			 (mcr_val & ~(MCFGR_AXIPIPE_MASK)) |
			 ((1 << MCFGR_AXIPIPE_SHIFT) & MCFGR_AXIPIPE_MASK));
	}
}

#ifdef CONFIG_DEBUG_FS
static int caam_debugfs_u64_get(void *data, u64 *val)
{
	*val = caam64_to_cpu(*(u64 *)data);
	return 0;
}

static int caam_debugfs_u32_get(void *data, u64 *val)
{
	*val = caam32_to_cpu(*(u32 *)data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(caam_fops_u32_ro, caam_debugfs_u32_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(caam_fops_u64_ro, caam_debugfs_u64_get, NULL, "%llu\n");
#endif

/* Probe routine for CAAM top (controller) level */
static int caam_probe(struct platform_device *pdev)
{
	int ret, ring, rspec, gen_sk, ent_delay = RTSDCTL_ENT_DLY_MIN;
	u64 caam_id;
	struct device *dev;
	struct device_node *nprop, *np;
	struct caam_ctrl __iomem *ctrl;
	struct caam_drv_private *ctrlpriv;
	struct clk *clk;
#ifdef CONFIG_DEBUG_FS
	struct caam_perfmon *perfmon;
#endif
	u32 scfgr, comp_params;
	u32 cha_vid_ls;
	int pg_size;
	int BLOCK_OFFSET = 0;

	ctrlpriv = devm_kzalloc(&pdev->dev, sizeof(*ctrlpriv), GFP_KERNEL);
	if (!ctrlpriv)
		return -ENOMEM;

	dev = &pdev->dev;
	dev_set_drvdata(dev, ctrlpriv);
	ctrlpriv->pdev = pdev;
	nprop = pdev->dev.of_node;

	/* Enable clocking */
	clk = caam_drv_identify_clk(&pdev->dev, "ipg");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(&pdev->dev,
			"can't identify CAAM ipg clk: %d\n", ret);
		return ret;
	}
	ctrlpriv->caam_ipg = clk;

	ret = clk_prepare_enable(ctrlpriv->caam_ipg);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't enable CAAM ipg clock: %d\n", ret);
		return ret;
	}

	clk = caam_drv_identify_clk(&pdev->dev, "aclk");
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		dev_err(&pdev->dev,
			"can't identify CAAM aclk clk: %d\n", ret);
		goto disable_clocks;
	}
	ctrlpriv->caam_aclk = clk;

	ret = clk_prepare_enable(ctrlpriv->caam_aclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "can't enable CAAM aclk clock: %d\n",
			ret);
		goto disable_clocks;
	}

	if (!(of_find_compatible_node(NULL, NULL, "fsl,imx7d-caam"))) {

		clk = caam_drv_identify_clk(&pdev->dev, "mem");
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			dev_err(&pdev->dev,
				"can't identify CAAM mem clk: %d\n", ret);
			goto disable_clocks;
		}
		ctrlpriv->caam_mem = clk;

		ret = clk_prepare_enable(ctrlpriv->caam_mem);
		if (ret < 0) {
			dev_err(&pdev->dev, "can't enable CAAM secure mem clock: %d\n",
				ret);
			goto disable_clocks;
		}

		if (!(of_find_compatible_node(NULL, NULL, "fsl,imx6ul-caam"))) {
			clk = caam_drv_identify_clk(&pdev->dev, "emi_slow");
			if (IS_ERR(clk)) {
				ret = PTR_ERR(clk);
				dev_err(&pdev->dev,
					"can't identify CAAM emi_slow clk: %d\n", ret);
				goto disable_clocks;
			}
			ctrlpriv->caam_emi_slow = clk;

			ret = clk_prepare_enable(ctrlpriv->caam_emi_slow);
			if (ret < 0) {
				dev_err(&pdev->dev, "can't enable CAAM emi slow clock: %d\n",
					ret);
				goto disable_clocks;
			}
		}
	}

	/* Get configuration properties from device tree */
	/* First, get register page */
	ctrl = of_iomap(nprop, 0);
	if (ctrl == NULL) {
		dev_err(dev, "caam: of_iomap() failed\n");
		ret = -ENOMEM;
		goto disable_clocks;
	}

	caam_little_end = !(bool)(rd_reg32(&ctrl->perfmon.status) &
				  (CSTA_PLEND | CSTA_ALT_PLEND));

	/* Finding the page size for using the CTPR_MS register */
	comp_params = rd_reg32(&ctrl->perfmon.comp_parms_ms);
	pg_size = (comp_params & CTPR_MS_PG_SZ_MASK) >> CTPR_MS_PG_SZ_SHIFT;

	/* Allocating the BLOCK_OFFSET based on the supported page size on
	 * the platform
	 */
	if (pg_size == 0)
		BLOCK_OFFSET = PG_SIZE_4K;
	else
		BLOCK_OFFSET = PG_SIZE_64K;

	ctrlpriv->ctrl = (struct caam_ctrl __force *)ctrl;
	ctrlpriv->assure = (struct caam_assurance __force *)
			   ((uint8_t *)ctrl +
			    BLOCK_OFFSET * ASSURE_BLOCK_NUMBER
			   );
	ctrlpriv->deco = (struct caam_deco __force *)
			 ((uint8_t *)ctrl +
			 BLOCK_OFFSET * DECO_BLOCK_NUMBER
			 );

	detect_era(ctrlpriv);

	/* Get CAAM-SM node and of_iomap() and save */
	np = of_find_compatible_node(NULL, NULL, "fsl,imx6q-caam-sm");
	if (!np)
		return -ENODEV;

	ctrlpriv->sm_base = of_iomap(np, 0);
	ctrlpriv->sm_size = 0x3fff;

	/*
	 * Enable DECO watchdogs and, if this is a PHYS_ADDR_T_64BIT kernel,
	 * long pointers in master configuration register
	 */
	clrsetbits_32(&ctrl->mcr, MCFGR_AWCACHE_MASK | MCFGR_LONG_PTR,
		      MCFGR_AWCACHE_CACH | MCFGR_AWCACHE_BUFF |
		      MCFGR_WDENABLE | MCFGR_LARGE_BURST |
		      (sizeof(dma_addr_t) == sizeof(u64) ? MCFGR_LONG_PTR : 0));

	handle_imx6_err005766(ctrlpriv);

	/*
	 *  Read the Compile Time paramters and SCFGR to determine
	 * if Virtualization is enabled for this platform
	 */
	scfgr = rd_reg32(&ctrl->scfgr);

	ctrlpriv->virt_en = 0;
	if (comp_params & CTPR_MS_VIRT_EN_INCL) {
		/* VIRT_EN_INCL = 1 & VIRT_EN_POR = 1 or
		 * VIRT_EN_INCL = 1 & VIRT_EN_POR = 0 & SCFGR_VIRT_EN = 1
		 */
		if ((comp_params & CTPR_MS_VIRT_EN_POR) ||
		    (!(comp_params & CTPR_MS_VIRT_EN_POR) &&
		       (scfgr & SCFGR_VIRT_EN)))
				ctrlpriv->virt_en = 1;
	} else {
		/* VIRT_EN_INCL = 0 && VIRT_EN_POR_VALUE = 1 */
		if (comp_params & CTPR_MS_VIRT_EN_POR)
				ctrlpriv->virt_en = 1;
	}

	if (ctrlpriv->virt_en == 1)
		clrsetbits_32(&ctrl->jrstart, 0, JRSTART_JR0_START |
			      JRSTART_JR1_START | JRSTART_JR2_START |
			      JRSTART_JR3_START);

	/* Set DMA masks according to platform ranging */
	if (sizeof(dma_addr_t) == sizeof(u64))
		if (of_device_is_compatible(nprop, "fsl,sec-v5.0"))
			dma_set_mask_and_coherent(dev, DMA_BIT_MASK(40));
		else
			dma_set_mask_and_coherent(dev, DMA_BIT_MASK(36));
	else
		dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));

	/*
	 * Detect and enable JobRs
	 * First, find out how many ring spec'ed, allocate references
	 * for all, then go probe each one.
	 */
	rspec = 0;
	for_each_available_child_of_node(nprop, np)
		if (of_device_is_compatible(np, "fsl,sec-v4.0-job-ring") ||
		    of_device_is_compatible(np, "fsl,sec4.0-job-ring"))
			rspec++;

	ctrlpriv->jrpdev = devm_kcalloc(&pdev->dev, rspec,
					sizeof(*ctrlpriv->jrpdev), GFP_KERNEL);
	if (ctrlpriv->jrpdev == NULL) {
		ret = -ENOMEM;
		goto iounmap_ctrl;
	}

	ring = 0;
	ctrlpriv->total_jobrs = 0;
	for_each_available_child_of_node(nprop, np)
		if (of_device_is_compatible(np, "fsl,sec-v4.0-job-ring") ||
		    of_device_is_compatible(np, "fsl,sec4.0-job-ring")) {
			ctrlpriv->jrpdev[ring] =
				of_platform_device_create(np, NULL, dev);
			if (!ctrlpriv->jrpdev[ring]) {
				pr_warn("JR%d Platform device creation error\n",
					ring);
				continue;
			}
			ctrlpriv->jr[ring] = (struct caam_job_ring __force *)
					     ((uint8_t *)ctrl +
					     (ring + JR_BLOCK_NUMBER) *
					      BLOCK_OFFSET
					     );
			ctrlpriv->total_jobrs++;
			ring++;
	}

	/* Check to see if QI present. If so, enable */
	ctrlpriv->qi_present =
			!!(rd_reg32(&ctrl->perfmon.comp_parms_ms) &
			   CTPR_MS_QI_MASK);
	if (ctrlpriv->qi_present) {
		ctrlpriv->qi = (struct caam_queue_if __force *)
			       ((uint8_t *)ctrl +
				 BLOCK_OFFSET * QI_BLOCK_NUMBER
			       );
		/* This is all that's required to physically enable QI */
		wr_reg32(&ctrlpriv->qi->qi_control_lo, QICTL_DQEN);
	}

	/* If no QI and no rings specified, quit and go home */
	if ((!ctrlpriv->qi_present) && (!ctrlpriv->total_jobrs)) {
		dev_err(dev, "no queues configured, terminating\n");
		ret = -ENOMEM;
		goto caam_remove;
	}

	cha_vid_ls = rd_reg32(&ctrl->perfmon.cha_id_ls);

	/*
	 * If SEC has RNG version >= 4 and RNG state handle has not been
	 * already instantiated, do RNG instantiation
	 */
	if ((cha_vid_ls & CHA_ID_LS_RNG_MASK) >> CHA_ID_LS_RNG_SHIFT >= 4) {
		ctrlpriv->rng4_sh_init =
			rd_reg32(&ctrl->r4tst[0].rdsta);
		/*
		 * If the secure keys (TDKEK, JDKEK, TDSK), were already
		 * generated, signal this to the function that is instantiating
		 * the state handles. An error would occur if RNG4 attempts
		 * to regenerate these keys before the next POR.
		 */
		gen_sk = ctrlpriv->rng4_sh_init & RDSTA_SKVN ? 0 : 1;
		ctrlpriv->rng4_sh_init &= RDSTA_IFMASK;
		do {
			int inst_handles =
				rd_reg32(&ctrl->r4tst[0].rdsta) &
								RDSTA_IFMASK;
			/*
			 * If either SH were instantiated by somebody else
			 * (e.g. u-boot) then it is assumed that the entropy
			 * parameters are properly set and thus the function
			 * setting these (kick_trng(...)) is skipped.
			 * Also, if a handle was instantiated, do not change
			 * the TRNG parameters.
			 */
			if (!(ctrlpriv->rng4_sh_init || inst_handles)) {
				dev_info(dev,
					 "Entropy delay = %u\n",
					 ent_delay);
				kick_trng(pdev, ent_delay);
				ent_delay += 400;
			}
			/*
			 * if instantiate_rng(...) fails, the loop will rerun
			 * and the kick_trng(...) function will modfiy the
			 * upper and lower limits of the entropy sampling
			 * interval, leading to a sucessful initialization of
			 * the RNG.
			 */
			ret = instantiate_rng(dev, inst_handles,
					      gen_sk);
			if (ret == -EAGAIN)
				/*
				 * if here, the loop will rerun,
				 * so don't hog the CPU
				 */
				cpu_relax();
		} while ((ret == -EAGAIN) && (ent_delay < RTSDCTL_ENT_DLY_MAX));
		if (ret) {
			dev_err(dev, "failed to instantiate RNG");
			goto caam_remove;
		}
		/*
		 * Set handles init'ed by this module as the complement of the
		 * already initialized ones
		 */
		ctrlpriv->rng4_sh_init = ~ctrlpriv->rng4_sh_init & RDSTA_IFMASK;

		/* Enable RDB bit so that RNG works faster */
		clrsetbits_32(&ctrl->scfgr, 0, SCFGR_RDBENABLE);
	}

	/* NOTE: RTIC detection ought to go here, around Si time */

	caam_id = (u64)rd_reg32(&ctrl->perfmon.caam_id_ms) << 32 |
		  (u64)rd_reg32(&ctrl->perfmon.caam_id_ls);

	dev_info(dev, "device ID = 0x%016llx (Era %d)\n", caam_id,
		 ctrlpriv->era);

	dev_info(dev, "job rings = %d, qi = %d\n",
		 ctrlpriv->total_jobrs, ctrlpriv->qi_present);

#ifdef CONFIG_DEBUG_FS
	/*
	 * FIXME: needs better naming distinction, as some amalgamation of
	 * "caam" and nprop->full_name. The OF name isn't distinctive,
	 * but does separate instances
	 */
	perfmon = (struct caam_perfmon __force *)&ctrl->perfmon;

	ctrlpriv->dfs_root = debugfs_create_dir(dev_name(dev), NULL);
	ctrlpriv->ctl = debugfs_create_dir("ctl", ctrlpriv->dfs_root);

	/* Controller-level - performance monitor counters */

	ctrlpriv->ctl_rq_dequeued =
		debugfs_create_file("rq_dequeued",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->req_dequeued,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ob_enc_req =
		debugfs_create_file("ob_rq_encrypted",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ob_enc_req,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ib_dec_req =
		debugfs_create_file("ib_rq_decrypted",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ib_dec_req,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ob_enc_bytes =
		debugfs_create_file("ob_bytes_encrypted",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ob_enc_bytes,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ob_prot_bytes =
		debugfs_create_file("ob_bytes_protected",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ob_prot_bytes,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ib_dec_bytes =
		debugfs_create_file("ib_bytes_decrypted",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ib_dec_bytes,
				    &caam_fops_u64_ro);
	ctrlpriv->ctl_ib_valid_bytes =
		debugfs_create_file("ib_bytes_validated",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->ib_valid_bytes,
				    &caam_fops_u64_ro);

	/* Controller level - global status values */
	ctrlpriv->ctl_faultaddr =
		debugfs_create_file("fault_addr",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->faultaddr,
				    &caam_fops_u32_ro);
	ctrlpriv->ctl_faultdetail =
		debugfs_create_file("fault_detail",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->faultdetail,
				    &caam_fops_u32_ro);
	ctrlpriv->ctl_faultstatus =
		debugfs_create_file("fault_status",
				    S_IRUSR | S_IRGRP | S_IROTH,
				    ctrlpriv->ctl, &perfmon->status,
				    &caam_fops_u32_ro);

	/* Internal covering keys (useful in non-secure mode only) */
	ctrlpriv->ctl_kek_wrap.data = &ctrlpriv->ctrl->kek[0];
	ctrlpriv->ctl_kek_wrap.size = KEK_KEY_SIZE * sizeof(u32);
	ctrlpriv->ctl_kek = debugfs_create_blob("kek",
						S_IRUSR |
						S_IRGRP | S_IROTH,
						ctrlpriv->ctl,
						&ctrlpriv->ctl_kek_wrap);

	ctrlpriv->ctl_tkek_wrap.data = &ctrlpriv->ctrl->tkek[0];
	ctrlpriv->ctl_tkek_wrap.size = KEK_KEY_SIZE * sizeof(u32);
	ctrlpriv->ctl_tkek = debugfs_create_blob("tkek",
						 S_IRUSR |
						 S_IRGRP | S_IROTH,
						 ctrlpriv->ctl,
						 &ctrlpriv->ctl_tkek_wrap);

	ctrlpriv->ctl_tdsk_wrap.data = &ctrlpriv->ctrl->tdsk[0];
	ctrlpriv->ctl_tdsk_wrap.size = KEK_KEY_SIZE * sizeof(u32);
	ctrlpriv->ctl_tdsk = debugfs_create_blob("tdsk",
						 S_IRUSR |
						 S_IRGRP | S_IROTH,
						 ctrlpriv->ctl,
						 &ctrlpriv->ctl_tdsk_wrap);
#endif
	return 0;

caam_remove:
	caam_remove(pdev);
	return ret;

iounmap_ctrl:
	iounmap(ctrl);
disable_clocks:
	clk_disable_unprepare(ctrlpriv->caam_emi_slow);
	clk_disable_unprepare(ctrlpriv->caam_aclk);
	clk_disable_unprepare(ctrlpriv->caam_mem);
	clk_disable_unprepare(ctrlpriv->caam_ipg);
	return ret;
}

static struct of_device_id caam_match[] = {
	{
		.compatible = "fsl,sec-v4.0",
	},
	{
		.compatible = "fsl,sec4.0",
	},
	{},
};
MODULE_DEVICE_TABLE(of, caam_match);

static struct platform_driver caam_driver = {
	.driver = {
		.name = "caam",
		.of_match_table = caam_match,
	},
	.probe       = caam_probe,
	.remove      = caam_remove,
};

module_platform_driver(caam_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FSL CAAM request backend");
MODULE_AUTHOR("Freescale Semiconductor - NMG/STC");
