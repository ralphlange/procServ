#!/usr/bin/env sh
set -e -x

# Build base for use with https://travis-ci.org
#
# Set environment variables
# BRANCH= 3.14 3.15 or 3.16  (VCS branch)
# STATIC= static or shared

die() {
  echo "$1" >&2
  exit 1
}

[ "$BRANCH" ] || die "Set BRANCH"
[ "$STATIC" ] || STATIC=shared

git clone --quiet --depth 10 --branch $BRANCH https://github.com/epics-base/epics-base.git "$HOME/.source/base"

HEAD=`cd "$HOME/.source/base" && git log -n1 --pretty=format:%H`
echo "HEAD revision $HEAD"

CDIR="$HOME/.cache/base-$BRANCH-$STATIC"
EPICS_BASE="$CDIR/base"

[ -d install ] || install -d "$CDIR"
touch "$CDIR/built"

BUILT=`cat $CDIR/built`
echo "BUILT revision $BUILT"


if [ "$HEAD" != "$BUILT" ]
then
  rm -rf "$CDIR"
  install -d "$CDIR"
  mv "$HOME/.source/base" "$CDIR/base"

  case "$STATIC" in
  static)
    cat << EOF >> "$EPICS_BASE/configure/CONFIG_SITE"
SHARED_LIBRARIES=NO
STATIC_BUILD=YES
EOF
     ;;
  *) ;;
  esac

  make -C "$EPICS_BASE" -j2

  echo "$HEAD" > "$CDIR/built"
fi

echo "EPICS_BASE=$EPICS_BASE" > "./.env"
