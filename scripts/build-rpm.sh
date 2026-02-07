#!/bin/sh
set -e

BINARY="$1"
VERSION="$2"
CONFIG="$3"
PKG_NAME="$4"
SERVICE="$5"
DESCRIPTION="$6"
ARCH="x86_64"
RELEASE="1"

if [ -z "$BINARY" ] || [ -z "$VERSION" ] || [ -z "$CONFIG" ] || [ -z "$PKG_NAME" ] || [ -z "$SERVICE" ] || [ -z "$DESCRIPTION" ]; then
    echo "Usage: $0 <binary> <version> <config> <pkg-name> <service-file> <description>"
    exit 1
fi

TOPDIR="$(pwd)/rpmbuild"
rm -rf "$TOPDIR"
mkdir -p "$TOPDIR/BUILD" "$TOPDIR/RPMS" "$TOPDIR/SOURCES" "$TOPDIR/SPECS" "$TOPDIR/SRPMS"

# Stage source files
SRCDIR="$TOPDIR/SOURCES"
cp "$BINARY" "$SRCDIR/${PKG_NAME}"
cp "$CONFIG" "$SRCDIR/config.json"
cp "$SERVICE" "$SRCDIR/${PKG_NAME}.service"

cat > "$TOPDIR/SPECS/${PKG_NAME}.spec" << EOF
Name:    ${PKG_NAME}
Version: ${VERSION}
Release: ${RELEASE}
Summary: ${DESCRIPTION}
License: GPL-3.0-or-later
URL:     https://github.com/HashVault/${PKG_NAME}

%description
${DESCRIPTION}

%install
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/etc/${PKG_NAME}
mkdir -p %{buildroot}/usr/lib/systemd/system
cp %{_sourcedir}/${PKG_NAME} %{buildroot}/usr/bin/${PKG_NAME}
cp %{_sourcedir}/config.json %{buildroot}/etc/${PKG_NAME}/config.json
cp %{_sourcedir}/${PKG_NAME}.service %{buildroot}/usr/lib/systemd/system/${PKG_NAME}.service

%files
%attr(755, root, root) /usr/bin/${PKG_NAME}
%config(noreplace) /etc/${PKG_NAME}/config.json
/usr/lib/systemd/system/${PKG_NAME}.service

%pre
getent passwd ${PKG_NAME} >/dev/null || useradd --system --no-create-home --shell /usr/sbin/nologin ${PKG_NAME}

%post
systemctl daemon-reload

%preun
systemctl stop ${PKG_NAME} 2>/dev/null || true
systemctl disable ${PKG_NAME} 2>/dev/null || true

%postun
systemctl daemon-reload
EOF

rpmbuild --define "_topdir $TOPDIR" -bb "$TOPDIR/SPECS/${PKG_NAME}.spec"

RPM_FILE="$TOPDIR/RPMS/${ARCH}/${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"
cp "$RPM_FILE" .
echo "Built ${PKG_NAME}-${VERSION}-${RELEASE}.${ARCH}.rpm"
