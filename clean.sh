#!/bin/sh

rm -rf build-aux autom4te.cache

if hg --version &>/dev/null
then

  hg status -i -n | xargs rm -f

else
  rm -f configure aclocal.m4 Makefile.in

fi
