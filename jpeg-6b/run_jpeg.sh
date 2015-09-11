#!/bin/bash
set -x
BASEHOME=/home/stewart/Dropbox/nvml/src/examples/libpmemobj/jpeg-6b
BASENVLIB=$NVMALLOC_HOME
DATAORIG=$SHARED_DATA/images/
STORAGE=pmfs
EXEPATH="$BASEHOME/djpeg -dct int -bmp -outfile im1029.jpg.bmp"
#EXEPATH="LD_PRELOAD=~/codes/Hoard/src/libhoard_optimized.so $BASEHOME/djpeg -dct int -bmp -outfile im1029.jpg.bmp"
#EXEPATH="LD_PRELOAD=~/codes/Hoard/src/libhoard.so taskset -c 4 $BASEHOME/djpeg -dct int -bmp -outfile im1029.jpg.bmp"

DATASET=/mnt/$STORAGE/
PDIR=$PWD

if [[ x$1 == x ]];
   then
      echo You have specify 1 or 0 for using nvm heap interface
      exit 1
   fi

cd /mnt/$STORAGE/
rm -rf  /mnt/$STORAGE/*
rm -rf  /mnt/$STORAGE/
rm -f /mnt/$STORAGE/out/*
rm -f /mnt/$STORAGE/*.bmp
rm -f /mnt/$STORAGE/*.*.bmp
rm -f /mnt/$STORAGE/*.bmp

sudo rm -rf /tmp/ramdisk/test


if [[ x$1 == x1 ]];
   then
	rm -f /mnt/$STORAGE/*
	$BASENVLIB/test/load_file $DATAORIG 1 1
   else
	cp $DATAORIG/* /mnt/$STORAGE/
	cd $DATASET
   fi

mkdir out
sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
sudo sh -c "sync"
sudo sh -c "sync"
sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
#exit
#export LD_PRELOAD=/usr/lib/librdpmc.so
$APP_PREFIX "$EXEPATH $DATASET"
#$EXEPATH $DATASET
exit


