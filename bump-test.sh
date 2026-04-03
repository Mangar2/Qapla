#!/bin/bash
# bump-test.sh - Create next test version tag and optionally commit
#
# Usage:
#   ./bump-test.sh                    # just create next tag on current HEAD
#   ./bump-test.sh "description"      # git commit -a -m "description", then tag

set -e

# Get latest tag matching the test pattern (e.g. 0.4.0-027)
LATEST_TAG=$(git tag --list '0.4.0-*' | sort -V | tail -1)

if [ -z "$LATEST_TAG" ]; then
    echo "Error: No existing 0.4.0-* tag found"
    exit 1
fi

# Extract test number and increment
CURRENT_NUM=$(echo "$LATEST_TAG" | sed 's/.*-//')
NEXT_NUM=$(printf "%03d" $((10#$CURRENT_NUM + 1)))
PREFIX=$(echo "$LATEST_TAG" | sed 's/-[0-9]*$//')
NEW_TAG="${PREFIX}-${NEXT_NUM}"

echo "Current tag: $LATEST_TAG"
echo "New tag:     $NEW_TAG"

# If a commit message was provided, commit first
if [ -n "$1" ]; then
    git commit -a -m "$1"
    echo "Committed: $1"
fi

git tag "$NEW_TAG"
echo "Tag $NEW_TAG created."
echo ""
echo "Next build will show: Qapla $NEW_TAG"
