/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)pathnames.h	5.3 (Berkeley) 5/9/89
 */

#define _PATH_BSHELL    "/bin/sh"
#define _PATH_CSHELL    "/bin/csh"
#define UT_NAMESIZE     8
#define _PATH_TTY       "/dev/tty"
#define TTYTYPES        "/etc/ttytype"
#define SECURETTY       "/etc/securetty"
#define _PATH_UTMP      "/etc/utmp"
#define _PATH_WTMP      "/etc/wtmp"

#define	_PATH_DEFPATH	        "/usr/local/bin:/bin:/usr/bin:."
#define	_PATH_DEFPATH_ROOT	"/bin:/usr/bin:/etc"
#define	_PATH_HUSHLOGIN	".hushlogin"
#define	_PATH_LASTLOG	"/usr/adm/lastlog"
#define	_PATH_MAILDIR	"/var/spool/mail"
#define	_PATH_MOTDFILE	"/etc/motd"
#define	_PATH_NOLOGIN	"/etc/nologin"
