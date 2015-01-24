#!/usr/bin/env bash
# Usage: dogfood.sh tempfile-stem path-to-tas file.tas[.cpp] [ files... ]
here=`dirname $BASH_SOURCE`
stem=`basename $1`
tas=$2
shift
shift
base=`mktemp -t $stem`

function fail ()
{
	file=$1
	bn=$(basename $file)
	echo $bn: FAILED
	mkdir -p dogfood_failures/$bn
	cp -p $file $base.{en,de}.? dogfood_failures/$bn/
}

for file in $* ; do
	trap "rm $base.{en,de}.[123]" EXIT
	if [[ $file = *.cpp ]] ; then
		pp="cpp -I$here/../lib"
	else
		pp=cat
	fi
	$pp $file | $tas -f text - | tee $base.en.1 | $tas -d -f text - | tee $base.de.1 | $tas -f text - | tee $base.en.2 | $tas -d -f text - | tee $base.de.2 | $tas -f text -o $base.en.3 -
	diff -q $base.en.1 $base.en.2 && diff -q $base.en.2 $base.en.3 && echo $(basename $file): OK || fail $file
done

base=random
$here/random.sh | tee $base.en.1 | $tas -d -f text - | tee $base $base.de.1 | $tas -f text - | tee $base.en.2 | $tas -d -f text - | tee $base.de.2 | $tas -f text -o $base.en.3 -
diff -q $base.en.1 $base.en.2 && diff -q $base.en.2 $base.en.3 && echo $(basename $file): OK || fail $base

