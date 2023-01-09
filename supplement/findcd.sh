#!/bin/sh


mkdir -p /cdroms


# Find and mount all CDROMs

i=0
# "fd*:tty*:zero:null:console:random:vcs*:vga*:pty*"

CD=

# cd /dev

for f in $(find /dev ! -name 'fd*' ! -name 'tty*' ! -name 'vcs*' ! -name 'pty*' ! -name 'zero' ! -name 'null' ! -name 'console' ! -name 'random' ! -name 'loop*')
do
	mkdir -p /cdroms/$i
	if mount -t iso9660 $f /cdroms/$i >/dev/null 2>&1 ; then
	        # echo cdrom found: $f -- /cdroms/$i
		if find /cdroms/$i/bzImage >/dev/null 2>&1 ; then
			CD=/cdroms/$i
			# echo Win98 QuickInstall CD found at $f, mounted at $CD
			break
		fi
		((i=i+1))
	fi
done

# back to root
cd /

if [[ -z "$CD" ]] ; then
	echo CD-ROM Not found!
	return 127
else
	echo CD-ROM found and mounted at $CD
	export CDROM=$CD
	export PATH=$PATH:$CD/bin
	return 0
fi
