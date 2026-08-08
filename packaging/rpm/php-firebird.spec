# =============================================================================
# RPM spec file for php-firebird
# =============================================================================
#
# Builds php-firebird for RPM-based distributions:
#   AlmaLinux 8/9, Rocky 8/9, RHEL 8/9, Fedora 41
#
# Build runs inside manylinux_2_28 container (AlmaLinux 8 base, glibc 2.28).
# The FB5 client is bundled (not system firebird-devel) so that
# #if FB_API_VER >= 40 code paths are always enabled.
#
# The spec builds ONE PHP version per invocation. The build.sh script
# invokes rpmbuild once per PHP version, passing PHP_VERSION as a macro.
#
# Usage:
#   rpmbuild -bb packaging/rpm/php-firebird.spec \
#       --define "php_version 8.4" \
#       --define "fb_root /opt/firebird"
#
# =============================================================================

%global php_ver %{?php_version}%{!?php_version:8.4}
%global php_short %(echo %{php_ver} | tr -d '.')
%global ext_dir %(php-config --extension-dir 2>/dev/null || echo /usr/lib64/php/modules)
%global fb_root %{?fb_root}%{!?fb_root:/opt/firebird}

# jane: FB5 client .so files are pre-built binaries from the official
#       tarball, not compiled in this RPM build. They lack build-ids
#       which Fedora's debuginfo processor rejects. Disable it.
%global debug_package %{nil}
%define _build_id_links none

Name:           php-firebird
Version:        13.1.0
Release:        1%{?dist}
Summary:        Modern Firebird database extension for PHP

License:        PHP-3.01
URL:            https://github.com/satwareAG/php-firebird
# jane: build.sh creates tarball as php-firebird-VERSION.tar.gz (not vVERSION.tar.gz)
Source0:        php-firebird-%{version}.tar.gz

BuildRequires:  php-devel
BuildRequires:  %{fb_root}/lib/libfbclient.so
BuildRequires:  autoconf
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  patchelf
BuildRequires:  gcc
BuildRequires:  gcc-c++

# php-firebird replaces the legacy ext/interbase.
# jane: Fedora 2026 packaging guidelines say Obsoletes alone is sufficient
#       for replacement. Conflicts only if coexistence causes real problems.
#       Obsoletes handles upgrade; Provides keeps dependency resolution working.
Provides:       php-interbase = %{version}-%{release}
Obsoletes:      php-interbase < %{version}

Requires:       php-common >= %{php_ver}

%description
php-firebird is a modernized PHP extension providing native connectivity
to Firebird databases (3.0, 4.0, 5.0+). It is a high-performance
replacement for the legacy ext/interbase, written in C/C++17 using the
modern Firebird OO API.

This package provides:
  * Procedural API (fbird_* functions)
  * OOP API (Firebird\Connection, Firebird\Database, etc.)
  * PDO driver (pdo_fbird)
  * Bundled Firebird 5.0 client library (FB4+ features always enabled:
    DECFLOAT, INT128, TIME/TIMESTAMP WITH TIME ZONE, batch DML)

%prep
%setup -q -n %{name}-%{version}

%build
# Build main extension (firebird.so)
phpize
./configure \
    --with-php-config=$(command -v php-config) \
    --with-firebird=%{fb_root}
make -j$(nproc)

# Build PDO driver (pdo_fbird.so) as separate extension
cd pdo_fbird
phpize
./configure \
    --with-php-config=$(command -v php-config) \
    --with-firebird=%{fb_root}
make -j$(nproc)
cd ..

%install
# Extension directory
mkdir -p %{buildroot}%{ext_dir}
install -m 0755 modules/firebird.so %{buildroot}%{ext_dir}/
install -m 0755 pdo_fbird/modules/pdo_fbird.so %{buildroot}%{ext_dir}/

# Bundled FB5 client libraries
mkdir -p %{buildroot}%{_libdir}/php-firebird
cp -a %{fb_root}/lib/libfbclient.so* %{buildroot}%{_libdir}/php-firebird/
cp -a %{fb_root}/lib/libicu*.so* %{buildroot}%{_libdir}/php-firebird/ 2>/dev/null || true
cp -a %{fb_root}/lib/libtomcrypt.so* %{buildroot}%{_libdir}/php-firebird/ 2>/dev/null || true
cp -a %{fb_root}/lib/libre2.so* %{buildroot}%{_libdir}/php-firebird/ 2>/dev/null || true

# libtommath is NOT in the FB5 tarball but libfbclient depends on it at runtime.
# Copy from system (manylinux container has it in /usr/lib64 or /usr/lib).
# jane: same fallback as packaging/debian/build.sh lines 150-169.
if [ ! -f %{buildroot}%{_libdir}/php-firebird/libtommath.so* ] 2>/dev/null; then
    for dir in /usr/lib64 /usr/lib; do
        if ls $dir/libtommath.so* >/dev/null 2>&1; then
            cp -aL $dir/libtommath.so* %{buildroot}%{_libdir}/php-firebird/
            break
        fi
    done
fi

# OOP API PHP classes + autoloader
mkdir -p %{buildroot}%{_datadir}/php/Firebird
install -m 0644 src/Firebird/*.php %{buildroot}%{_datadir}/php/Firebird/
install -m 0644 packaging/firebird-autoload.php %{buildroot}%{_datadir}/php/Firebird/

# INI files
mkdir -p %{buildroot}%{_sysconfdir}/php.d
echo "extension=firebird.so" > %{buildroot}%{_sysconfdir}/php.d/20-firebird.ini
echo "extension=pdo_fbird.so" > %{buildroot}%{_sysconfdir}/php.d/30-pdo_fbird.ini

# Set RPATH on extension .so files to find bundled client libs
# jane: Use pure-relative $ORIGIN path (like Debian rules does).
#   ext_dir is %{_libdir}/php/modules, libs are in %{_libdir}/php-firebird.
#   $ORIGIN/../../php-firebird resolves correctly from modules/ to ../php-firebird/.
#   Do NOT mix $ORIGIN with an absolute %{_libdir} prefix - produces garbage path.
patchelf --force-rpath --set-rpath '$ORIGIN/../../php-firebird' \
    %{buildroot}%{ext_dir}/firebird.so
patchelf --force-rpath --set-rpath '$ORIGIN/../../php-firebird' \
    %{buildroot}%{ext_dir}/pdo_fbird.so

# Set RPATH on bundled client libs (find siblings)
for lib in %{buildroot}%{_libdir}/php-firebird/*.so*; do
    [ -f "$lib" ] && [ ! -L "$lib" ] && \
        patchelf --force-rpath --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
done

%check
# PHPT tests require a running Firebird server - skip in package build

%files
%{ext_dir}/firebird.so
%{ext_dir}/pdo_fbird.so
%{_libdir}/php-firebird/
%{_datadir}/php/Firebird/
%config(noreplace) %{_sysconfdir}/php.d/20-firebird.ini
%config(noreplace) %{_sysconfdir}/php.d/30-pdo_fbird.ini
%license LICENSE
%doc README.md CHANGELOG.md

%changelog
* Fri Aug 08 2026 satware AG <info@satware.com> - 13.1.0-1
- Transparent DDL commit+restart for explicit transactions (#540)
- APT install smoke test job (#490)
- INSTALL-GITHUB.md documentation (#505)

* Thu Aug 07 2026 satware AG <info@satware.com> - 13.0.3-1
- Initial RPM spec for EL8/9 + Fedora
- Mirrors packaging/debian/ structure with RPM conventions
- Bundles FB5 client library (FB4+ code paths always enabled)
