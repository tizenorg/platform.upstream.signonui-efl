Name: signon-ui
Summary: EFL based Single Sign-On UI
URL: https://code.google.com/p/accounts-sso/source/checkout?repo=signonui-efl
Version: 0.0.1
Release: 1
Group: Security/Secure Storage
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
Requires: dbus-1
BuildRequires: pkgconfig(dbus-1)
BuildRequires: pkgconfig(glib-2.0) >= 2.30
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(gsignond)
Provides: signon-ui


%description
EFL based Single Sign-On UI used by gsignond.


%prep
%setup -q -n %{name}-%{version}
autoreconf -f -i


%build
%configure
make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install


%files
%defattr(-,root,root,-)
%{_libexecdir}/%{name}
%{_datadir}/dbus-1/services/*.service

