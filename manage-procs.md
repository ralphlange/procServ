---
date: UNRELEASED
title: MANAGE-PROCS(1)
---

# NAME

manage-procs - manage procServ instances as systemd new-style daemons

# SYNOPSIS

**manage-procs** \[-h\|--help\] \[--user\] \[--system\] \[-v\] *command*
\[*args*\]

# DESCRIPTION

manage-procs(1) is a helper script for creating/maintaining procServ(1)
instances managed as systemd(1) new-style daemons.

Both user and system mode of systemd are supported. Specifying the
**--user** options will consider the user unit configuration, while
the **--system** option will consider the system unit configuration.

Configuration files defining procServ instances will reside in

    /etc/procServ.conf
    /etc/procServ.d/*.conf

for global systemd units or

    ~/.config/procServ.conf
    ~/.config/procServ.d/*.conf

for user systemd units. These configuration files contain blocks like

    [instancename]
    command = /bin/bash
    ## optional
    #chdir = /
    #user = nobody
    #group = nogroup
    #port=0  # default to dynamic assignment

The procServUtils package installs systemd generators that will generate
unit files from these configuration blocks.

# GENERAL OPTIONS

**-h, --help**
Show a help message and exit.

**--user**
Consider user configuration.

**--system**
Consider system configuration. (default)

**-v, --verbose**
Increase verbosity level. (may be specified multiple times)

# COMMANDS

**manage-procs add** \[-h\] \[-f\] \[-A\] \[-C *dir*\] \[-P *port*\] \[-U *user*\] \[-G *group*\] *name* *command*…​  
Create a new procServ instance.

**-h, --help**
Show a help message and exit.

**-f, --force**
Overwrite an existing instance of the same name.

**-A, --autostart**
Start instance after creating it.

**-C, --chdir** *dir*
Set *dir* as run directory for instance. (default: current directory)

**-P, --port** *port*
Control endpoint specification (e.g. telnet port) for instance.
(default: `unix:RUNDIR/procserv-NAME/control` where *RUNDIR* is
defined by the system, e.g. "/run" or "/run/user/UID")

**-U, --user** *username*
User name for instance to run as.

**-G, --group** *groupname*
Group name for instance to run as.

**name**  
Instance name.

**command…​**  
The remaining line is interpreted as the command (with arguments) to run
inside the procServ instance.

**manage-procs remove** \[-h\] \[-f\] *name*  
Remove an existing procServ instance from the configuration.

**-h, --help**
Show a help message and exit.

**-f, --force**
Remove without asking for confirmation.

**name**  
Instance name.

**manage-procs start** \[-h\] \[*pattern*\]  
Start procServ instances.

**-h, --help**
Show a help message and exit.

**pattern**  
Pattern to match existing instance names against. (default: "\*" = start
all procServ instances)

**manage-procs stop** \[-h\] \[*pattern*\]  
Stop procServ instances.

**-h, --help**
Show a help message and exit.

**pattern**  
Pattern to match existing instance names against. (default: "\*" = stop
all procServ instances)

**manage-procs attach** \[-h\] *name*  
Attach to the control port of a running procServ instance.

For this, manage-procs is using one of two existing CLI client
applications to connect: *telnet* to connect to TCP ports and *socat* to
connect to UNIX domain sockets.

For both connection types, press `^D` to detach from the session.

**-h, --help**
Show a help message and exit.

**name**  
Instance name.

**manage-procs list** \[-h\] \[--all\]  
List all procServ instances.

**-h, --help**
Show a help message and exit.

**--all**
Also list inactive instances.

**manage-procs status** \[-h\]  
Report the status of all procServ instances.

**-h, --help**
Show a help message and exit.

# SEE ALSO

**procServ**(1)

# KNOWN PROBLEMS

None so far.

# REPORTING BUGS

Please report bugs using the issue tracker at
<https://github.com/ralphlange/procServ/issues>.

# AUTHORS

Written by Michael Davidsaver \<<mdavidsaver@ospreydcs.com>\>.
Contributing author: Ralph Lange \<<ralph.lange@gmx.de>\>.

# RESOURCES

GitHub project: <https://github.com/ralphlange/procServ>

# COPYING

All rights reserved. Free use of this software is granted under the
terms of the GNU General Public License (GPLv3).
