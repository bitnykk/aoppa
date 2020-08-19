#!/bin/sh

previ=11000000
prevn=11000000

compare_ids()
{
  echo "# $1 vs $2"
  diff $1 $2 | sed -ne 's,^> \([0-9]*\)$,\1 '$3',p'
}

for p in $(ls -1 ids|cut -f3 -d-|cut -f1 -d.|sort -u)
do \
  if test -f ids/ids-items-$p.txt
  then \
    compare_ids ids/ids-items-$previ.txt ids/ids-items-$p.txt $p
    previ=$p
  fi
  if test -f ids/ids-nanos-$p.txt
  then \
    compare_ids ids/ids-nanos-$previ.txt ids/ids-nanos-$p.txt $p
    prevn=$p
  fi
done
