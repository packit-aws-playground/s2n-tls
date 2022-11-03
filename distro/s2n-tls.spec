Name:           s2n-tls
Version:        1.3.2
Release:        3%{?dist}
Summary:        s2n: an implementation of the TLS/SSL protocols utilities

License:        ASL 2.0
URL:            https://github.com/davdunc/%{name}
Source0:        %{url}/archive/v%{version}/%{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: cmake
BuildRequires: openssl-devel
BuildRequires: ninja-build
Requires:      openssl

%description
s2n-tls is a C99 implementation of the TLS/SSL protocols that is
designed to be simple, small, fast, and with security as a priority.

%package libs
Summary: s2n: an implementation of the TLS/SSL protocols libraries
Requires:      %{name}%{?_isa} = %{version}-%{release}

%description libs 
s2n-tls is a C99 implementation of the TLS/SSL protocols that is
designed to be simple, small, fast, and with security as a priority.

%package devel
Summary: s2n: an implementation of the TLS/SSL protocols headers and source files.
Requires:      %{name}-libs%{?_isa} = %{version}-%{release}
Requires:      openssl-devel

%description devel
Headers and Source files for s2n-tls is a C99 implementation of the
TLS/SSL protocols that is designed to be simple, small, fast, and with
security as a priority.

%prep
%autosetup


%build
%cmake -DUNSAFE_TREAT_WARNINGS_AS_ERRORS=OFF -DBUILD_SHARED_LIBS=ON
%cmake_build

%install
%cmake_install


%files devel
%{_includedir}/s2n.h
%{_libdir}/s2n/cmake/modules/FindLibCrypto.cmake
%{_libdir}/s2n/cmake/s2n-config.cmake
%{_libdir}/s2n/cmake/shared/s2n-targets-noconfig.cmake
%{_libdir}/s2n/cmake/shared/s2n-targets.cmake


%files libs
%license LICENSE
%{_libdir}/libs2n.so

%files
%license LICENSE
%doc VERSIONING.rst NOTICE README.md
%doc docs/READING-LIST.md docs/USAGE-GUIDE.md



%changelog
* Tue Feb 22 2022 David Duncan <davdunc@amazon.com> - 1.3.2-3
- Updated for package review

* Wed Feb 02 2022 David Duncan <davdunc@amazon.com> - 1.3.2-2
- Prepare for package review

* Sat Dec 18 2021 David Duncan <davdunc@amazon.com> - 1.3.2-1
- Initial Package Review release
