#!/bin/bash

usage ()
{
cat << EOF

Usage:
   $0 [OPTIONS]

Run docker container and build libvpx inside of it for further linux platform.

OPTIONS:
   -v VER    libVpx version [$VER]
   -d        Debug mode (trace commands)
   -h        Show this message
EOF
}

VER=${VER:-1.13.1}

while getopts :v:dh OPTION; do
  case $OPTION in
  v) VER=$OPTARG ;;
  d) DEBUG=1 ;;
  h) usage; exit 1 ;;
  esac
done

[ "$DEBUG" = 1 ] && set -x

# Stop and remove any old instances of the builder
docker stop centos7_build_libvpx > /dev/null 2>&1
docker rm centos7_build_libvpx > /dev/null 2>&1

# Build centos image
echo "Building Linux builder image..."
docker build -q -f ./centos7_build_libvpx.dockerfile -t centos7_build_libvpx . || exit 1

# Run our container with the provided options
echo "Running Linux builder image..."

MSYS_NO_PATHCONV=1 builder_args="/mnt/libvpx/Linux/build-libvpx-linux.sh -v $VER"
MSYS_NO_PATHCONV=1 [ "$DEBUG" = 1 ] && builder_args="$builder_args -d"

# If the user environment variable isn't set (ie. our host is Windows), set the user and group id's to 
# 0 (root) in the container, otherwise we will encounter file permission errors under a Windows host
if [ -z "$USER" ]; then 
	USER_ID=0
	GROUP_ID=0
fi

pushd ../
# # We want to mount parent folder for PWD (build folder)
MSYS_NO_PATHCONV=1 libvpx_root=$(pwd)
popd

# uncomment to run shell in the docker
#MSYS_NO_PATHCONV=1 builder_args="/bin/bash"

MSYS_NO_PATHCONV=1 docker run \
	--interactive \
	--name centos7_build_libvpx \
	-u $USER_ID:$GROUP_ID \
	-v "$libvpx_root:/mnt/libvpx:rw" \
	centos7_build_libvpx:latest "$builder_args" || exit 1

#Success
exit 0
