#!/bin/sh 
dirName="CopyHeadFile"
argc=$#
if [  $argc -ne 1 ];then
	echo "extra argument should be 1(directoryName)!"
	exit 1
else
    if [ -d $dirName/$1 ];then
        rm -rf $dirName/$1
    fi
fi
