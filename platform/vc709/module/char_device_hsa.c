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
 * @file char_device_dma.c
 * @brief TODO
 * */

/******************************************************************************/

#include "char_device_hsa.h"

/******************************************************************************/
/* global struct and variable declarations */

/* file operations for sw-calls as a char device */
static struct file_operations hsa_fops = {
	.owner          = THIS_MODULE,
	.open           = hsa_open,
	.release        = hsa_close,
	.unlocked_ioctl = hsa_ioctl,
	.read           = hsa_read,
	.write          = hsa_write,
	.mmap           = hsa_mmap
};

/* private data used to hold additional information throughout multiple system calls */
static struct priv_data_struct priv_data;

/* char device structure basically for dev_t and fops */
static struct cdev char_hsa_cdev;
/* char device number for f3_char_driver major and minor number */
static dev_t char_hsa_dev_t;
/* device class entry for sysfs */
struct class *char_hsa_class;

static int disabled = 0;
static int device_opened = 0;
static DEFINE_SPINLOCK(device_open_close_mutex);
static DEFINE_SPINLOCK(ioctl_mutex);

                       /******************************************************************************/
                       /* helper functions used by sys-calls */

static void enable_queue_fetcher(uint64_t update_rate) {
	struct hsa_mmap_space *dma_mem = (struct hsa_mmap_space*)priv_data.dma_shared_mem;
	pcie_writeq((uint64_t)&dma_mem->queue, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_HOST_ADDR);
	pcie_writeq((uint64_t)&dma_mem->read_index, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_READ_INDEX_ADDR);
	pcie_writeq((uint64_t)&dma_mem->pasids, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_PASID_ADDR);
	pcie_writeq(HSA_QUEUE_LENGTH_MOD2, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_QUEUE_SIZE);

	pcie_writeq(HSA_MEMORY_BASE_ADDR, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_FPGA_ADDR);

	pcie_writeq(update_rate, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_UPDATE_RATE);
}

static void disable_queue_fetcher(void) {
	pcie_writeq(-1, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_WRITE_INDEX_ADDR);
	pcie_writeq(0, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_UPDATE_RATE);
}

static int hsa_alloc_queue(void** p, dma_addr_t *handle)
{
	int err = 0;

	size_t mem_size = sizeof(struct hsa_mmap_space);

	*p = dma_alloc_coherent(&get_pcie_dev()->dev, mem_size, handle, GFP_KERNEL | __GFP_HIGHMEM);
	if (*p == 0) {
		fflink_warn("Couldn't allocate %lu bytes coherent memory for the HSA Queue\n", mem_size);
	}
	fflink_warn("Got dma memory at dma address %llx and virtual address %llx", *handle, (uint64_t)*p);

	return err;
}

/**
 * @brief Free kernel buffers
 * @param p Array of kernel pages for buffer
 * @param handle Array of dma bus addresses for buffer
 * @param direction Whether writeable or readable
 * @return none
 * */
static void hsa_free_queue(void *p, dma_addr_t handle)
{
	if (p != 0) {
		size_t mem_size = sizeof(struct hsa_mmap_space);
		dma_free_coherent(&get_pcie_dev()->dev, mem_size, p, handle);
	}
}

/**
 * @brief Initializes priv_data for corresponding minor node
 * @param p Pointer to privade data for minor node
 * @param node Minor number
 * @return none
 * */
static void hsa_init_pdata(struct priv_data_struct * p)
{
	int i = 0;

	p->kvirt_shared_mem = 0;

	p->dma_shared_mem = 0;

	for (i = 0; i < HSA_SIGNALS; ++i) {
		p->signal_allocated[i] = 0;
	}
}

static int hsa_initialize(void) {
	int i;
	hsa_init_pdata(&priv_data);

	if (pcie_readl((void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_ID) != HSA_ARBITER_ID) {
		fflink_warn("Couldn't find queue fetcher core");
		goto open_failed;
	}

	if (pcie_readl((void*)HSA_SIGNAL_BASE_ADDR + HSA_SIGNAL_REGISTER_ID) != HSA_SIGNAL_ID) {
		fflink_warn("Couldn't find signal handler");
		goto open_failed;
	}

	hsa_alloc_queue((void**)&priv_data.kvirt_shared_mem, &priv_data.dma_shared_mem);

	// Invalidate all packages in the queue
	for (i = 0; i < HSA_QUEUE_LENGTH; ++i) {
		priv_data.kvirt_shared_mem->queue[i][0] = 1;
	}
	priv_data.kvirt_shared_mem->read_index = 0;

	enable_queue_fetcher(HSA_UPDATE_RATE);

	return 0;

open_failed:
	disabled = 1;
	return 0;
}

static void hsa_deinit(void) {
	fflink_notice("Device not used anymore, removing it.\n");
	if(!disabled) {
		disable_queue_fetcher();
		hsa_free_queue(priv_data.kvirt_shared_mem, priv_data.dma_shared_mem);
	}
}

/******************************************************************************/
/* functions for user-space interaction */

/**
 * @brief When minor node is opened, kernel buffers will be allocated,
    performs dma-mappings and registeres these in filp for further calls
 * @param inode Representation of node in /dev, used to get major-/minor-number
 * @param filp Mostly used to allocate private data for consecutive calls
 * @return Zero, if char-device could be opened, error code otherwise
 * */
static int hsa_open(struct inode *inode, struct file *filp)
{

	spin_lock(&device_open_close_mutex);

	fflink_notice("Already %d files in use.\n", device_opened);

	++device_opened;
	/* set filp for further sys calls to this minor number */
	filp->private_data = &priv_data;
	spin_unlock(&device_open_close_mutex);
	return 0;
}

/**
 * @brief Tidy up code, when device isn't needed anymore
 * @param inode Representation of node in /dev, used to get major-/minor-number
 * @param filp Needed to identify device node and get access to corresponding buffers
 * @return Zero, if char-device could be closed, error code otherwise
 * */
static int hsa_close(struct inode *inode, struct file *filp)
{
	spin_lock(&device_open_close_mutex);
	--device_opened;
	fflink_notice("Still %d files in use.\n", device_opened);
	spin_unlock(&device_open_close_mutex);
	return 0;
}

irqreturn_t intr_handler_hsa_signals(int irq, void * dev_id)
{
	struct hsa_mmap_space *dma_mem = (struct hsa_mmap_space*)priv_data.dma_shared_mem;
	uint64_t signal = (uint64_t)pcie_readq((void*)HSA_SIGNAL_BASE_ADDR + HSA_SIGNAL_ADDR);
	uint64_t *signal_kvirt = (uint64_t*)priv_data.kvirt_shared_mem->signals;
	signal -= (uint64_t)dma_mem->signals;
	signal /= sizeof(uint64_t);
	--signal_kvirt[signal];
	pcie_writeq(1, (void*)HSA_SIGNAL_BASE_ADDR + HSA_SIGNAL_ACK);
	return IRQ_HANDLED;
}

/******************************************************************************/
/* functions for user-space interaction */

/**
 * */
static ssize_t hsa_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	// TODO
	return -EACCES;
}

/**
 * */
static ssize_t hsa_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	// TODO

	return -EACCES;
}

/******************************************************************************/
/* function for user-space interaction */

void assign_doorbell(int offset) {
	struct hsa_mmap_space *dma_mem = (struct hsa_mmap_space*)priv_data.dma_shared_mem;
	pcie_writeq((uint64_t)&dma_mem->signals[offset], (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_WRITE_INDEX_ADDR);
}

void unassign_doorbell(void) {
	pcie_writeq(-1, (void*)HSA_ARBITER_BASE_ADDR + HSA_ARBITER_REGISTER_WRITE_INDEX_ADDR);
}

/**
 * */
static long hsa_ioctl(struct file *filp, unsigned int ioctl_num, unsigned long ioctl_param)
{
	int i;
	int err = 0;
	struct priv_data_struct * p = (struct priv_data_struct *) filp->private_data;
	struct hsa_ioctl_params params;
	struct hsa_mmap_space *dma_mem = (struct hsa_mmap_space*)priv_data.dma_shared_mem;
	fflink_notice("Called with number %X for minor %u\n", ioctl_num, 0);

	if (_IOC_SIZE(ioctl_num) != sizeof(struct hsa_ioctl_params)) {
		fflink_warn("Wrong size to read out registers %d vs %ld\n", _IOC_SIZE(ioctl_num), sizeof(struct hsa_ioctl_params));
		return -EACCES;
	}
	if (copy_from_user(&params, (void *)ioctl_param, _IOC_SIZE(ioctl_num))) {
		fflink_warn("Couldn't copy all bytes\n");
		return -EACCES;
	}

	spin_lock(&ioctl_mutex);
	switch (ioctl_num) {
	case IOCTL_CMD_HSA_SIGNAL_ALLOC:
		for(i = 0; i < HSA_SIGNALS; ++i) {
			if(p->signal_allocated[i] == 0) {
				params.addr = (void*)&dma_mem->signals[i];
				params.offset = i;
				p->signal_allocated[i] = 1;
				fflink_warn("Allocated signal %d at 0x%llx", (int)params.offset, (uint64_t)params.addr);
				if(copy_to_user((void*)ioctl_param, &params, _IOC_SIZE(ioctl_num))) {
					fflink_warn("Couldn't copy all bytes back to userspace\n");
					err = -EACCES;
					goto err_handler;
				}
				break;
			}
		}
		if(i == HSA_SIGNALS) {
			fflink_warn("No signals left.");
			err = -EACCES;
			goto err_handler;
		}
		break;
	case IOCTL_CMD_HSA_SIGNAL_DEALLOC:
		p->signal_allocated[params.offset] = 0;
		fflink_warn("Deallocated signal %d", (int)params.offset);
		break;
	case IOCTL_CMD_HSA_DOORBELL_ASSIGN:
		fflink_warn("Assign doorbell to signal %d", (int)params.offset);
		assign_doorbell(params.offset);
		break;
	case IOCTL_CMD_HSA_DOORBELL_UNASSIGN:
		fflink_warn("Unassign doorbell");
		unassign_doorbell();
		break;
	}

	err_handler:
	spin_unlock(&ioctl_mutex);
	return err;

}

static int hsa_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if (priv_data.kvirt_shared_mem == 0)
		return -EAGAIN;
	return dma_mmap_coherent(&get_pcie_dev()->dev, vma, priv_data.kvirt_shared_mem, priv_data.dma_shared_mem, vma->vm_end - vma->vm_start);
}

/******************************************************************************/
/* helper functions externally called e.g. to (un/)load this char device */

/**
 * @brief Registers char device with multiple minor nodes in /dev
 * @param none
 * @return Returns error code or zero if successful
 * */
int char_hsa_register(void)
{
	int err = 0;
	struct device *device = NULL;

	fflink_info("Try to add char_device to /dev\n");

	/* create device class to register under sysfs */
	err = alloc_chrdev_region(&char_hsa_dev_t, 0, 1, FFLINK_HSA_NAME);
	if (err < 0 || MINOR(char_hsa_dev_t) != 0) {
		fflink_warn("failed to allocate chrdev with %d minors\n", 1);
		goto error_no_device;
	}

	/* create device class to register under udev/sysfs */
	if (IS_ERR((char_hsa_class = class_create(THIS_MODULE, FFLINK_HSA_NAME)))) {
		fflink_warn("failed to create class\n");
		goto error_class_invalid;
	}

	/* initialize char dev with fops to prepare for adding */
	cdev_init(&char_hsa_cdev, &hsa_fops);
	char_hsa_cdev.owner = THIS_MODULE;

	/* try to add char dev */
	err = cdev_add(&char_hsa_cdev, char_hsa_dev_t, 1);
	if (err) {
		fflink_warn("failed to add char dev\n");
		goto error_add_to_system;
	}

	/* create device file via udev */
	device = device_create(char_hsa_class, NULL, MKDEV(MAJOR(char_hsa_dev_t), MINOR(char_hsa_dev_t)), NULL, FFLINK_HSA_NAME "_%d", MINOR(char_hsa_dev_t));
	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		fflink_warn("failed while device create %d\n", MINOR(char_hsa_dev_t));
		goto error_device_create;
	}

	return hsa_initialize();

	/* tidy up for everything successfully allocated */
error_device_create:
	device_destroy(char_hsa_class, MKDEV(MAJOR(char_hsa_dev_t), MINOR(char_hsa_dev_t)));
	cdev_del(&char_hsa_cdev);
error_add_to_system:
	class_destroy(char_hsa_class);
error_class_invalid:
	unregister_chrdev_region(char_hsa_dev_t, 1);
error_no_device:
	return -ENODEV;
}

/**
 * @brief Unregisters char device, which was initialized with dma_register before
 * @param none
 * @return none
 * */
void char_hsa_unregister(void)
{
	fflink_info("Tidy up\n");

	hsa_deinit();

	device_destroy(char_hsa_class, MKDEV(MAJOR(char_hsa_dev_t), MINOR(char_hsa_dev_t)));

	cdev_del(&char_hsa_cdev);

	class_destroy(char_hsa_class);

	unregister_chrdev_region(char_hsa_dev_t, 1);
}

/******************************************************************************/
