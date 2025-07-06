|               Latest Release              |          Linux / EPICS Build / MacOS         |                  Cygwin@Windows                  |
| :---------------------------------------: | :------------------------------------------: | :----------------------------------------------: |
| [![Version][badge.version]][link.version] | [![Travis Build][badge.travis]][link.travis] | [![Cygwin Build][badge.appveyor]][link.appveyor] |

# procServ

A wrapper to start arbitrary interactive commands in the background,
with telnet access to stdin/stdout.

On systems that use systemd, the procServUtils set of helper/convenience
scripts can be used to manage procServ instances using per-instance
systemd unit files.

## Dependencies

-   Posix compliant OS with a C++ compiler
    <br/>
    Known to work on Linux, Solaris, MacOS, Cygwin.

-   [**pandoc**](https://pandoc.org/MANUAL.html)
    (package: pandoc), to create documentation in different formats
    (man, pdf, html)
    <br/>
    Note: The distribution tar contains the doc in all available formats,
    so you don't need pandoc to make and install procServ.

-   [**libtelnet**](https://github.com/seanmiddleditch/libtelnet)
    (package: libtelnet)
    <br/>
    Note: The procServ distribution tar contains the libtelnet sources.
    It will be compiled into procServ automatically, if the library
    is not found on the system.

-   Suggested: **telnet** and/or **socat** as clients to attach to
    procServ instances.
    The former is used to connect using TCP ports, the latter when using
    domain sockets.

-   For the procServUtils scripts on systems with systemd:
    *   **Python** (2.7 and up) with distutils
    *   **telnet** and/or **socat** for the attach command (see above)

## Building procServ

### Using autotools

1.  Unpack the procServ distribution tar.

2.  Perform a regular autotools build:
    ```
    $ ./configure
    $ make
    ```
    Configure `--with-systemd-utils` to include the procServUtils
    scripts in the build.

### Using the EPICS Build System

1.  Unpack the procServ distribution tar into an appropriate place
    within your EPICS application structure.

2.  Inside that directory, run `./configure --with-epics-top=TOP`
    where TOP is the relative path to the EPICS TOP directory.
    <br/>
    (For a structure created with epicsMakeBaseExt.pl, the appropriate
    place for the procServ subdir would be under `TOP/src`,
    with `../..` being the relative path to specify to configure -
    which is the default.)

3.  Build your EPICS structure.

### From the procServ Source Repository

Requires autoconf >=2.61, automake >= 1.10
<br/>
Optional pandoc >= 2.17.1.1
```
   $ git clone https://github.com/ralphlange/procServ.git
   $ cd procserv
   $ make
   $ ./configure --enable-doc
   $ make
```
Configure `--with-systemd-utils` to include the procServUtils
scripts in the build.

Note: When building from the repository, you must explicitly
use `--enable-doc` or `--disable-doc`.  Omitting this
option assumes the distribution behaviour:
the documentation should be installed, but doesn't
need to be generated.

### Building on Cygwin/Windows

In general,
```sh
   sh configure
   make
```
should be enough. If you have `autoconf` and `automake` packages,
then for a really clean build type
```sh
   sh autoreconf -fi
   sh configure
   make clean
   make
```

If you plan to connect to procServ from a non-localhost address,
you will need to use
```sh
   sh configure --enable-access-from-anywhere
```
as the configure step.

The executable is also available for download on GitHub/SourceForge.

## Repository, CI, Distribution and Packaging Ecosystem

### Sources

The procServ upstream repository is on 
[GitHub](https://github.com/ralphlange/procServ).

### Continuous Integration

Automated builds are provided by
[Travis](https://travis-ci.org/ralphlange/procServ) (for Linux and MacOS) and
[AppVeyor](https://ci.appveyor.com/project/ralphlange/procserv) (Cygwin).

### Source Distribution Tars

These specifically created tars are different from a check-out
of the upstream sources. They are available through
[GitHub releases](https://github.com/ralphlange/procServ/releases) or on
[SourceForge](http://sourceforge.net/projects/procserv/).

### Linux System Packages

procServ is part of official Linux distributions:

-   Debian/Ubuntu: `apt-get install procserv`
-   Fedora/RHEL:   `yum install procServ`

The [source repository](https://github.com/ralphlange/procServ) also contains 
the packaging extras. These are usually from the last release and not part of
the distribution tar.

## Using procServ

### Running Applications (e.g. EPICS IOCs) as Services on Unix/Linux

Michael Davidsaver has contributed procServUtils, a set of utility scripts
for managing procServ-run system service instances under systemd.
These scripts generate the systemd unit files as well as configuration
snippets for the [conserver](https://www.conserver.com/) tool.

`manage-procs` is the script to add and remove procServ instances to
the systemd configuration, create conserver configuration snippets,
start and stop configured procServ instances,
generate lists of the instances known on the current host
and report their status.

For more details, check the manpage and use the script's `-h` option.

For older systems using SysV-style rc scripts, you can look at the
[Debian packaging](http://epics.nsls2.bnl.gov/debian/) or
at the [upstream repository](https://github.com/epicsdeb/sysv-rc-softioc)
of the predecessor package of these utilities.

### Using procServ on Cygwin/Windows

In the `.bat` file to launch procServ you should add
```bat
   set CYGWIN=nodosfilewarning
```
to suppress warnings about using windows style paths.

If you plan to control procServ from a non-localhost address,
you will need to run it with `--allow` to allow remote access
to the child console.

The default build on Cygwin uses static linking.
I.e. to run on a non-Cygwin Windows system, procServ only needs `Cygwin1.dll`,
e.g. in the same directory as the executable.

Using Windows style paths ('`\`' delimiter) in arguments to procServ
is usually OK and suggested under `command.com`.
If you have problems try replacing them with Cygwin syntax,
i.e. "`/cygdrive/c/my/path`" rather than "`C:\my\path`".

Under `command.com`, the caret sign '`^`' has to be escaped using '`^^`'.

If you wish to run a `.bat` file rather than an executable as child under
procServ, you should use something along the lines of
```bat
   %ComSpec% /c runIOC.bat st.cmd
```
as arguments to procServ to launch your `.bat` file.

## License

All copyrights reserved.
Free use of this software is granted under the terms of the GNU General
Public License (GPLv3 or later).

## Enjoy!

<!-- Links -->
[badge.version]: https://badge.fury.io/gh/ralphlange%2FprocServ.svg
[link.version]: http://semver.org

[badge.travis]: https://travis-ci.org/ralphlange/procServ.svg?branch=master
[link.travis]: https://travis-ci.org/ralphlange/procServ

[badge.appveyor]: https://ci.appveyor.com/api/projects/status/h59hhep87tqn204u?svg=true
[link.appveyor]: https://ci.appveyor.com/project/ralphlange/procserv
