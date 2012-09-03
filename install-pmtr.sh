#!/bin/bash

# 
# this installer makes a guess whether this is RHEL/Centos, Ubuntu 
# or Debian (sorry to other platforms, please send me improvements)
# and tries to install a suitable init script for the platform.
#
# Troy D. Hanson <tdh@tkhanson.net>
# 9/2012
#

if [ ! -x ./pmtr ]
then
  echo "./pmtr not found, please run make"
  exit -1
fi

if [ "$UID" -ne 0 ]
then
  echo "this script should be run as root"
  exit -1
fi

OS=
if [ -f /etc/redhat-release ]; then OS=rhel; fi
if [ -f /etc/debian_version ]; then OS=debian; fi
if [ -f /etc/lsb-release ]
then
    /bin/grep -q Ubuntu /etc/lsb-release
    if [ $? -eq 0 ]; then OS=ubuntu; fi
fi
if [ -z "$OS" ]
then
  echo "unknown OS, not sure what initscript to use, sorry."
  echo "please help improve install-pmtr.sh if you can."
  exit -1
fi

BINDIR=/usr/bin
if [ ! -d "$BINDIR" ]
then
  echo "the install directory $BINDIR does not exist."
  echo "please change BINDIR in install-pmtr.sh and re-run."
  exit -1
fi

cp ./pmtr "$BINDIR"
touch /etc/pmtr.conf

case "$OS" in 
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

