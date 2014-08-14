%define _use_browser 1

Name: signonui-efl
Summary: EFL based Single Sign-On UI
URL: https://code.google.com/p/accounts-sso/source/checkout?repo=signonui-efl
Version: 0.0.3
Release: 2
Group: Security/Secure Storage
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
Requires: dbus-1
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(dbus-1)
BuildRequires: pkgconfig(glib-2.0) >= 2.30
BuildRequires: pkgconfig(gio-unix-2.0)
%if %{_use_browser} == 1
BuildRequires: pkgconfig(libsoup-2.4)
%else
BuildRequires: pkgconfig(ewebkit2)
%endif
BuildRequires: pkgconfig(gsignond)
Provides: signon-ui


%description
EFL based Single Sign-On UI used by gsignond.


%prep
%setup -q -n %{name}-%{version}
%if %{_use_browser} == 1
autoreconf -f -i 
%else
autoreconf -f -i --with-ewebkit2
%endif


%build
CFLAGS="$CFLAGS -DTIZEN" %configure \
%if %{_use_browser} == 1
 --enable-browser-cmd=/usr/bin/xwalk
%else
 --with-ewebkit2
%endif
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install


%files
%defattr(-,root,root,-)
%{_libexecdir}/%{name}
%{_datadir}/dbus-1/services/*.service

