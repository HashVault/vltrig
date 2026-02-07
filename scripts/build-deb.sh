#!/bin/sh
set -e

BINARY="$1"
VERSION="$2"
CONFIG="$3"
PKG_NAME="$4"
SERVICE="$5"
DESCRIPTION="$6"
ARCH="amd64"

if [ -z "$BINARY" ] || [ -z "$VERSION" ] || [ -z "$CONFIG" ] || [ -z "$PKG_NAME" ] || [ -z "$SERVICE" ] || [ -z "$DESCRIPTION" ]; then
    echo "Usage: $0 <binary> <version> <config> <pkg-name> <service-file> <description>"
    exit 1
fi

STAGING="${PKG_NAME}_${VERSION}_${ARCH}"

rm -rf "$STAGING"
mkdir -p "${STAGING}/DEBIAN"
mkdir -p "${STAGING}/usr/bin"
mkdir -p "${STAGING}/etc/${PKG_NAME}"
mkdir -p "${STAGING}/usr/lib/systemd/system"

cp "$BINARY" "${STAGING}/usr/bin/${PKG_NAME}"
chmod 755 "${STAGING}/usr/bin/${PKG_NAME}"

cp "$CONFIG" "${STAGING}/etc/${PKG_NAME}/config.json"
chmod 644 "${STAGING}/etc/${PKG_NAME}/config.json"

cp "$SERVICE" "${STAGING}/usr/lib/systemd/system/${PKG_NAME}.service"
chmod 644 "${STAGING}/usr/lib/systemd/system/${PKG_NAME}.service"

cat > "${STAGING}/DEBIAN/control" << EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HashVault <root@hashvault.pro>
Description: ${DESCRIPTION}
Homepage: https://github.com/HashVault/${PKG_NAME}
Section: utils
Priority: optional
EOF

echo "/etc/${PKG_NAME}/config.json" > "${STAGING}/DEBIAN/conffiles"

cat > "${STAGING}/DEBIAN/postinst" << EOF
#!/bin/sh
set -e
if ! getent passwd ${PKG_NAME} >/dev/null 2>&1; then
    useradd --system --no-create-home --shell /usr/sbin/nologin ${PKG_NAME}
fi
systemctl daemon-reload
EOF
chmod 755 "${STAGING}/DEBIAN/postinst"

cat > "${STAGING}/DEBIAN/prerm" << EOF
#!/bin/sh
set -e
if systemctl is-active --quiet ${PKG_NAME}; then
    systemctl stop ${PKG_NAME}
fi
systemctl disable ${PKG_NAME} 2>/dev/null || true
EOF
chmod 755 "${STAGING}/DEBIAN/prerm"

cat > "${STAGING}/DEBIAN/postrm" << EOF
#!/bin/sh
set -e
systemctl daemon-reload
EOF
chmod 755 "${STAGING}/DEBIAN/postrm"

dpkg-deb --build --root-owner-group "$STAGING"
echo "Built ${STAGING}.deb"
