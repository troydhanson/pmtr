#!/bin/bash

# 
# this installer makes a guess whether this is RHEL/Centos, Ubuntu or
# Debian and whether it's based on systemd, upstart or legacy initscript.
# It installs pmtr and sets it up under that init system.
#
# Troy D. Hanson <tdh@tkhanson.net>
#
#  9/2012
# 11/2015 updated for systemd on Ubuntu 15, Intel Edison
#

if [ "$UID" -ne 0 ]
then
  echo "this script should be run as root"
  exit -1
fi

if [ ! -x ./pmtr ]
then
  echo "./pmtr not found, please run make"
  exit -1
fi

BINDIR=/usr/bin
if [ ! -d "$BINDIR" ]
then
  echo "the install directory $BINDIR does not exist."
  echo "please change BINDIR in install-pmtr.sh and re-run."
  exit -1
fi

TYPE=
if [ -L /sbin/init ]
then
  `readlink /sbin/init | grep -q systemd`
  if [ $? -eq 0 ]; then TYPE=systemd; fi
fi

if [ -z "$TYPE" ]
then
  if [ -f /etc/redhat-release ]; then TYPE=rhel; fi
  if [ -f /etc/debian_version ]; then TYPE=debian; fi
  if [ -f /etc/lsb-release ]
  then
      /bin/grep -q Ubuntu /etc/lsb-release
      if [ $? -eq 0 ]; then TYPE=ubuntu; fi
  fi
  if [ -z "$TYPE" ]
  then
    echo "unknown OS/initsystem. installation cancelled."
    echo "please help improve install-pmtr.sh if you can."
    exit -1
  fi
fi

echo "Installing ${BINDIR}/pmtr"
cp ./pmtr "$BINDIR"
chmod a+rx ${BINDIR}/pmtr
touch /etc/pmtr.conf

echo "Installing to: ${BINDIR}/pmtr"
echo "Initsystem/OS: $TYPE"
case "$TYPE" in 
  systemd)
    cp initscripts/pmtr.service /lib/systemd/system/pmtr.service
    systemctl enable pmtr
    systemctl start pmtr
    systemctl status pmtr
  rhel)
    cp initscripts/rhel /etc/rc.d/init.d/pmtr
    /sbin/chkconfig --add pmtr
    /etc/init.d/pmtr start
    ;;
  ubuntu)
    cp initscripts/ubuntu /etc/init/pmtr.conf
    /sbin/start pmtr
    ;;
  debian)
    cp initscripts/debian /etc/init.d/pmtr
    /usr/sbin/update-rc.d pmtr defaults
    /usr/sbin/service pmtr start
    ;;
  *)
    echo "unknown platform"
    exit -1
    ;;
esac

