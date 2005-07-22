/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <asm/semaphore.h>
#include <linux/completion.h>
#include <linux/buffer_head.h>

#include "gfs2.h"
#include "mount.h"
#include "proc.h"

/**
 * gfs2_mount_args - Parse mount options
 * @sdp:
 * @data:
 *
 * Return: errno
 */

int gfs2_mount_args(struct gfs2_sbd *sdp, char *data_arg, int remount)
{
       	struct gfs2_args *args = &sdp->sd_args;
	char *data = data_arg;
	char *options, *o, *v;
	int error = 0;

	if (!remount) {
		/*  If someone preloaded options, use those instead  */
		spin_lock(&gfs2_proc_margs_lock);
		if (gfs2_proc_margs) {
			data = gfs2_proc_margs;
			gfs2_proc_margs = NULL;
		}
		spin_unlock(&gfs2_proc_margs_lock);

		/*  Set some defaults  */
		args->ar_num_glockd = GFS2_GLOCKD_DEFAULT;
		args->ar_quota = GFS2_QUOTA_DEFAULT;
		args->ar_data = GFS2_DATA_DEFAULT;
	}

	/* Split the options into tokens with the "," character and
	   process them */

	for (options = data; (o = strsep(&options, ",")); ) {
		if (!*o)
			continue;

		v = strchr(o, '=');
		if (v)
			*v++ = 0;

		if (!strcmp(o, "lockproto")) {
			if (!v)
				goto need_value;
			if (remount && strcmp(v, args->ar_lockproto))
				goto cant_remount;
			strncpy(args->ar_lockproto, v, GFS2_LOCKNAME_LEN);
			args->ar_lockproto[GFS2_LOCKNAME_LEN - 1] = 0;
		}

		else if (!strcmp(o, "locktable")) {
			if (!v)
				goto need_value;
			if (remount && strcmp(v, args->ar_locktable))
				goto cant_remount;
			strncpy(args->ar_locktable, v, GFS2_LOCKNAME_LEN);
			args->ar_locktable[GFS2_LOCKNAME_LEN - 1] = 0;
		}

		else if (!strcmp(o, "hostdata")) {
			if (!v)
				goto need_value;
			if (remount && strcmp(v, args->ar_hostdata))
				goto cant_remount;
			strncpy(args->ar_hostdata, v, GFS2_LOCKNAME_LEN);
			args->ar_hostdata[GFS2_LOCKNAME_LEN - 1] = 0;
		}

		else if (!strcmp(o, "spectator")) {
			if (remount && !args->ar_spectator)
				goto cant_remount;
			args->ar_spectator = TRUE;
			sdp->sd_vfs->s_flags |= MS_RDONLY;

		} else if (!strcmp(o, "ignore_local_fs")) {
			if (remount && !args->ar_ignore_local_fs)
				goto cant_remount;
			args->ar_ignore_local_fs = TRUE;

		} else if (!strcmp(o, "localflocks")) {
			if (remount && !args->ar_localflocks)
				goto cant_remount;
			args->ar_localflocks = TRUE;

		} else if (!strcmp(o, "localcaching")) {
			if (remount && !args->ar_localcaching)
				goto cant_remount;
			args->ar_localcaching = TRUE;

		} else if (!strcmp(o, "oopses_ok"))
			args->ar_oopses_ok = TRUE;

		else if (!strcmp(o, "nooopses_ok"))
			args->ar_oopses_ok = FALSE;

		else if (!strcmp(o, "debug")) {
			args->ar_debug = TRUE;

		} else if (!strcmp(o, "nodebug"))
			args->ar_debug = FALSE;

		else if (!strcmp(o, "upgrade")) {
			if (remount && !args->ar_upgrade)
				goto cant_remount;
			args->ar_upgrade = TRUE;

		} else if (!strcmp(o, "num_glockd")) {
			unsigned int x;
			if (!v)
				goto need_value;
			sscanf(v, "%u", &x);
			if (remount && x != args->ar_num_glockd)
				goto cant_remount;
			if (!x || x > GFS2_GLOCKD_MAX) {
				printk("GFS2: 0 < num_glockd <= %u  (not %u)\n",
				       GFS2_GLOCKD_MAX, x);
				error = -EINVAL;
				break;
			}
			args->ar_num_glockd = x;
		}

		else if (!strcmp(o, "acl")) {
			args->ar_posix_acl = TRUE;
			sdp->sd_vfs->s_flags |= MS_POSIXACL;

		} else if (!strcmp(o, "noacl")) {
			args->ar_posix_acl = FALSE;
			sdp->sd_vfs->s_flags &= ~MS_POSIXACL;

		} else if (!strcmp(o, "quota")) {
			if (!v)
				goto need_value;
			if (!strcmp(v, "off"))
				args->ar_quota = GFS2_QUOTA_OFF;
			else if (!strcmp(v, "account"))
				args->ar_quota = GFS2_QUOTA_ACCOUNT;
			else if (!strcmp(v, "on"))
				args->ar_quota = GFS2_QUOTA_ON;
			else {
				printk("GFS2: invalid value for quota\n");
				error = -EINVAL;
				break;
			}

		} else if (!strcmp(o, "suiddir"))
			args->ar_suiddir = TRUE;

		else if (!strcmp(o, "nosuiddir"))
			args->ar_suiddir = FALSE;

		else if (!strcmp(o, "data")) {
			if (!v)
				goto need_value;
			if (!strcmp(v, "writeback"))
				args->ar_data = GFS2_DATA_WRITEBACK;
			else if (!strcmp(v, "ordered"))
				args->ar_data = GFS2_DATA_ORDERED;
			else {
				printk("GFS2: invalid value for data\n");
				error = -EINVAL;
				break;
			}

		} else {
			printk("GFS2: unknown option: %s\n", o);
			error = -EINVAL;
			break;
		}
	}

	if (error)
		printk("GFS2: invalid mount option(s)\n");

	if (data != data_arg)
		kfree(data);

	return error;

 need_value:
	printk("GFS2: need value for option %s\n", o);
	return -EINVAL;

 cant_remount:
	printk("GFS2: can't remount with option %s\n", o);
	return -EINVAL;
}

