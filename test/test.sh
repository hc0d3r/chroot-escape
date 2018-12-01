#!/bin/bash
# simple test:
# spawn a process, out of chroot jail,
# with the same uid and gid of the
# chroot process, then try escape

if [[ $UID -ne 0 ]];then
	echo "you must be ro.ot"
	exit
fi

./suid-sleep &
echo "+ ./suid-sleep & [$!]"
echo "+ chroot --userspec=57005:57005 jail/ /chroot-escape -p $! -f"
chroot --userspec=57005:57005 jail/ /chroot-escape -p $! -f
wait
