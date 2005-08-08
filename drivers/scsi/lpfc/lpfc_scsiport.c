/*******************************************************************
 * This file is part of the Emulex Linux Device Driver for         *
 * Enterprise Fibre Channel Host Bus Adapters.                     *
 * Refer to the README file included with this package for         *
 * driver version and adapter support.                             *
 * Copyright (C) 2004 Emulex Corporation.                          *
 * www.emulex.com                                                  *
 *                                                                 *
 * This program is free software; you can redistribute it and/or   *
 * modify it under the terms of the GNU General Public License     *
 * as published by the Free Software Foundation; either version 2  *
 * of the License, or (at your option) any later version.          *
 *                                                                 *
 * This program is distributed in the hope that it will be useful, *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of  *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the   *
 * GNU General Public License for more details, a copy of which    *
 * can be found in the file COPYING included with this package.    *
 *******************************************************************/

/*
 * $Id: lpfc_scsiport.c 1.208 2004/12/03 11:27:34EST sf_support Exp  $
 */
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <scsi/scsi.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_transport_fc.h>

#include "lpfc_hw.h"
#include "lpfc_sli.h"
#include "lpfc_mem.h"
#include "lpfc_disc.h"
#include "lpfc_scsi.h"
#include "lpfc.h"
#include "lpfc_logmsg.h"
#include "lpfc_fcp.h"
#include "lpfc_crtn.h"

/* This routine allocates a scsi buffer, which contains all the necessary
 * information needed to initiate a SCSI I/O. The non-DMAable region of
 * the buffer contains the area to build the IOCB. The DMAable region contains
 * the memory for the FCP CMND, FCP RSP, and the inital BPL.
 * In addition to allocating memeory, the FCP CMND and FCP RSP BDEs are setup
 * in the BPL and the BPL BDE is setup in the IOCB.
 */
struct lpfc_scsi_buf *
lpfc_get_scsi_buf(struct lpfc_hba * phba, int gfp_flags)
{
	struct lpfc_scsi_buf *psb;
	struct ulp_bde64 *bpl;
	IOCB_t *cmd;
	uint8_t *ptr;
	dma_addr_t pdma_phys;

	psb = mempool_alloc(phba->scsibuf_mem_pool, gfp_flags);
	if (!psb)
		return NULL;

	memset(psb, 0, sizeof (struct lpfc_scsi_buf));

	/* Get a SCSI DMA extention for an I/O */
	/*
	 * The DMA buffer for struct fcp_cmnd, struct fcp_rsp and BPL use
	 * lpfc_scsi_dma_ext_pool with size LPFC_SCSI_DMA_EXT_SIZE
	 *
	 *
	 *    The size of struct fcp_cmnd  = 32 bytes.
	 *    The size of struct fcp_rsp   = 160 bytes.
	 *    The size of struct ulp_bde64 = 12 bytes and driver can only
	 *    support LPFC_SCSI_INITIAL_BPL_SIZE (3) S/G segments for scsi data.
	 *    One struct ulp_bde64 is used for each of the struct fcp_cmnd and
	 *    struct fcp_rsp
	 *
	 *    Total usage for each I/O use 32 + 160 + (2 * 12) +
	 *    (4 * 12) = 264 bytes.
	 */

	INIT_LIST_HEAD(&psb->dma_ext.list);

	psb->dma_ext.virt = pci_pool_alloc(phba->lpfc_scsi_dma_ext_pool,
					   GFP_ATOMIC, &psb->dma_ext.phys);
	if (!psb->dma_ext.virt) {
		mempool_free(psb, phba->scsibuf_mem_pool);
		return NULL;
	}

	/* Save virtual ptrs to FCP Command, Response, and BPL */
	ptr = (uint8_t *) psb->dma_ext.virt;

	memset(ptr, 0, LPFC_SCSI_DMA_EXT_SIZE);
	psb->fcp_cmnd = (struct fcp_cmnd *) ptr;
	ptr += sizeof (struct fcp_cmnd);
	psb->fcp_rsp = (struct fcp_rsp *) ptr;
	ptr += (sizeof (struct fcp_rsp));
	psb->fcp_bpl = (struct ulp_bde64 *) ptr;
	psb->scsi_hba = phba;

	/* Since this is for a FCP cmd, the first 2 BDEs in the BPL are always
	 * the FCP CMND and FCP RSP, so lets just set it up right here.
	 */
	bpl = psb->fcp_bpl;
	/* ptr points to physical address of FCP CMD */
	pdma_phys = psb->dma_ext.phys;
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys));
	bpl->addrLow = le32_to_cpu(putPaddrLow(pdma_phys));
	bpl->tus.f.bdeSize = sizeof (struct fcp_cmnd);
	bpl->tus.f.bdeFlags = BUFF_USE_CMND;
	bpl->tus.w = le32_to_cpu(bpl->tus.w);
	bpl++;

	/* Setup FCP RSP */
	pdma_phys += sizeof (struct fcp_cmnd);
	bpl->addrHigh = le32_to_cpu(putPaddrHigh(pdma_phys));
	bpl->addrLow = le32_to_cpu(putPaddrLow(pdma_phys));
	bpl->tus.f.bdeSize = sizeof (struct fcp_rsp);
	bpl->tus.f.bdeFlags = (BUFF_USE_CMND | BUFF_USE_RCV);
	bpl->tus.w = le32_to_cpu(bpl->tus.w);
	bpl++;

	/* Since the IOCB for the FCP I/O is built into the struct
	 * lpfc_scsi_buf, lets setup what we can right here.
	 */
	pdma_phys += (sizeof (struct fcp_rsp));
	cmd = &psb->cur_iocbq.iocb;
	cmd->un.fcpi64.bdl.ulpIoTag32 = 0;
	cmd->un.fcpi64.bdl.addrHigh = putPaddrHigh(pdma_phys);
	cmd->un.fcpi64.bdl.addrLow = putPaddrLow(pdma_phys);
	cmd->un.fcpi64.bdl.bdeSize = (2 * sizeof (struct ulp_bde64));
	cmd->un.fcpi64.bdl.bdeFlags = BUFF_TYPE_BDL;
	cmd->ulpBdeCount = 1;
	cmd->ulpClass = CLASS3;

	return (psb);
}

static void
lpfc_free_scsi_buf(struct lpfc_scsi_buf * psb)
{
	struct lpfc_hba *phba = psb->scsi_hba;
	struct lpfc_dmabuf *pbpl, *next_bpl;

	/*
	 * There are only two special cases to consider.  (1) the scsi command
	 * requested scatter-gather usage or (2) the scsi command allocated
	 * a request buffer, but did not request use_sg.  There is a third
	 * case, but it does not require resource deallocation.
	 */

	if ((psb->seg_cnt > 0) && (psb->pCmd->use_sg)) {
		/*
		 * Since the segment count is nonzero, the scsi command 
		 * requested scatter-gather usage and the driver allocated
		 * addition memory buffers to chain BPLs.  Traverse this list
		 * and release those resource before freeing the parent 
		 * structure.
		 */
		dma_unmap_sg(&phba->pcidev->dev, psb->pCmd->request_buffer, 
				psb->seg_cnt, psb->pCmd->sc_data_direction);

		list_for_each_entry_safe(pbpl, next_bpl, 
						&psb->dma_ext.list, list) {
			lpfc_mbuf_free(phba, pbpl->virt, pbpl->phys);
			list_del(&pbpl->list);
			kfree(pbpl);
		}
	} else {
		 if ((psb->nonsg_phys) && (psb->pCmd->request_bufflen)) {
			/*
			 * Since either the segment count or the use_sg
			 * value is zero, the scsi command did not request 
			 * scatter-gather usage and no additional buffers were
			 * required.  Just unmap the dma single resource.
			 */
			dma_unmap_single(&phba->pcidev->dev, psb->nonsg_phys,
						psb->pCmd->request_bufflen,
						psb->pCmd->sc_data_direction);
		 }
	}

	/*
	 * Release the pci pool resource and clean up the scsi buffer.  Neither
	 * are required now that the IO has completed.
	 */
	pci_pool_free(phba->lpfc_scsi_dma_ext_pool, psb->dma_ext.virt,
							 psb->dma_ext.phys);
	mempool_free(psb, phba->scsibuf_mem_pool);
}

static int
lpfc_os_prep_io(struct lpfc_hba * phba, struct lpfc_scsi_buf * lpfc_cmd)
{
	struct fcp_cmnd *fcp_cmnd;
	struct ulp_bde64 *topbpl = NULL;
	struct ulp_bde64 *bpl;
	struct lpfc_dmabuf *bmp;
	struct lpfc_dmabuf *head_bmp;
	IOCB_t *cmd;
	struct scsi_cmnd *cmnd;
	struct scatterlist *sgel = NULL;
	struct scatterlist *sgel_begin = NULL;
	dma_addr_t physaddr;
	uint32_t i;
	uint32_t num_bmps = 1, num_bde = 0, max_bde;
	uint16_t use_sg;
	int datadir;
	int dma_error;

	bpl = lpfc_cmd->fcp_bpl;
	fcp_cmnd = lpfc_cmd->fcp_cmnd;

	bpl += 2;		/* Bump past FCP CMND and FCP RSP */
	max_bde = LPFC_SCSI_INITIAL_BPL_SIZE - 1;

	cmnd = lpfc_cmd->pCmd;
	cmd = &lpfc_cmd->cur_iocbq.iocb;

	/* These are needed if we chain BPLs */
	head_bmp = &(lpfc_cmd->dma_ext);
	use_sg = cmnd->use_sg;

	/*
	 * Fill in the FCP CMND
	 */
	memcpy(&fcp_cmnd->fcpCdb[0], cmnd->cmnd, 16);

	if (cmnd->device->tagged_supported) {
		switch (cmnd->tag) {
		case HEAD_OF_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = HEAD_OF_Q;
			break;
		case ORDERED_QUEUE_TAG:
			fcp_cmnd->fcpCntl1 = ORDERED_Q;
			break;
		default:
			fcp_cmnd->fcpCntl1 = SIMPLE_Q;
			break;
		}
	} else {
		fcp_cmnd->fcpCntl1 = 0;
	}

	datadir = cmnd->sc_data_direction;

	if (use_sg) {
		/*
		 * Get a local pointer to the scatter-gather list.  The
		 * scatter-gather list head must be preserved since
		 * sgel is incremented in the loop.  The driver must store
		 * the segment count returned from pci_map_sg for calls to
		 * pci_unmap_sg later on because the use_sg field in the
		 * scsi_cmd is a count of physical memory pages, whereas the
		 * seg_cnt is a count of dma-mappings used by the MMIO to
		 * map the use_sg pages.  They are not the same in most
		 * cases for those architectures that implement an MMIO.
		 */
		sgel = (struct scatterlist *)cmnd->request_buffer;
		sgel_begin = sgel;
		lpfc_cmd->seg_cnt = dma_map_sg(&phba->pcidev->dev, sgel,
						use_sg, datadir);

		/* return error if we cannot map sg list */
		if (lpfc_cmd->seg_cnt == 0)
			return 1;

		/* scatter-gather list case */
		for (i = 0; i < lpfc_cmd->seg_cnt; i++) {
			/* Check to see if current BPL is full of BDEs */
			/* If this is last BDE and there is one left in */
			/* current BPL, use it.                         */
			if (num_bde == max_bde) {
				bmp = kmalloc(sizeof (struct lpfc_dmabuf),
					      GFP_ATOMIC);
				if (bmp == 0) {
					goto error_out;
				}
				memset(bmp, 0, sizeof (struct lpfc_dmabuf));
				bmp->virt =
				    lpfc_mbuf_alloc(phba, 0, &bmp->phys);
				if (!bmp->virt) {
					kfree(bmp);
					goto error_out;
				}
				max_bde = ((1024 / sizeof(struct ulp_bde64))-3);
				/* Fill in continuation entry to next bpl */
				bpl->addrHigh =
				    le32_to_cpu(putPaddrHigh(bmp->phys));
				bpl->addrLow =
				    le32_to_cpu(putPaddrLow(bmp->phys));
				bpl->tus.f.bdeFlags = BPL64_SIZE_WORD;
				num_bde++;
				if (num_bmps == 1) {
					cmd->un.fcpi64.bdl.bdeSize += (num_bde *
						sizeof (struct ulp_bde64));
				} else {
					topbpl->tus.f.bdeSize = (num_bde *
						sizeof (struct ulp_bde64));
					topbpl->tus.w =
					    le32_to_cpu(topbpl->tus.w);
				}
				topbpl = bpl;
				bpl = (struct ulp_bde64 *) bmp->virt;
				list_add(&bmp->list, &head_bmp->list);
				num_bde = 0;
				num_bmps++;
			}

			physaddr = sg_dma_address(sgel);

			bpl->addrLow = le32_to_cpu(putPaddrLow(physaddr));
			bpl->addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
			bpl->tus.f.bdeSize = sg_dma_len(sgel);
			if (datadir == DMA_TO_DEVICE)
				bpl->tus.f.bdeFlags = 0;
			else
				bpl->tus.f.bdeFlags = BUFF_USE_RCV;
			bpl->tus.w = le32_to_cpu(bpl->tus.w);
			bpl++;
			sgel++;
			num_bde++;
		}		/* end for loop */

		if (datadir == DMA_TO_DEVICE) {
			cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
			fcp_cmnd->fcpCntl3 = WRITE_DATA;

			phba->fc4OutputRequests++;
		} else {
			cmd->ulpCommand = CMD_FCP_IREAD64_CR;
			cmd->ulpPU = PARM_READ_CHECK;
			cmd->un.fcpi.fcpi_parm = cmnd->request_bufflen;
			fcp_cmnd->fcpCntl3 = READ_DATA;

			phba->fc4InputRequests++;
		}
	} else if (cmnd->request_buffer && cmnd->request_bufflen) {
		physaddr = dma_map_single(&phba->pcidev->dev,
					  cmnd->request_buffer,
					  cmnd->request_bufflen,
					  datadir);
			dma_error = dma_mapping_error(physaddr);
			if (dma_error){
				lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
					"%d:0718 Unable to dma_map_single "
					"request_buffer: x%x\n",
					phba->brd_no, dma_error);
				return 1;
			}

		/* no scatter-gather list case */
		lpfc_cmd->nonsg_phys = physaddr;
		bpl->addrLow = le32_to_cpu(putPaddrLow(physaddr));
		bpl->addrHigh = le32_to_cpu(putPaddrHigh(physaddr));
		bpl->tus.f.bdeSize = cmnd->request_bufflen;
		if (datadir == DMA_TO_DEVICE) {
			cmd->ulpCommand = CMD_FCP_IWRITE64_CR;
			fcp_cmnd->fcpCntl3 = WRITE_DATA;
			bpl->tus.f.bdeFlags = 0;

			phba->fc4OutputRequests++;
		} else {
			cmd->ulpCommand = CMD_FCP_IREAD64_CR;
			cmd->ulpPU = PARM_READ_CHECK;
			cmd->un.fcpi.fcpi_parm = cmnd->request_bufflen;
			fcp_cmnd->fcpCntl3 = READ_DATA;
			bpl->tus.f.bdeFlags = BUFF_USE_RCV;

			phba->fc4InputRequests++;
		}
		bpl->tus.w = le32_to_cpu(bpl->tus.w);
		num_bde = 1;
		bpl++;
	} else {
		cmd->ulpCommand = CMD_FCP_ICMND64_CR;
		cmd->un.fcpi.fcpi_parm = 0;
		fcp_cmnd->fcpCntl3 = 0;

		phba->fc4ControlRequests++;
	}

	bpl->addrHigh = 0;
	bpl->addrLow = 0;
	bpl->tus.w = 0;
	if (num_bmps == 1) {
		cmd->un.fcpi64.bdl.bdeSize +=
			(num_bde * sizeof (struct ulp_bde64));
	} else {
		topbpl->tus.f.bdeSize = (num_bde * sizeof (struct ulp_bde64));
		topbpl->tus.w = le32_to_cpu(topbpl->tus.w);
	}
	cmd->ulpBdeCount = 1;
	cmd->ulpLe = 1;		/* Set the LE bit in the iocb */

	/* set the Data Length field in the FCP CMND accordingly */
	fcp_cmnd->fcpDl = be32_to_cpu(cmnd->request_bufflen);

	return 0;

error_out:
	/*
	 * Allocation of a chained BPL failed, unmap the sg list and return
	 * error.  This will ultimately cause lpfc_free_scsi_buf to be called
	 * which will handle the rest of the cleanup.  Set seg_cnt back to zero
	 * to avoid double unmaps of the sg resources.
	 */
	dma_unmap_sg(&phba->pcidev->dev, sgel_begin, lpfc_cmd->seg_cnt,
			datadir);
	lpfc_cmd->seg_cnt = 0;
	return 1;
}

static void
lpfc_handle_fcp_err(struct lpfc_scsi_buf *lpfc_cmd)
{
	struct scsi_cmnd *cmnd = lpfc_cmd->pCmd;
	struct fcp_cmnd *fcpcmd = lpfc_cmd->fcp_cmnd;
	struct fcp_rsp *fcprsp = lpfc_cmd->fcp_rsp;
	struct lpfc_hba *phba = lpfc_cmd->scsi_hba;
	uint32_t fcpi_parm = lpfc_cmd->cur_iocbq.iocb.un.fcpi.fcpi_parm;
	uint32_t resp_info = fcprsp->rspStatus2;
	uint32_t scsi_status = fcprsp->rspStatus3;
	uint32_t host_status = DID_OK;
	uint32_t rsplen = 0;

	/*
	 *  If this is a task management command, there is no
	 *  scsi packet associated with this lpfc_cmd.  The driver
	 *  consumes it.
	 */
	if (fcpcmd->fcpCntl2) {
		scsi_status = 0;
		goto out;
	}

	lpfc_printf_log(phba, KERN_WARNING, LOG_FCP,
			"%d:0730 FCP command failed: RSP "
			"Data: x%x x%x x%x x%x x%x x%x\n",
			phba->brd_no, resp_info, scsi_status,
			be32_to_cpu(fcprsp->rspResId),
			be32_to_cpu(fcprsp->rspSnsLen),
			be32_to_cpu(fcprsp->rspRspLen),
			fcprsp->rspInfo3);

	if (resp_info & RSP_LEN_VALID) {
		rsplen = be32_to_cpu(fcprsp->rspRspLen);
		if ((rsplen != 0 && rsplen != 4 && rsplen != 8) ||
		    (fcprsp->rspInfo3 != RSP_NO_FAILURE)) {
			host_status = DID_ERROR;
			goto out;
		}
	}

	if ((resp_info & SNS_LEN_VALID) && fcprsp->rspSnsLen) {
		uint32_t snslen = be32_to_cpu(fcprsp->rspSnsLen);
		if (snslen > SCSI_SENSE_BUFFERSIZE)
			snslen = SCSI_SENSE_BUFFERSIZE;

		memcpy(cmnd->sense_buffer, &fcprsp->rspInfo0 + rsplen, snslen);
	}

	cmnd->resid = 0;
	if (resp_info & RESID_UNDER) {
		cmnd->resid = be32_to_cpu(fcprsp->rspResId);

		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				"%d:0716 FCP Read Underrun, expected %d, "
				"residual %d Data: x%x x%x x%x\n", phba->brd_no,
				be32_to_cpu(fcpcmd->fcpDl), cmnd->resid,
				fcpi_parm, cmnd->cmnd[0], cmnd->underflow);

		/*
		 * The cmnd->underflow is the minimum number of bytes that must
		 * be transfered for this command.  Provided a sense condition is
		 * not present, make sure the actual amount transferred is at
		 * least the underflow value or fail.
		 */
		if (!(resp_info & SNS_LEN_VALID) &&
		    (scsi_status == SAM_STAT_GOOD) &&
		    (cmnd->request_bufflen - cmnd->resid) < cmnd->underflow) {
			lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
					"%d:0717 FCP command x%x residual "
					"underrun converted to error "
					"Data: x%x x%x x%x\n", phba->brd_no,
					cmnd->cmnd[0], cmnd->request_bufflen,
					cmnd->resid, cmnd->underflow);

			host_status = DID_ERROR;
		}
	} else if (resp_info & RESID_OVER) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_FCP,
				"%d:0720 FCP command x%x residual "
				"overrun error. Data: x%x x%x \n",
				phba->brd_no, cmnd->cmnd[0], 
				cmnd->request_bufflen, cmnd->resid);
		host_status = DID_ERROR;

	/*
	 * Check SLI validation that all the transfer was actually done
	 * (fcpi_parm should be zero). Apply check only to reads.
	 */
	} else if ((scsi_status == SAM_STAT_GOOD) && fcpi_parm &&
			(cmnd->sc_data_direction == DMA_FROM_DEVICE)) {
		lpfc_printf_log(phba, KERN_WARNING, LOG_FCP,
			"%d:0734 FCP Read Check Error Data: "
			"x%x x%x x%x x%x\n", phba->brd_no,
			be32_to_cpu(fcpcmd->fcpDl),
			be32_to_cpu(fcprsp->rspResId),
			fcpi_parm, cmnd->cmnd[0]);
		host_status = DID_ERROR;
		cmnd->resid = cmnd->request_bufflen;
	}

 out:
	cmnd->result = ScsiResult(host_status, scsi_status);
}

static void
lpfc_scsi_cmd_iocb_cmpl(struct lpfc_hba *phba, struct lpfc_iocbq *pIocbIn,
			struct lpfc_iocbq *pIocbOut)
{
	struct lpfc_scsi_buf *lpfc_cmd =
		(struct lpfc_scsi_buf *) pIocbIn->context1;
	struct lpfc_target *target = lpfc_cmd->target;
	struct scsi_cmnd *cmd = lpfc_cmd->pCmd;

	lpfc_cmd->result = pIocbOut->iocb.un.ulpWord[4];
	lpfc_cmd->status = pIocbOut->iocb.ulpStatus;

	pci_dma_sync_single_for_cpu(phba->pcidev, lpfc_cmd->dma_ext.phys,
			LPFC_BPL_SIZE, PCI_DMA_FROMDEVICE);

	target->iodonecnt++;

	if (lpfc_cmd->status) {
		target->errorcnt++;

		if (lpfc_cmd->status == IOSTAT_LOCAL_REJECT &&
		    (lpfc_cmd->result & IOERR_DRVR_MASK))
			lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
		else if (lpfc_cmd->status >= IOSTAT_CNT)
			lpfc_cmd->status = IOSTAT_DEFAULT;

		lpfc_printf_log(phba, KERN_WARNING, LOG_FCP,
				"%d:0729 FCP cmd x%x failed <%d/%d> status: "
				"x%x result: x%x Data: x%x x%x\n",
				phba->brd_no, cmd->cmnd[0], cmd->device->id,
				cmd->device->lun, lpfc_cmd->status,
				lpfc_cmd->result, pIocbOut->iocb.ulpContext,
				lpfc_cmd->cur_iocbq.iocb.ulpIoTag);

		switch (lpfc_cmd->status) {
		case IOSTAT_FCP_RSP_ERROR:
			/* Call FCP RSP handler to determine result */
			lpfc_handle_fcp_err(lpfc_cmd);
			break;
		case IOSTAT_NPORT_BSY:
		case IOSTAT_FABRIC_BSY:
			cmd->result = ScsiResult(DID_BUS_BUSY, 0);
			break;
		default:
			cmd->result = ScsiResult(DID_ERROR, 0);
			break;
		}

		if (target->pnode) {
			if(target->pnode->nlp_state != NLP_STE_MAPPED_NODE)
				cmd->result = ScsiResult(DID_BUS_BUSY,
					SAM_STAT_BUSY);
		}
		else {
			cmd->result = ScsiResult(DID_NO_CONNECT, 0);
		}
	} else {
		cmd->result = ScsiResult(DID_OK, 0);
	}

	if (cmd->result || lpfc_cmd->fcp_rsp->rspSnsLen) {
		uint32_t *lp = (uint32_t *)cmd->sense_buffer;

		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				"%d:0710 Iodone <%d/%d> cmd %p, error x%x "
				"SNS x%x x%x Data: x%x x%x\n",
				phba->brd_no, cmd->device->id,
				cmd->device->lun, cmd, cmd->result,
				*lp, *(lp + 3), cmd->retries, cmd->resid);
	}

	lpfc_free_scsi_buf(lpfc_cmd);
	cmd->host_scribble = NULL;
	cmd->scsi_done(cmd);
}

static int
lpfc_scsi_prep_task_mgmt_cmd(struct lpfc_hba *phba,
			     struct lpfc_scsi_buf *lpfc_cmd,
			     uint8_t task_mgmt_cmd)
{

	struct lpfc_sli *psli;
	struct lpfc_iocbq *piocbq;
	IOCB_t *piocb;
	struct fcp_cmnd *fcp_cmnd;
	struct scsi_device *scsi_dev = lpfc_cmd->pCmd->device;
	struct lpfc_target *target = scsi_dev->hostdata;
	struct lpfc_nodelist *ndlp = target->pnode;

	if ((ndlp == 0) || (ndlp->nlp_state != NLP_STE_MAPPED_NODE)) {
		return 0;
	}

	/* allocate an iocb command */
	psli = &phba->sli;
	piocbq = &(lpfc_cmd->cur_iocbq);
	piocb = &piocbq->iocb;


	fcp_cmnd = lpfc_cmd->fcp_cmnd;
	putLunHigh(fcp_cmnd->fcpLunMsl, 0);
	putLunLow(fcp_cmnd->fcpLunLsl, lpfc_cmd->pCmd->device->lun)
	fcp_cmnd->fcpCntl2 = task_mgmt_cmd;

	piocb->ulpIoTag =
	    lpfc_sli_next_iotag(phba, &phba->sli.ring[psli->fcp_ring]);
	piocb->ulpCommand = CMD_FCP_ICMND64_CR;

	piocb->ulpContext = ndlp->nlp_rpi;
	if (ndlp->nlp_fcp_info & NLP_FCP_2_DEVICE) {
		piocb->ulpFCP2Rcvy = 1;
	}
	piocb->ulpClass = (ndlp->nlp_fcp_info & 0x0f);

	/* ulpTimeout is only one byte */
	if (lpfc_cmd->timeout > 0xff) {
		/*
		 * Do not timeout the command at the firmware level.
		 * The driver will provide the timeout mechanism.
		 */
		piocb->ulpTimeout = 0;
	} else {
		piocb->ulpTimeout = lpfc_cmd->timeout;
	}

	lpfc_cmd->target = target;

	switch (task_mgmt_cmd) {
	case FCP_LUN_RESET:
		/* Issue LUN Reset to TGT <num> LUN <num> */
		lpfc_printf_log(phba,
				KERN_INFO,
				LOG_FCP,
				"%d:0703 Issue LUN Reset to TGT %d LUN %d "
				"Data: x%x x%x\n",
				phba->brd_no,
				scsi_dev->id, scsi_dev->lun,
				ndlp->nlp_rpi, ndlp->nlp_flag);

		break;
	case FCP_ABORT_TASK_SET:
		/* Issue Abort Task Set to TGT <num> LUN <num> */
		lpfc_printf_log(phba,
				KERN_INFO,
				LOG_FCP,
				"%d:0701 Issue Abort Task Set to TGT %d LUN %d "
				"Data: x%x x%x\n",
				phba->brd_no,
				scsi_dev->id, scsi_dev->lun,
				ndlp->nlp_rpi, ndlp->nlp_flag);

		break;
	case FCP_TARGET_RESET:
		/* Issue Target Reset to TGT <num> */
		lpfc_printf_log(phba,
				KERN_INFO,
				LOG_FCP,
				"%d:0702 Issue Target Reset to TGT %d "
				"Data: x%x x%x\n",
				phba->brd_no,
				scsi_dev->id, ndlp->nlp_rpi,
				ndlp->nlp_flag);
		break;
	}

	return (1);
}

static int
lpfc_scsi_tgt_reset(struct lpfc_scsi_buf * lpfc_cmd, struct lpfc_hba * phba)
{
	struct lpfc_iocbq *piocbq, *piocbqrsp;
	struct lpfc_sli *psli;
	int ret;

	ret = lpfc_scsi_prep_task_mgmt_cmd(phba, lpfc_cmd, FCP_TARGET_RESET);
	if (!ret)
		return FAILED;

	lpfc_cmd->scsi_hba = phba;
	psli = &phba->sli;
	piocbq = &lpfc_cmd->cur_iocbq;
	piocbqrsp = mempool_alloc(phba->iocb_mem_pool,
				  GFP_ATOMIC);
	if (!piocbqrsp)
		return FAILED;

	/* First flush all outstanding commands on the txq for the target */
	lpfc_sli_abort_iocb_tgt(phba, &phba->sli.ring[phba->sli.fcp_ring],
				lpfc_cmd->pCmd->device->id, LPFC_ABORT_TXQ);

	memset(piocbqrsp, 0, sizeof (struct lpfc_iocbq));

	piocbq->iocb_flag |= LPFC_IO_POLL;

	ret = lpfc_sli_issue_iocb_wait_high_priority(phba,
		     &phba->sli.ring[psli->fcp_ring],
		     piocbq, SLI_IOCB_HIGH_PRIORITY,
		     piocbqrsp,
		     lpfc_cmd->timeout);
	if (ret != IOCB_SUCCESS) {
		lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
	} else {
		lpfc_cmd->result = piocbqrsp->iocb.un.ulpWord[4];
		lpfc_cmd->status = piocbqrsp->iocb.ulpStatus;
		if (lpfc_cmd->status == IOSTAT_LOCAL_REJECT &&
			(lpfc_cmd->result & IOERR_DRVR_MASK))
				lpfc_cmd->status = IOSTAT_DRIVER_REJECT;
	}

	/* At this point in time, target reset completion, all outstanding
	 * txcmplq I/Os should have been aborted by the target.
	 * Unfortunately, all targets do not abide by this so we need
	 * to help it out a bit.
	 */
	lpfc_sli_abort_iocb_tgt(phba, &phba->sli.ring[phba->sli.fcp_ring],
				lpfc_cmd->pCmd->device->id, LPFC_ABORT_ALLQ);

	/* Done with piocbqrsp, return to free list. */
	mempool_free(piocbqrsp, phba->iocb_mem_pool);
	return ret;
}


int
lpfc_reset_bus_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host *shost = cmnd->device->host;
	struct lpfc_hba *phba = (struct lpfc_hba *)shost->hostdata[0];
	struct lpfc_target *target;
	int ret = FAILED, i, err_count = 0;
	int cnt, loopcnt;
	struct scsi_cmnd *lnx_cmd;
	struct scsi_device *scsi_dev;
	struct lpfc_scsi_buf * lpfc_cmd;

	lpfc_cmd = lpfc_get_scsi_buf(phba, GFP_ATOMIC);
	if (!lpfc_cmd)
		goto out;

	lnx_cmd = kmalloc(sizeof(struct scsi_cmnd), GFP_ATOMIC);
	if (!lnx_cmd)
		goto out_free_scsi_buf;

	scsi_dev = kmalloc(sizeof(struct scsi_device), GFP_ATOMIC);
	if (!scsi_dev)
		goto out_free_lnx_cmd;

	/* The lpfc_cmd storage is reused.  Set all loop invariants. */
	lpfc_cmd->timeout = cmnd->timeout;
	if (!cmnd->timeout)
		 lpfc_cmd->timeout = LPFC_DRVR_TIMEOUT;

	lpfc_cmd->pCmd			= lnx_cmd;
	lpfc_cmd->scsi_hba		= phba;
	lpfc_cmd->pCmd->device		= scsi_dev;
	lpfc_cmd->pCmd->device->channel = 0;
	lpfc_cmd->pCmd->device->lun	= 0;

	/*
	 * Since the driver manages a single bus device, reset all
	 * targets known to the driver.  Should any target reset
	 * fail, this routine returns failure to the midlayer.
	 */
	for (i = 0; i < MAX_FCP_TARGET; i++) {
		target = phba->device_queue_hash[i];
		if (!target)
			continue;

		lpfc_cmd->pCmd->device->id = i;
		lpfc_cmd->pCmd->device->hostdata = target;
		ret = lpfc_scsi_tgt_reset(lpfc_cmd, phba);
		if (ret != IOCB_SUCCESS) {
			lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
				"%d:0713 Bus Reset on target %d failed\n",
				phba->brd_no, i);
			err_count++;
		}
	}

	loopcnt = 0;
	while((cnt = lpfc_sli_sum_iocb_host(phba,
				&phba->sli.ring[phba->sli.fcp_ring]))) {
		spin_unlock_irq(phba->host->host_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)
		mdelay(50);
#else
		msleep(50);
#endif
		spin_lock_irq(phba->host->host_lock);

		/* 50ms * 100 = 5 sec
		 * wait upto 5 seconds for all I/Os for this HBA to cmpl
		 */
		if(++loopcnt >= 100)
			break;
	}

	if(cnt) {
		/* flush all outstanding commands on the host */
		i = lpfc_sli_abort_iocb_host(phba,
				&phba->sli.ring[phba->sli.fcp_ring],
				LPFC_ABORT_ALLQ);

		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
		   "%d:0715 Bus Reset I/O flush failure: cnt x%x left x%x\n",
		   phba->brd_no, cnt, i);
	}
	if (!err_count)
		ret = SUCCESS;

out_free_lnx_cmd:
	kfree(lnx_cmd);
out_free_scsi_buf:
	lpfc_free_scsi_buf(lpfc_cmd);
	lpfc_printf_log(phba,
			KERN_ERR,
			LOG_FCP,
			"%d:0714 SCSI layer issued Bus Reset Data: x%x\n",
			phba->brd_no, ret);
out:
	return ret;
}


int
lpfc_queuecommand(struct scsi_cmnd *cmnd, void (*done) (struct scsi_cmnd *))
{
	struct lpfc_hba *phba =
		(struct lpfc_hba *) cmnd->device->host->hostdata[0];
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_target *targetp = cmnd->device->hostdata;
	struct lpfc_nodelist *ndlp;
	struct lpfc_iocbq *piocbq;
	struct lpfc_scsi_buf *lpfc_cmd;
	IOCB_t *piocb;
	int err = 0;
	uint16_t nlp_state;

	targetp->qcmdcnt++;

	/*
	 * The target pointer is guaranteed not to be NULL because the driver
	 * only clears the device->hostdata field in lpfc_slave_destroy.  This
	 * approach guarantees no further IO calls on this target.
	 */
	ndlp =  targetp->pnode;
	if (!ndlp) {
		if(phba->fc_flag & FC_UNLOADING) {
			/* Driver is in process of unloading */
			cmnd->result = ScsiResult(DID_BAD_TARGET, 0);
		}
		else {
			cmnd->result = ScsiResult(DID_NO_CONNECT, 0);
		}
		goto out_fail_command;
	}

	nlp_state = ndlp->nlp_state;

	/*
	 * A Fibre Channel is present and functioning only when the node state
	 * is MAPPED.  Any other state is a failure.
	 */
	if (nlp_state != NLP_STE_MAPPED_NODE) {
		if ((nlp_state == NLP_STE_UNMAPPED_NODE) ||
		    (nlp_state == NLP_STE_UNUSED_NODE)) {
			cmnd->result = ScsiResult(DID_NO_CONNECT, 0);
			goto out_fail_command;
		}
		/*
		 * The device is most likely recovered and the driver
		 * needs a bit more time to finish.  Ask the midlayer
		 * to retry.
		 */
		goto out_host_busy;
	}

	lpfc_cmd = lpfc_get_scsi_buf(phba, GFP_ATOMIC);
	if (!lpfc_cmd)
		goto out_host_busy;

	/*
	 * Store the midlayer's command structure for the completion phase
	 * and complete the command initialization.
	 */
	cmnd->scsi_done = done;
	cmnd->host_scribble = (unsigned char *)lpfc_cmd;

	lpfc_cmd->target = targetp;
	lpfc_cmd->timeout = 0;
	lpfc_cmd->pCmd = cmnd;
	putLunHigh(lpfc_cmd->fcp_cmnd->fcpLunMsl, lpfc_cmd->pCmd->device->lun);
	putLunLow(lpfc_cmd->fcp_cmnd->fcpLunLsl, lpfc_cmd->pCmd->device->lun);

	err = lpfc_os_prep_io(phba, lpfc_cmd);
	if (err)
		goto out_host_busy_free_buf;

	piocbq = &(lpfc_cmd->cur_iocbq);
	piocb = &piocbq->iocb;
	piocb->ulpTimeout = lpfc_cmd->timeout;
	piocbq->context1 = lpfc_cmd;
	piocbq->iocb_cmpl = lpfc_scsi_cmd_iocb_cmpl;

	piocbq->iocb.ulpContext = ndlp->nlp_rpi;
	if (ndlp->nlp_fcp_info & NLP_FCP_2_DEVICE) {
		piocbq->iocb.ulpFCP2Rcvy = 1;
	}

	piocbq->iocb.ulpClass = (ndlp->nlp_fcp_info & 0x0f);

	pci_dma_sync_single_for_device(phba->pcidev, lpfc_cmd->dma_ext.phys,
			LPFC_SCSI_DMA_EXT_SIZE, PCI_DMA_TODEVICE);

	err = lpfc_sli_issue_iocb(phba, &phba->sli.ring[psli->fcp_ring], piocbq,
				 SLI_IOCB_RET_IOCB);
	if (err)
		goto out_host_busy_free_buf;
	return 0;

 out_host_busy_free_buf:
	lpfc_free_scsi_buf(lpfc_cmd);
 out_host_busy:
	targetp->iodonecnt++;
	targetp->errorcnt++;
	return SCSI_MLQUEUE_HOST_BUSY;

 out_fail_command:
	targetp->iodonecnt++;
	targetp->errorcnt++;
	done(cmnd);
	return 0;
}

int
lpfc_reset_lun_handler(struct scsi_cmnd *cmnd)
{
	struct Scsi_Host *shost = cmnd->device->host;
	struct lpfc_hba *phba = (struct lpfc_hba *)shost->hostdata[0];
	struct lpfc_sli *psli = &phba->sli;
	struct lpfc_scsi_buf *lpfc_cmd;
	struct lpfc_iocbq *piocbq, *piocbqrsp = NULL;
	int ret = FAILED;
	int cnt, loopcnt;

	lpfc_cmd = lpfc_get_scsi_buf(phba, GFP_ATOMIC);
	if (!lpfc_cmd)
		goto out;

	lpfc_cmd->pCmd = cmnd;
	lpfc_cmd->timeout = 60; /* set command timeout to 60 seconds */
	lpfc_cmd->scsi_hba = phba;

	ret = lpfc_scsi_prep_task_mgmt_cmd(phba, lpfc_cmd, FCP_LUN_RESET);
	if (!ret)
		goto out_free_scsi_buf;

	piocbq = &lpfc_cmd->cur_iocbq;

	/* get a buffer for this IOCB command response */
	piocbqrsp = mempool_alloc(phba->iocb_mem_pool, GFP_ATOMIC);
	if(!piocbqrsp)
		goto out_free_scsi_buf;

	/* First flush all outstanding commands on the txq for the lun */
	lpfc_sli_abort_iocb_lun(phba,
				&phba->sli.ring[phba->sli.fcp_ring],
				cmnd->device->id,
				cmnd->device->lun, LPFC_ABORT_TXQ);

	memset(piocbqrsp, 0, sizeof (struct lpfc_iocbq));

	piocbq->iocb_flag |= LPFC_IO_POLL;
	piocbq->iocb_cmpl = lpfc_sli_wake_iocb_high_priority;

	ret = lpfc_sli_issue_iocb_wait_high_priority(phba,
		     &phba->sli.ring[psli->fcp_ring],
		     piocbq, 0,
		     piocbqrsp, 60);
	if (ret == IOCB_SUCCESS)
		ret = SUCCESS;

	lpfc_cmd->result = piocbqrsp->iocb.un.ulpWord[4];
	lpfc_cmd->status = piocbqrsp->iocb.ulpStatus;
	if (lpfc_cmd->status == IOSTAT_LOCAL_REJECT)
		if (lpfc_cmd->result & IOERR_DRVR_MASK)
			lpfc_cmd->status = IOSTAT_DRIVER_REJECT;

	/* At this point in time, lun reset completion, all outstanding
	 * txcmplq I/Os should have been aborted by the target.
	 * Unfortunately, all targets do not abide by this so we need
	 * to help it out a bit.
	 */
	lpfc_sli_abort_iocb_lun(phba,
				&phba->sli.ring[phba->sli.fcp_ring],
				cmnd->device->id,
				cmnd->device->lun, LPFC_ABORT_ALLQ);

	loopcnt = 0;
	while((cnt = lpfc_sli_sum_iocb_lun(phba,
				&phba->sli.ring[phba->sli.fcp_ring],
				cmnd->device->id,
				cmnd->device->lun))) {
		spin_unlock_irq(phba->host->host_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)
		mdelay(50);
#else
		msleep(50);
#endif
		spin_lock_irq(phba->host->host_lock);

		/* 50ms * 100 = 5 sec
		 * wait upto 5 seconds for all I/Os for this lun to cmpl
		 */
		if(++loopcnt >= 100)
			break;
	}

	if(cnt) {
		lpfc_printf_log(phba, KERN_INFO, LOG_FCP,
			"%d:0719 LUN Reset I/O flush failure: cnt x%x\n",
			phba->brd_no, cnt);
	}

	mempool_free(piocbqrsp, phba->iocb_mem_pool);

out_free_scsi_buf:
	lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
			"%d:0713 SCSI layer issued LUN reset (%d, %d) "
			"Data: x%x x%x x%x\n",
			phba->brd_no, lpfc_cmd->pCmd->device->id,
			lpfc_cmd->pCmd->device->lun, ret, lpfc_cmd->status,
			lpfc_cmd->result);
	lpfc_free_scsi_buf(lpfc_cmd);
out:
	return ret;
}


int
lpfc_abort_handler(struct scsi_cmnd *cmnd)
{
	struct lpfc_hba *phba =
			(struct lpfc_hba *)cmnd->device->host->hostdata[0];
	struct lpfc_sli_ring *pring = &phba->sli.ring[phba->sli.fcp_ring];
	struct lpfc_iocbq *iocb, *next_iocb, *abtsiocbp;
	struct lpfc_scsi_buf *lpfc_cmd;
	IOCB_t *cmd, *icmd;
	unsigned long snum;
	unsigned int id, lun;
	int ret = IOCB_ERROR;

	/* Returns SUCESS if command aborted, else FAILED */

	lpfc_cmd = (struct lpfc_scsi_buf *)cmnd->host_scribble;
	if (!lpfc_cmd)
       		return(FAILED);

	/* save these now since lpfc_cmd can be freed */
	id   = lpfc_cmd->pCmd->device->id;
	lun  = lpfc_cmd->pCmd->device->lun;
	snum = lpfc_cmd->pCmd->serial_number;

	/* Search the txq first. */
	list_for_each_entry_safe(iocb, next_iocb, &pring->txq, list) {
		cmd = &iocb->iocb;
		if (iocb->context1 != lpfc_cmd)
			continue;

		list_del_init(&iocb->list);
		pring->txq_cnt--;
		if (!iocb->iocb_cmpl) {
			mempool_free(iocb, phba->iocb_mem_pool);
		}
		else {
			cmd->ulpStatus = IOSTAT_LOCAL_REJECT;
			cmd->un.ulpWord[4] = IOERR_SLI_ABORTED;
			(iocb->iocb_cmpl)(phba, iocb, iocb);
		}
		ret = IOCB_SUCCESS;
       		goto out;
	}

	abtsiocbp = mempool_alloc(phba->iocb_mem_pool, GFP_ATOMIC);
	if (!abtsiocbp)
		goto out;
	memset(abtsiocbp, 0, sizeof (struct lpfc_iocbq));

	/*
	 * The scsi command was not in the txq.  Check the txcmplq and if it is
	 * found, send an abort to the FW. 
	 */
	list_for_each_entry_safe(iocb, next_iocb, &pring->txcmplq, list) {
		if (iocb->context1 != lpfc_cmd)
			continue;

		iocb->iocb_cmpl = NULL;
		cmd = &iocb->iocb;
		icmd = &abtsiocbp->iocb;
		icmd->un.acxri.abortType = ABORT_TYPE_ABTS;
		icmd->un.acxri.abortContextTag = cmd->ulpContext;
		icmd->un.acxri.abortIoTag = cmd->ulpIoTag;

		icmd->ulpLe = 1;
		icmd->ulpClass = cmd->ulpClass;
		icmd->ulpIoTag = lpfc_sli_next_iotag(phba, pring);
		if (phba->hba_state >= LPFC_LINK_UP)
			icmd->ulpCommand = CMD_ABORT_XRI_CN;
		else
			icmd->ulpCommand = CMD_CLOSE_XRI_CN;

		/* set up an iotag  */
		icmd->ulpIoTag = lpfc_sli_next_iotag(phba, pring);

		if (lpfc_sli_issue_iocb(phba, pring, abtsiocbp, 0) ==
								IOCB_ERROR) {
			mempool_free(abtsiocbp, phba->iocb_mem_pool);
			break;
		}

		/*
		 * The rsp ring completion will remove IOCB from txcmplq when
		 * abort is read by HBA.
		 */
		ret = IOCB_SUCCESS;
		break;
	}

 out:
	lpfc_printf_log(phba, KERN_ERR, LOG_FCP,
			"%d:0749 SCSI layer issued abort device "
			"Data: x%x x%x x%x x%lx\n",
			phba->brd_no, ret, id, lun, snum);

	return (ret == IOCB_SUCCESS ? SUCCESS : FAILED);
}

#if defined(FC_TRANS_VER1) || defined(FC_TRANS_265_BLKPATCH)
void
lpfc_target_unblock(struct lpfc_hba *phba, struct lpfc_target *targetp)
{
#if defined(FC_TRANS_VER1)
	/*
	 * This code to be removed once block/unblock and the new
	 * dicovery state machine are fully debugged.
	 */
	if (!targetp || !targetp->starget) {
#else
	if (!targetp) {
#endif
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY | LOG_FCP,
			"%d:0262 Cannot unblock scsi target\n", phba->brd_no);

		return;
	}

	/* Unblock IO to target scsi id <sid> to NPort <nlp_DID> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY | LOG_FCP,
			"%d:0258 Unblocking IO to Target scsi id x%x  "
			"NPort pointer x%p\n",
			phba->brd_no, targetp->scsi_id, targetp->pnode);

	spin_unlock_irq(phba->host->host_lock);

#if defined(FC_TRANS_VER1)
	fc_target_unblock(targetp->starget);
#elif defined(FC_TRANS_265_BLKPATCH)
	fc_target_unblock(phba->host, targetp->scsi_id,
			  &targetp->dev_loss_timer);
#endif
	spin_lock_irq(phba->host->host_lock);
	targetp->blocked--;
}

void
lpfc_target_block(struct lpfc_hba *phba, struct lpfc_target *targetp)
{
#if defined(FC_TRANS_VER1)
	/*
	 * This code to be removed once block/unblock and the new
	 * dicovery state machine are fully debugged.
	 */
	if (!targetp || !targetp->starget) {
#else
	if (!targetp) {
#endif
		lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY | LOG_FCP,
				"%d:0263 Cannot block scsi target."
				" target ptr x%p\n",
				phba->brd_no, targetp);
		return;
	}

	/* Block all IO to target scsi id <sid> to NPort <nlp_DID> */
	lpfc_printf_log(phba, KERN_INFO, LOG_DISCOVERY | LOG_FCP,
			"%d:0259 Blocking IO to Target scsi id x%x"
			" NPort pointer x%p\n",
			phba->brd_no, targetp->scsi_id, targetp->pnode);

	spin_unlock_irq(phba->host->host_lock);
#if defined(FC_TRANS_VER1)
	fc_target_block(targetp->starget);
#elif defined(FC_TRANS_265_BLKPATCH)
	fc_target_block(phba->host, targetp->scsi_id, &targetp->dev_loss_timer,
			phba->cfg_nodev_tmo);
#endif
	spin_lock_irq(phba->host->host_lock);
	targetp->blocked++;
}
#endif

#if defined(FC_TRANS_VER1) || defined(FC_TRANS_265_BLKPATCH)
int
lpfc_target_remove(struct lpfc_hba *phba, struct lpfc_target *targetp)
{
	struct scsi_device *sdev;
	struct Scsi_Host   *shost = phba->host;

	/* This is only called if scsi target (targetp->starget) is valid */
	lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY | LOG_FCP,
			"%d:0260 Remove Target scsi id x%x\n",
			phba->brd_no, targetp->scsi_id);

	/* If this target is blocked, we must unblock it first */
	if (targetp->blocked)
		lpfc_target_unblock(phba, targetp);

	/* Remove all associated devices for this target */
	if (phba->cfg_scsi_hotplug) {
top:
		list_for_each_entry(sdev, &shost->__devices, siblings) {
			if (sdev->channel == 0
			    && sdev->id == targetp->scsi_id) {
				spin_unlock_irq(shost->host_lock);
				scsi_device_get(sdev);
				scsi_remove_device(sdev);
				scsi_device_put(sdev);
				spin_lock_irq(shost->host_lock);
				goto top;
			}
		}
	}

	return 0;
}

int
lpfc_target_add(struct lpfc_hba *phba, struct lpfc_target *targetp)
{
	struct Scsi_Host   *shost;

	if(!phba->cfg_scsi_hotplug)
		return 1;

	/* This is only called if scsi target (targetp->starget) is valid */

	lpfc_printf_log(phba, KERN_ERR, LOG_DISCOVERY | LOG_FCP,
			"%d:0261 Adding Target scsi id x%x\n",
			phba->brd_no, targetp->scsi_id);

	shost = phba->host;

#ifdef USE_SCAN_TARGET
	lpfc_discq_post_event(phba, targetp, NULL, LPFC_EVT_SCAN);
#else
	/*
	 * The driver discovered a new target.  Call the midlayer and get this
	 * target's luns added into the device list.
	 * Since we are going to scan the entire host, kick off a timer to 
	 * do this so we can possibly consolidate multiple target scans into
	 * one scsi host scan.
	 */
	mod_timer(&phba->fc_scantmo, jiffies + HZ);
	phba->fc_flag |= FC_SCSI_SCAN_TMO;
#endif
	return 0;
}
#endif