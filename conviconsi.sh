#!/bin/sh

DIR=/srv/site/auno/www/res/aotextures
for i in $(echo 1010004-*|sed -e s,.png,,g -e s,1010004-,,g)
do \
  fo=1010004-$i.png
  fn=$DIR/$i.gif
  if test '!' -f $fn
  then \
    echo Converting $fo to $fn
    convert -comment AUNO.ORG -transparent lime $fo $fn
  fi
done

DIR=/srv/site/auno/www/res/aoicons
for i in $(echo 1010008-*|sed -e s,.png,,g -e s,1010008-,,g)
do \
  fo=1010008-$i.png
  fn=$DIR/$i.gif
  if test '!' -f $fn
  then \
    echo Converting $fo to $fn
    convert -comment AUNO.ORG -transparent lime $fo $fn
  fi
done
