#!/bin/bash

# Set package name and version
PACKAGE_NAME="K9-DE"
VERSION="1.0-1"
MAINTAINER="WolfTech Innovations"
ARCHITECTURE="amd64"
DESCRIPTION="A Desktop Env for WolfOS"
LICENSE="GPL"
SECTION="utils"
PRIORITY="optional"
DISTRO="stable"
STANDARDS_VERSION="4.5.0"
BUILD_DEPENDS="debhelper-compat (= 12)"

# Create the package directory structure
mkdir -p ${PACKAGE_NAME}/debian/source
mkdir -p ${PACKAGE_NAME}/usr/bin
mkdir -p ${PACKAGE_NAME}/DEBIAN

# Copy source files into package structure
cp -r src/* ${PACKAGE_NAME}/usr/bin/

# Create debian/control file
cat <<EOF > ${PACKAGE_NAME}/DEBIAN/control
Package: $PACKAGE_NAME
Version: $VERSION
Section: $SECTION
Priority: $PRIORITY
Architecture: $ARCHITECTURE
Maintainer: $MAINTAINER
Depends: bash, coreutils, libc6
Description: $DESCRIPTION
 K9 is a desktop env for any use case,
EOF

# Create postinst script to run install.bash on package installation
cat <<EOF > ${PACKAGE_NAME}/DEBIAN/postinst
#!/bin/bash
set -e

# Run install script if it exists
if [ -f "/usr/bin/install.bash" ]; then
    chmod +x /usr/bin/install.bash
    /usr/bin/install.bash
fi

exit 0
EOF
chmod +x ${PACKAGE_NAME}/DEBIAN/postinst

# Create debian/copyright
cat <<EOF > ${PACKAGE_NAME}/DEBIAN/copyright
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: $PACKAGE_NAME
Source: none

Files: *
Copyright: $(date +%Y), $MAINTAINER
License: $LICENSE
EOF

# Create README.Debian
cat <<EOF > ${PACKAGE_NAME}/DEBIAN/README.Debian
This is a Debian package for $PACKAGE_NAME.
EOF

# Set correct permissions
find ${PACKAGE_NAME}/ -type d -exec chmod 755 {} +
find ${PACKAGE_NAME}/ -type f -exec chmod 644 {} +
chmod 755 ${PACKAGE_NAME}/DEBIAN/postinst

# Build the .deb package
dpkg-deb --build ${PACKAGE_NAME}

echo "Debian package '$PACKAGE_NAME.deb' created successfully."
