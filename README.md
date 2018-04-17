# Network Mirror

Mirror all net packet to `netmirror` interface

tested in Ubuntu 16.10 x64(4.13.0-38-generic)

# usage

    # build
    make
    # load
    sudo insmod netmirror.ko
    # capture packet
    sudo tcpdump -i netmirror
