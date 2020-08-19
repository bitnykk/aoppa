#!/bin/sh
#
# $Id: getids.sh 1099 2008-09-12 12:34:47Z os $
#

cnt=0

for fnin in $(ls -1 data)
do \
  fnout="ids/ids-$(basename $(basename $fnin .gz) .dat).txt"
  if [ ! -f $fnout ]
  then \
    echo "Generating $fnout"
    cnt=$((cnt+1))
    zgrep -a '^AOID: ' data/$fnin | cut -f 2 -d ' ' > $fnout
  fi
done

echo "Done - generated $cnt files."
