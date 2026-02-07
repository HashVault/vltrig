#!/bin/sh
set -e

RPM_FILE="$1"

if [ -z "$RPM_FILE" ]; then
    echo "Usage: $0 <rpm-file>"
    exit 1
fi

PKG_NAME=$(rpm -qp --queryformat '%{NAME}' "$RPM_FILE")
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

if [ ! -d "$REPO_DIR" ]; then
    git clone "$REPO_URL" "$REPO_DIR" 2>/dev/null || {
        mkdir "$REPO_DIR"
        cd "$REPO_DIR"
        git init
        git remote add origin "$REPO_URL"
        cd ..
    }
else
    cd "$REPO_DIR"
    git pull origin HEAD || true
    cd ..
fi

cd "$REPO_DIR"

# Set up directory structure
mkdir -p rpm

# Remove old version of this package only, add new
rm -f rpm/${PKG_NAME}-*.x86_64.rpm
cp "../$RPM_FILE" rpm/

# Generate repo metadata
createrepo_c rpm/

# GPG sign repomd.xml
echo "$GPG_PASSPHRASE" | gpg --batch --yes --pinentry-mode loopback \
    --passphrase-fd 0 --armor --detach-sign \
    --output rpm/repodata/repomd.xml.asc \
    rpm/repodata/repomd.xml

# Export public key
gpg --armor --export > rpm/KEY.gpg

# Commit and push
git add -A
git config user.name "github-actions[bot]"
git config user.email "github-actions[bot]@users.noreply.github.com"
git diff --cached --quiet || git commit -m "Update RPM repository - ${PKG_NAME} $(rpm -qp --queryformat '%{VERSION}' "../$RPM_FILE")"
git push origin HEAD
