/**
 * en-/disable nvhda
 *
 * Usage:
 * Disable audio
 * # echo OFF > /proc/acpi/nvhda
 * Enable audio
 * # echo ON > /proc/acpi/nvhda
 * Get status
 * # cat /proc/acpi/nvhda
 */
/*  Based on bbswitch,
 *  Copyright (C) 2011-2013 Bumblebee Project
 *  Author: Peter Wu <lekensteyn@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
#include <linux/uaccess.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/suspend.h>
#include <linux/seq_file.h>
#include <linux/pm_runtime.h>

#define NVHDA_VERSION "0.01"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Toggle the discrete graphics card audio");
MODULE_AUTHOR("Peter Wu <lekensteyn@gmail.com>");
MODULE_VERSION(NVHDA_VERSION);

enum {
    CARD_UNCHANGED = -1,
    CARD_OFF = 0,
    CARD_ON = 1,
};

static int load_state = CARD_UNCHANGED;
MODULE_PARM_DESC(load_state, "Initial card state (0 = off, 1 = on, -1 = unchanged)");
module_param(load_state, int, 0400);
static int unload_state = CARD_UNCHANGED;
MODULE_PARM_DESC(unload_state, "Card state on unload (0 = off, 1 = on, -1 = unchanged)");
module_param(unload_state, int, 0600);
static bool skip_checks = false;
MODULE_PARM_DESC(skip_checks, "Skip checks, unimplemented (default = false)");
module_param(skip_checks, bool, 0400);

extern struct proc_dir_entry *acpi_root_dir;

static struct pci_dev *dis_dev;
static struct pci_dev *sub_dev = NULL;

// Returns 1 if the card is disabled, 0 if enabled
static int is_card_disabled(void) {
//check for: 1.bit is set 2.sub-function is available.
    u32 cfg_word;
    struct pci_dev *tmp_dev = NULL;
   
    sub_dev = NULL;

    // read config word at 0x488
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    if ((cfg_word & 0x2000000)==0x2000000)
    {
        //check for subdevice. read first config dword of sub function 1
        while ((tmp_dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, tmp_dev)) != NULL) {
            int pci_class = tmp_dev->class >> 8;

            if (pci_class != 0x403)
                continue;

            if (tmp_dev->vendor == PCI_VENDOR_ID_NVIDIA) {
                sub_dev = tmp_dev;
                pr_info("Found nv audio device %s\n",
                dev_name(&tmp_dev->dev));
            }
        }
        
        if (sub_dev == NULL)
        {
            pr_info("No audio device found, unsetting config bit.\n");
            cfg_word|=0x2000000;
            pci_write_config_dword(dis_dev, 0x488, cfg_word);
            return 1;
        }

        return 0;
    }
    else
    {
        return 1;
    }
}

static void nvhda_off(void) {
    u32 cfg_word;
    if (is_card_disabled())
        return;
//Driver unbind?
    //remove device
    pci_dev_put(sub_dev);
    pci_stop_and_remove_bus_device(sub_dev);

    pr_info("disabling audio\n");

    //setting bit to turn off
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    cfg_word&=0xfdffffff;
    pci_write_config_dword(dis_dev, 0x488, cfg_word);
    
}

static void nvhda_on(void) {
    u32 cfg_word;
    u8 hdr_type;

    if (!is_card_disabled())
        return;

    pr_info("enabling audio\n");

    // read,set bit, write config word at 0x488
    pci_read_config_dword(dis_dev, 0x488, &cfg_word);
    cfg_word|=0x2000000;
    pci_write_config_dword(dis_dev, 0x488, cfg_word);

    //pci_scan_single_device
	pci_read_config_byte(dis_dev, PCI_HEADER_TYPE, &hdr_type);

	if (!(hdr_type & 0x80))
    {
        pr_err("Not multifunction, no audio\n");
		return;
    }

	sub_dev = pci_scan_single_device(dis_dev->bus, 1);
	if (!sub_dev)
    {
        pr_err("No nv audio device found\n");
		return;
    }    
    pr_info("Audio found, adding\n");
	pci_assign_unassigned_bus_resources(dis_dev->bus);
	pci_bus_add_devices(dis_dev->bus);
    pci_dev_get(sub_dev);
	return;

}

/* power bus so we can read PCI configuration space */
static void dis_dev_get(void) {
    if (dis_dev->bus && dis_dev->bus->self)
        pm_runtime_get_sync(&dis_dev->bus->self->dev);
}

static void dis_dev_put(void) {
    if (dis_dev->bus && dis_dev->bus->self)
        pm_runtime_put_sync(&dis_dev->bus->self->dev);
}

static ssize_t nvhda_proc_write(struct file *fp, const char __user *buff,
    size_t len, loff_t *off) {
    char cmd[8];

    if (len >= sizeof(cmd))
        len = sizeof(cmd) - 1;

    if (copy_from_user(cmd, buff, len))
        return -EFAULT;

    dis_dev_get();

    if (strncmp(cmd, "OFF", 3) == 0)
        nvhda_off();

    if (strncmp(cmd, "ON", 2) == 0)
        nvhda_on();

    dis_dev_put();

    return len;
}

static int nvhda_proc_show(struct seq_file *seqfp, void *p) {
    // show the card state. Example output: 0000:01:00:00 ON
    dis_dev_get();
    seq_printf(seqfp, "%s %s\n", dev_name(&dis_dev->dev),
             is_card_disabled() ? "OFF" : "ON");
    dis_dev_put();
    return 0;
}
static int nvhda_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, nvhda_proc_show, NULL);
}

static struct file_operations nvhda_fops = {
    .open   = nvhda_proc_open,
    .read   = seq_read,
    .write  = nvhda_proc_write,
    .llseek = seq_lseek,
    .release= single_release
};

static int __init nvhda_init(void) {
    struct proc_dir_entry *acpi_entry;
    struct pci_dev *pdev = NULL;

    pr_info("version %s\n", NVHDA_VERSION);

    while ((pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, pdev)) != NULL) {
        int pci_class = pdev->class >> 8;

        if (pci_class != PCI_CLASS_DISPLAY_VGA &&
            pci_class != PCI_CLASS_DISPLAY_3D) // 3D class is useless since it has no outputs, just staying for testing purposes.
            continue;

        if (pdev->vendor == PCI_VENDOR_ID_NVIDIA) {
            dis_dev = pdev;
            pr_info("Found nv VGA device %s\n",
                dev_name(&pdev->dev));
        }
    }

    if (dis_dev == NULL) {
        pr_err("No nv VGA device found\n");
        return -ENODEV;
    }


    acpi_entry = proc_create("nvhda", 0664, acpi_root_dir, &nvhda_fops);
    if (acpi_entry == NULL) {
        pr_err("Couldn't create proc entry\n");
        return -ENOMEM;
    }

    dis_dev_get();

    if (load_state == CARD_ON)
        nvhda_on();
    else if (load_state == CARD_OFF)
        nvhda_off();

    pr_info("Succesfully loaded. Audio %s is %s\n",
        dev_name(&dis_dev->dev), is_card_disabled() ? "off" : "on");

    dis_dev_put();

    return 0;
}

static void __exit nvhda_exit(void) {
    remove_proc_entry("nvhda", acpi_root_dir);

    dis_dev_get();

    if (unload_state == CARD_ON)
        nvhda_on();
    else if (unload_state == CARD_OFF)
        nvhda_off();

    pr_info("Unloaded. Audio %s is %s\n",
        dev_name(&dis_dev->dev), is_card_disabled() ? "off" : "on");

    dis_dev_put();

}

module_init(nvhda_init);
module_exit(nvhda_exit);

/* vim: set sw=4 ts=4 et: */
