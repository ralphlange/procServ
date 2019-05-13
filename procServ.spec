%{!?_pkgdocdir: %global _pkgdocdir %{_docdir}/%{name}-%{version}}

Summary: Process server with telnet console and log access
Name: procServ
Version: 2.7.0
Release: 8%{?dist}

License: GPLv3
URL: https://github.com/ralphlange/procServ
Source0: https://github.com/ralphlange/procServ/releases/download/V%{version}/procServ-%{version}.tar.gz
BuildRequires: libtelnet-devel gcc-c++

%description
procServ is a wrapper that starts an arbitrary command as a child process in
the background, connecting its standard input and output to a Unix domain
socket or a TCP port for telnet access.
It supports logging, child restart (manual or automatic on exit), and more.

procServ does not have the rich feature set of the screen utility,
but is intended to provide running a command in a system service style,
in a small, robust way.
Handling multiple users, authorization, authentication, central logging
is done best on a higher level, using a package like conserver.

For security reasons, procServ only accepts connections from localhost.

%prep
%setup -q

%build
%configure --docdir=%{_pkgdocdir}
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT INSTALL="install -p"

%files
%{_pkgdocdir}/
%{_bindir}/procServ
%{_mandir}/man1/procServ.1*

%changelog
* Sat Feb 02 2019 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-8
- Rebuilt for https://fedoraproject.org/wiki/Fedora_30_Mass_Rebuild

* Fri Jul 13 2018 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_29_Mass_Rebuild

* Tue Feb 20 2018 Ralph Lange <ralph.lange@gmx.de> - 2.7.0-6
- Add BR against gcc-c++

* Fri Feb 09 2018 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Thu Aug 03 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Thu Jul 27 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Sat Feb 11 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.7.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_26_Mass_Rebuild

* Fri Jan 20 2017 Ralph Lange <ralph.lange@gmx.de> - 2.7.0-1
- New upstream version

* Wed Sep 28 2016 Ralph Lange <ralph.lange@gmx.de> - 2.6.1-1
- New upstream version
- New project homepage and download page

* Thu Feb 04 2016 Fedora Release Engineering <releng@fedoraproject.org> - 2.6.0-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Thu Jun 18 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-9
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Sat May 02 2015 Kalev Lember <kalevlember@gmail.com> - 2.6.0-8
- Rebuilt for GCC 5 C++11 ABI change

* Sun Aug 17 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Tue Aug 20 2013 Ville Skyttä <ville.skytta@iki.fi> - 2.6.0-5
- Install docs to %%{_pkgdocdir} where available (#994051).

* Sun Aug 04 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Sat Jul 21 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.6.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Mon Apr 16 2012 Ralph Lange <Ralph.Lange@gmx.de> 2.6.0-1
- New upstream version
- Added libtelnet dependency

* Sat Jan 14 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.5.1-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.5.1-5
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Wed Sep 29 2010 jkeating - 2.5.1-4
- Rebuilt for gcc bug 634757

* Thu Sep 02 2010 Ralph Lange <Ralph.Lange@bessy.de> 2.5.1-3
- More spec clean-up suggested by Martin Gieseking:
  Adapted Source0 URL to follow guidelines for Sourceforge.net
  Fixed macro usage in changelog (avoid rpmlint warning)
  Added specific man section to wildcard in %%files

* Fri Jul 23 2010 Ralph Lange <Ralph.Lange@bessy.de> 2.5.1-2
- Spec clean-up suggested by Michael Schwendt:
  Skipped "A" from summary, replaced %%doc with the --docdir
  directory to avoid conflict killing --docdir

* Tue Mar 23 2010 Ralph Lange <Ralph.Lange@bessy.de> 2.5.1-1
- New upstream version

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
