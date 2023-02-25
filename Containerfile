FROM docker.io/library/debian:stable

# All distrobox dependencies
RUN apt-get update
RUN apt-get install -y bash apt-utils bc curl dialog diffutils findutils gnupg2 less libnss-myhostname libvte-2.9[0-9]-common libvte-common lsof ncurses-base passwd pinentry-curses procps sudo time wget util-linux
RUN apt-get install -y libegl1-mesa libgl1-mesa-glx

# Dependencies
RUN apt-get install -y make genisoimage nasm python3 g++ gcc-10-i686-linux-gnu qemu-system-x86
