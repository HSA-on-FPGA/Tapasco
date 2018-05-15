//
// Copyright (C) 2017 Jaco A. Hofmann, TU Darmstadt
//
// This file is part of ThreadPoolComposer (TPC).
//
// ThreadPoolComposer is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// ThreadPoolComposer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with ThreadPoolComposer.  If not, see <http://www.gnu.org/licenses/>.
//
/**
 * @file char_device_hsa.h
 * @brief TODO
 * */

#ifndef __CHAR_DEVICE_HSA_H
#define __CHAR_DEVICE_HSA_H

/******************************************************************************/
/* Include section */

/* Includes from linux headers */
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mm.h>
//#include <linux/pagemap.h>
//#include <linux/gfp.h>
//#include <linux/dma-mapping.h>
//#include <linux/delay.h>
#include <linux/slab.h>
//#include <linux/clk.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/aio.h>
#include <linux/uio.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
//#include <linux/kernel.h>
//#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/sched.h>

/* Includes from local files */
#include "common/debug_print.h"
#include "common/device_dma.h"
#include "common/device_pcie.h"
#include "include/hsa_ioctl_calls.h"

/******************************************************************************/

#define HSA_UPDATE_RATE 30000

#define HSA_MEMORY_BASE_ADDR 0x0001000000000000

#define HSA_ARBITER_BASE_ADDR 0x600000

#define HSA_ARBITER_REGISTER_HOST_ADDR 0
#define HSA_ARBITER_REGISTER_READ_INDEX_ADDR 8
#define HSA_ARBITER_REGISTER_FPGA_ADDR 16
#define HSA_ARBITER_REGISTER_QUEUE_SIZE 40
#define HSA_ARBITER_REGISTER_UPDATE_RATE 48
#define HSA_ARBITER_REGISTER_PASID_ADDR 64
#define HSA_ARBITER_REGISTER_WRITE_INDEX_ADDR 72

#define HSA_ARBITER_REGISTER_ID 24
#define HSA_ARBITER_ID 0xE5A0024

#define HSA_SIGNAL_BASE_ADDR 0x000000
#define HSA_SIGNAL_ACK 0
#define HSA_SIGNAL_ADDR 0x80
#define HSA_SIGNAL_REGISTER_ID 24
#define HSA_SIGNAL_ID 0xE5A0025

#define FFLINK_HSA_NAME "HSA_AQL_QUEUE"

/******************************************************************************/

/* struct array to hold data over multiple fops-calls */
struct priv_data_struct {
	struct hsa_mmap_space * kvirt_shared_mem;

	dma_addr_t dma_shared_mem;

	uint8_t signal_allocated[HSA_SIGNALS];
};

/******************************************************************************/
/* functions for user-space interaction */

static int hsa_open(struct inode *, struct file *);
static int hsa_close(struct inode *, struct file *);
static long hsa_ioctl(struct file *, unsigned int, unsigned long);
static ssize_t hsa_read(struct file *, char __user *, size_t count, loff_t *);
static ssize_t hsa_write(struct file *, const char __user *, size_t count, loff_t *);
static int hsa_mmap(struct file *, struct vm_area_struct *vma);

/******************************************************************************/
/* helper functions called for sys-calls */
static int hsa_alloc_queue(void** p, dma_addr_t *handle);
static void hsa_free_queue(void *p, dma_addr_t handle);

/******************************************************************************/

#endif
