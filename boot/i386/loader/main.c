/*
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: releng/9.1/sys/boot/i386/loader/main.c 237766 2012-06-29 10:19:15Z avg $");

/*
 * MD bootstrap main() and assorted miscellaneous
 * commands.
 */

#include <stand.h>
#include <stddef.h>
#include <string.h>
#include <machine/bootinfo.h>
#include <machine/psl.h>
#include <sys/reboot.h>

#include "bootstrap.h"
#include "common/bootargs.h"
#include "libi386/libi386.h"
#include "btxv86.h"

#ifdef LOADER_VGAEFFECTS_SUPPORT
#include "vgaeffects.c"
#endif

#ifdef LOADER_ZFS_SUPPORT
#include "../zfs/libzfs.h"
#endif

CTASSERT(sizeof(struct bootargs) == BOOTARGS_SIZE);
CTASSERT(offsetof(struct bootargs, bootinfo) == BA_BOOTINFO);
CTASSERT(offsetof(struct bootargs, bootflags) == BA_BOOTFLAGS);
CTASSERT(offsetof(struct bootinfo, bi_size) == BI_SIZE);

/* Arguments passed in from the boot1/boot2 loader */
static struct bootargs *kargs;

static u_int32_t	initial_howto;
static u_int32_t	initial_bootdev;
static struct bootinfo	*initial_bootinfo;

struct arch_switch	archsw;		/* MI/MD interface boundary */

static void		extract_currdev(void);
int     		isa_inb(int port);
void        	isa_outb(int port, int value);
void			exit(int code);
#ifdef LOADER_ZFS_SUPPORT
static void		i386_zfs_probe(void);
#endif

/* from vers.c */
extern	char bootprog_name[], bootprog_rev[], bootprog_date[], bootprog_maker[];

/* XXX debugging */
extern char end[];

static void *heap_top;
static void *heap_bottom;

/* This is for debug only -- REMOVE after finished */
static int
IsCtlpressed()
{
v86.ctl = 0;
v86.eax = 0x0200; /* ah=2 */
v86.addr=0x16;
v86int();

return(v86.eax&4)? 1:0;

}

static int
IsShiftpressed()
{
v86.ctl = 0;
v86.eax = 0x0200; /* ah=2 */
v86.addr=0x16;
v86int();

return(v86.eax&3)? 1:0;

}


int
main(void)
{
    int			i;

    /* Show banner as early as possible */
#ifdef LOADER_VGAEFFECTS_SUPPORT
    if(!IsCtlpressed()) {
    vga_init();
    if(!IsShiftpressed()) vga_showbanner();
    else vga_slideupandexit();
    }
#endif



    /* Pick up arguments */
    kargs = (void *)__args;
    initial_howto = kargs->howto;
    initial_bootdev = kargs->bootdev;
    initial_bootinfo = kargs->bootinfo ? (struct bootinfo *)PTOV(kargs->bootinfo) : NULL;

    /* Initialize the v86 register set to a known-good state. */
    bzero(&v86, sizeof(v86));
    v86.efl = PSL_RESERVED_DEFAULT | PSL_I;

    /* 
     * Initialise the heap as early as possible.  Once this is done, malloc() is usable.
     */
    bios_getmem();

#if defined(LOADER_BZIP2_SUPPORT) || defined(LOADER_FIREWIRE_SUPPORT) || \
    defined(LOADER_GPT_SUPPORT) || defined(LOADER_ZFS_SUPPORT)
    if (high_heap_size > 0) {
	heap_top = PTOV(high_heap_base + high_heap_size);
	heap_bottom = PTOV(high_heap_base);
	if (high_heap_base < memtop_copyin)
	    memtop_copyin = high_heap_base;
    } else
#endif
    {
	heap_top = (void *)PTOV(bios_basemem);
	heap_bottom = (void *)end;
    }
    setheap(heap_bottom, heap_top);

    /* 
     * XXX Chicken-and-egg problem; we want to have console output early, but some
     * console attributes may depend on reading from eg. the boot device, which we
     * can't do yet.
     *
     * We can use printf() etc. once this is done.
     * If the previous boot stage has requested a serial console, prefer that.
     */
    bi_setboothowto(initial_howto);
    if (initial_howto & RB_MULTIPLE) {
	if (initial_howto & RB_SERIAL)
	    setenv("console", "comconsole vidconsole", 1);
	else
	    setenv("console", "vidconsole comconsole", 1);
    } else if (initial_howto & RB_SERIAL)
	setenv("console", "comconsole", 1);
    else if (initial_howto & RB_MUTE)
	setenv("console", "nullconsole", 1);
    cons_probe();

    /*
     * Initialise the block cache
     */
    bcache_init(32, 512);	/* 16k cache XXX tune this */

    /*
     * Special handling for PXE and CD booting.
     */
    if (kargs->bootinfo == 0) {
	/*
	 * We only want the PXE disk to try to init itself in the below
	 * walk through devsw if we actually booted off of PXE.
	 */
	if (kargs->bootflags & KARGS_FLAGS_PXE)
	    pxe_enable(kargs->pxeinfo ? PTOV(kargs->pxeinfo) : NULL);
	else if (kargs->bootflags & KARGS_FLAGS_CD)
	    bc_add(initial_bootdev);
    }

    archsw.arch_autoload = i386_autoload;
    archsw.arch_getdev = i386_getdev;
    archsw.arch_copyin = i386_copyin;
    archsw.arch_copyout = i386_copyout;
    archsw.arch_readin = i386_readin;
    archsw.arch_isainb = isa_inb;
    archsw.arch_isaoutb = isa_outb;
#ifdef LOADER_ZFS_SUPPORT
    archsw.arch_zfs_probe = i386_zfs_probe;
#endif

    /*
     * March through the device switch probing for things.
     */
    for (i = 0; devsw[i] != NULL; i++)
	if (devsw[i]->dv_init != NULL)
	    (devsw[i]->dv_init)();
    printf("BIOS %dkB/%dkB available memory\n", bios_basemem / 1024, bios_extmem / 1024);
    if (initial_bootinfo != NULL) {
	initial_bootinfo->bi_basemem = bios_basemem / 1024;
	initial_bootinfo->bi_extmem = bios_extmem / 1024;
    }

    /* detect ACPI for future reference */
    biosacpi_detect();

    /* detect SMBIOS for future reference */
    smbios_detect();

    printf("\n");
    printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
    printf("(%s, %s)\n", bootprog_maker, bootprog_date);

    extract_currdev();				/* set $currdev and $loaddev */
    setenv("LINES", "24", 1);			/* optional */
    
    bios_getsmap();

    if(IsShiftpressed()) interact("/boot/emergency_loader.rc");
    else interact("/boot/loader.rc");			/* doesn't return */

    /* if we ever get here, it is an error */
    return (1);
}

/*
 * Set the 'current device' by (if possible) recovering the boot device as 
 * supplied by the initial bootstrap.
 *
 * XXX should be extended for netbooting.
 */
static void
extract_currdev(void)
{
    struct i386_devdesc		new_currdev;
#ifdef LOADER_ZFS_SUPPORT
    struct zfs_boot_args	*zargs;
#endif
    int				biosdev = -1;

    /* Assume we are booting from a BIOS disk by default */
    new_currdev.d_dev = &biosdisk;

    /* new-style boot loaders such as pxeldr and cdldr */
    if (kargs->bootinfo == 0) {
        if ((kargs->bootflags & KARGS_FLAGS_CD) != 0) {
	    /* we are booting from a CD with cdboot */
	    new_currdev.d_dev = &bioscd;
	    new_currdev.d_unit = bc_bios2unit(initial_bootdev);
	} else if ((kargs->bootflags & KARGS_FLAGS_PXE) != 0) {
	    /* we are booting from pxeldr */
	    new_currdev.d_dev = &pxedisk;
	    new_currdev.d_unit = 0;
	} else {
	    /* we don't know what our boot device is */
	    new_currdev.d_kind.biosdisk.slice = -1;
	    new_currdev.d_kind.biosdisk.partition = 0;
	    biosdev = -1;
	}
#ifdef LOADER_ZFS_SUPPORT
    } else if ((kargs->bootflags & KARGS_FLAGS_ZFS) != 0) {
	zargs = NULL;
	/* check for new style extended argument */
	if ((kargs->bootflags & KARGS_FLAGS_EXTARG) != 0)
	    zargs = (struct zfs_boot_args *)(kargs + 1);

	if (zargs != NULL && zargs->size >= sizeof(*zargs)) {
	    /* sufficient data is provided */
	    new_currdev.d_kind.zfs.pool_guid = zargs->pool;
	    new_currdev.d_kind.zfs.root_guid = zargs->root;
	} else {
	    /* old style zfsboot block */
	    new_currdev.d_kind.zfs.pool_guid = kargs->zfspool;
	    new_currdev.d_kind.zfs.root_guid = 0;
	}
	new_currdev.d_dev = &zfs_dev;
#endif
    } else if ((initial_bootdev & B_MAGICMASK) != B_DEVMAGIC) {
	/* The passed-in boot device is bad */
	new_currdev.d_kind.biosdisk.slice = -1;
	new_currdev.d_kind.biosdisk.partition = 0;
	biosdev = -1;
    } else {
	new_currdev.d_kind.biosdisk.slice = B_SLICE(initial_bootdev) - 1;
	new_currdev.d_kind.biosdisk.partition = B_PARTITION(initial_bootdev);
	biosdev = initial_bootinfo->bi_bios_dev;

	/*
	 * If we are booted by an old bootstrap, we have to guess at the BIOS
	 * unit number.  We will lose if there is more than one disk type
	 * and we are not booting from the lowest-numbered disk type 
	 * (ie. SCSI when IDE also exists).
	 */
	if ((biosdev == 0) && (B_TYPE(initial_bootdev) != 2))	/* biosdev doesn't match major */
	    biosdev = 0x80 + B_UNIT(initial_bootdev);		/* assume harddisk */
    }
    new_currdev.d_type = new_currdev.d_dev->dv_type;

    /*
     * If we are booting off of a BIOS disk and we didn't succeed in determining
     * which one we booted off of, just use disk0: as a reasonable default.
     */
    if ((new_currdev.d_type == biosdisk.dv_type) &&
	((new_currdev.d_unit = bd_bios2unit(biosdev)) == -1)) {
	printf("Can't work out which disk we are booting from.\n"
	       "Guessed BIOS device 0x%x not found by probes, defaulting to disk0:\n", biosdev);
	new_currdev.d_unit = 0;
    }

    env_setenv("currdev", EV_VOLATILE, i386_fmtdev(&new_currdev),
	       i386_setcurrdev, env_nounset);
    env_setenv("loaddev", EV_VOLATILE, i386_fmtdev(&new_currdev), env_noset,
	       env_nounset);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
    int i;

    for (i = 0; devsw[i] != NULL; ++i)
	if (devsw[i]->dv_cleanup != NULL)
	    (devsw[i]->dv_cleanup)();

    printf("Rebooting...\n");
    delay(1000000);
    __exit(0);
}

/* provide this for panic, as it's not in the startup code */
void
exit(int code)
{
    __exit(code);
}

COMMAND_SET(heap, "heap", "show heap usage", command_heap);

static int
command_heap(int argc, char *argv[])
{
    mallocstats();
    printf("heap base at %p, top at %p, upper limit at %p\n", heap_bottom,
      sbrk(0), heap_top);
    return(CMD_OK);
}

/* ISA bus access functions for PnP, derived from <machine/cpufunc.h> */
int
isa_inb(int port)
{
    u_char	data;
    
    if (__builtin_constant_p(port) && 
	(((port) & 0xffff) < 0x100) && 
	((port) < 0x10000)) {
	__asm __volatile("inb %1,%0" : "=a" (data) : "id" ((u_short)(port)));
    } else {
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
    }
    return(data);
}

void
isa_outb(int port, int value)
{
    u_char	al = value;
    
    if (__builtin_constant_p(port) && 
	(((port) & 0xffff) < 0x100) && 
	((port) < 0x10000)) {
	__asm __volatile("outb %0,%1" : : "a" (al), "id" ((u_short)(port)));
    } else {
        __asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
    }
}

#ifdef LOADER_ZFS_SUPPORT
static void
i386_zfs_probe(void)
{
    char devname[32];
    int unit, slice;

    /*
     * Open all the disks we can find and see if we can reconstruct
     * ZFS pools from them. Bogusly assumes that the disks are named
     * diskN, diskNpM or diskNsM.
     */
    for (unit = 0; unit < MAXBDDEV; unit++) {
	sprintf(devname, "disk%d:", unit);
	if (zfs_probe_dev(devname, NULL) == ENXIO)
	    continue;
	for (slice = 1; slice <= 128; slice++) {
	    sprintf(devname, "disk%dp%d:", unit, slice);
	    zfs_probe_dev(devname, NULL);
	}
	for (slice = 1; slice <= 4; slice++) {
	    sprintf(devname, "disk%ds%d:", unit, slice);
	    zfs_probe_dev(devname, NULL);
	}
    }
}
#endif
