# chroot-escape
try escape from chroot with non root user, for now it only works on x86_64
## Options:
```
-- chroot-escape --
- try escape from chroot with non root user

 Options:
  --proc-scan                scan procfs for out of jail pid
  --proc STRING              proc mount point (Default: /proc)
  -r, --pid-range start-end  use this options if proc are not mounted
  -p, --pid NUMBER           try escape attaching specific pid
  -P, --port NUMBER          port number to listen (Default: random)
  -a, --any                  try attach any pid
  -f, --force                ignore everything
  -h, --help                 display this help menu
```

## Example:
![chroot-escape](https://i.imgur.com/A2X36Qq.png)
