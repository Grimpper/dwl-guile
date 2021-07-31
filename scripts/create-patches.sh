#!/bin/sh
# $1 = dwl-guile patch tag to diff patches to

git fetch --tags
rm -rf patches
mkdir -p patches

# Make sure we always have a branch/tag to diff against,
# otherwise, the patch output will be inverted.
DIFF_ORIGIN=$1
[ -z $DIFF_ORIGIN ] && DIFF_ORIGIN="main"

for patch in xwayland alpha
do
    git fetch origin patch/$patch
    git diff $1 patch/$patch \
        ':(exclude)README.md' \
        ':(exclude)patches' \
        ':(exclude)scripts/create-patches.sh' \
        ':(exclude).gitignore' > patches/$patch.patch
done

# Create dwl-guile patch based on dwl v0.2
git diff v0.2 > patches/dwl-guile.patch
