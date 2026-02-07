#!/bin/sh
set -e

DEB_FILE="$1"

if [ -z "$DEB_FILE" ]; then
    echo "Usage: $0 <deb-file>"
    exit 1
fi

PKG_NAME=$(dpkg-deb -f "$DEB_FILE" Package)
REPO_DIR="pages-repo"

# Clone hashvault.github.io
if [ -n "$PAGES_DEPLOY_KEY" ]; then
    mkdir -p ~/.ssh
    echo "$PAGES_DEPLOY_KEY" > ~/.ssh/deploy_key
    chmod 600 ~/.ssh/deploy_key
    export GIT_SSH_COMMAND="ssh -i ~/.ssh/deploy_key -o StrictHostKeyChecking=no"
    REPO_URL="git@github.com:${GITHUB_REPOSITORY_OWNER}/hashvault.github.io.git"
else
    REPO_URL="https://x-access-token:${GITHUB_TOKEN}@github.com/${GITHUB_REPOSITORY_OWNER}/hashvault.github.io.git"
fi

git clone "$REPO_URL" "$REPO_DIR" 2>/dev/null || {
    mkdir "$REPO_DIR"
    cd "$REPO_DIR"
    git init
    git remote add origin "$REPO_URL"
    cd ..
}

cd "$REPO_DIR"

# Set up directory structure
mkdir -p apt/pool/main
mkdir -p apt/dists/stable/main/binary-amd64

# Remove old version of this package only, add new
rm -f apt/pool/main/${PKG_NAME}_*_amd64.deb
cp "../$DEB_FILE" apt/pool/main/

# Generate Packages (run from apt/ so paths are relative)
cd apt
apt-ftparchive packages pool/ > dists/stable/main/binary-amd64/Packages
gzip -k -f dists/stable/main/binary-amd64/Packages

# Generate Release
apt-ftparchive release \
    -o APT::FTPArchive::Release::Origin="HashVault" \
    -o APT::FTPArchive::Release::Label="HashVault" \
    -o APT::FTPArchive::Release::Suite="stable" \
    -o APT::FTPArchive::Release::Codename="stable" \
    -o APT::FTPArchive::Release::Components="main" \
    -o APT::FTPArchive::Release::Architectures="amd64" \
    dists/stable/ > dists/stable/Release
cd ..

# GPG sign
echo "$GPG_PASSPHRASE" | gpg --batch --yes --pinentry-mode loopback \
    --passphrase-fd 0 --armor --detach-sign \
    --output apt/dists/stable/Release.gpg \
    apt/dists/stable/Release

echo "$GPG_PASSPHRASE" | gpg --batch --yes --pinentry-mode loopback \
    --passphrase-fd 0 --clearsign \
    --output apt/dists/stable/InRelease \
    apt/dists/stable/Release

# Export public key
gpg --armor --export > apt/KEY.gpg

# Commit and push
git add -A
git config user.name "github-actions[bot]"
git config user.email "github-actions[bot]@users.noreply.github.com"
git diff --cached --quiet || git commit -m "Update APT repository - ${PKG_NAME} $(dpkg-deb -f "../$DEB_FILE" Version)"
git push origin HEAD
