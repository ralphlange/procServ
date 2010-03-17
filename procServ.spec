Summary: A process server with telnet console and log access
Name: procServ
Version: 2.5.0
Release: 4%{?dist}
License: GPLv3
Group: Applications/System
URL: http://sourceforge.net/projects/procserv
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Source0: http://sourceforge.net/projects/procserv/files/%{version}/procServ-%{version}.tar.gz

%description
procServ is a wrapper that starts an arbitrary command as a child process in
the background, connecting its standard input and output to a TCP port for
telnet access. It supports logging, child restart (manual or automatic on
exit), and more.

procServ does not have the rich feature set of the screen utility,
but is intended to provide running a command in a system service style,
in a small, robust way.
Handling multiple users, authorization, authentication, central logging
is done best on a higher level, using a package like conserver.

For security reasons, procServ only accepts connections from localhost.

%prep
%setup -q

%build
%configure --docdir=%{_defaultdocdir}/%{name}-%{version}
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT INSTALL="install -p"

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING ChangeLog NEWS README procServ.html procServ.pdf procServ.txt
%{_bindir}/procServ
%{_mandir}/man1/procServ.*

%changelog
* Fri Jan 15 2010 Ralph Lange <Ralph.Lange@bessy.de> 2.5.0-4
- Improved description

* Sat Dec 26 2009 Ralph Lange <Ralph.Lange@bessy.de> 2.5.0-3
- Spec clean-up suggested by Fabian Affolter:
  Removed autotools requirement, removed attr for binary,
  added --docdir to configure, added flags for parallel make

* Thu Dec 24 2009 Ralph Lange <Ralph.Lange@bessy.de> 2.5.0-2
- Fixed rpmlint issue by breaking description into multiple lines

* Thu Dec 03 2009 Matthieu Bec <mbec@gemini.edu> 2.5.0-1
- first spec

