# TuxFetch
This is the official repository for TuxFetch, a verbose Linux command-line information tool written in C.

## build

The only dependency is libpci-dev or pciutils-dev depending on your distro
https://github.com/pciutils/pciutils and a c compiler (clang, gcc and tcc tested).

```
#arch
pacman -S pciutils
#debian/ubuntu/etc.
apt install libpci-dev
#alpine
apk add musl-dev pciutils-dev
cc tuxfetch.c -lpthread -lpci -O3 -o tuxfetch
```

# Made with love by Cob:web Development and our Open source contributors:

MathGeniusJodie - Lead Developer

### Please join the Cob:web Development discord to talk to us and contribute to our projects: https://cob-web.xyz/discord
