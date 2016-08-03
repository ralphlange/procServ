# procServ [![Build Status](https://travis-ci.org/ralphlange/procServ.svg?branch=master)](https://travis-ci.org/ralphlange/procServ)

A wrapper to start arbitrary interactive commands in the background,
with telnet access to stdin/stdout.

## Build and Release Ecosystem

### Sources

The procServ upstream repository is on 
[GitHub](https://github.com/ralphlange/procServ).

Distribution tars are also available through 
[SourceForge](http://sourceforge.net/projects/procserv/).

### Dependencies

- [**asciidoc**](http://www.methods.co.nz/asciidoc/)
  (package: asciidoc), to create documentation in different formats 
  (man, pdf, html)
  <br>
  NOTE: The distribution tar contains the doc in all available formats,
  so you don't need asciidoc to make and install procServ.

- [**libtelnet**](https://github.com/seanmiddleditch/libtelnet)
  (package: libtelnet)
  <br>
  NOTE: The distribution tar contains the libtelnet sources, so it
  will be compiled into procServ automatically, if not found.

### Continuous Integration

Automated builds are provided by
[CloudBees](https://openepics.ci.cloudbees.com/view/procServ/) and 
[Travis](https://travis-ci.org/ralphlange/procServ).

### Linux Packages

procServ is part of official Linux distributions:

- Debian/Ubuntu: `apt-get install procserv`
- Fedora/RHEL:   `yum install procServ`

The [source repository](https://github.com/ralphlange/procServ) also contains 
the packaging extras. These are not part of the distribution tar.

## Building procServ

### Building with the EPICS Build System

1. Unpack procServ at the appropriate place within your EPICS structure.
2. Inside that directory, run `./configure --with-epics-top=TOP`
   where TOP is the relative path to the EPICS TOP directory.
   <br>
   (For a structure created with epicsMakeBaseExt.pl, the appropriate 
   place for the procServ subdir would be under `TOP/src`, 
   with `../..` being the relative path to specify to configure - 
   which is the default.)
3. Build your EPICS structure.

### Building from the procServ Source Repository

Requires autoconf >=2.61, automake >= 1.10
<br>
Optional asciidoc >= 8.4, FOP >= 0.95, xsltproc >= 1.1.24

    $ git clone https://github.com/ralphlange/procServ.git
    $ cd procserv
    $ make
    $ ./configure --enable-doc
    $ make

Note: When building from the repository you must explicitly
use `--enable-doc` or `--disable-doc`.  Omitting this
option assumes the distribution behaviour, that
the documentation should be installed, but doesn't
need to be built.

### Building on Cygwin/Windows

In general,

    sh configure
    make

should be enough. If you have `autoconf` and `automake` packages,
then for a really clean build type

    autoreconf -fi
    sh configure
    make distclean
    make

If you plan to control procServ from a non-localhost address,
you will need to use

	sh configure --enable-access-from-anywhere

as the configure step.

The executable is also available for download on SourceForge.

## Using procServ

### Running EPICS IOCs as Services on Unix

Michael Davidsaver has created SysV-style rc scripts to configure
and run EPICS IOCs using procServ.

You can look at the [Debian package](http://epics.nsls2.bnl.gov/debian/) or 
at the [upstream repository](https://github.com/epicsdeb/sysv-rc-softioc).

### Using procServ on Cygwin/Windows

In the `.bat` file to launch procServ you should add

    set CYGWIN=nodosfilewarning
to suppress warnings about using windows style paths.

If you plan to control procServ from a non-localhost address,
you will need to run it with `--allow` to allow remote access
to the child console.

To run on a non-Cygwin Windows system, procServ only needs `Cygwin1.dll`,
e.g. in the same directory as the executable.

Using Windows style paths ('`\`' delimiter) in arguments to procServ
is usually OK and suggested under `command.com`.
If you have problems try replacing them with Cygwin syntax,
i.e. "`/cygdrive/c/my/path`" rather than "`C:\my\path`".

Under `command.com`, the caret sign '`^`' has to be escaped using '`^^`'.

If you wish to run a `.bat` file rather than an executable as child under
procServ, you should use something along the lines of

    %ComSpec% /c runIOC.bat st.cmd

as arguments to procServ to launch your `.bat` file.

## Enjoy!
