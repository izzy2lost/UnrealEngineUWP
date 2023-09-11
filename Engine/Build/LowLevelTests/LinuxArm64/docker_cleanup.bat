@ECHO OFF
set AppNameLowercase=%1
docker rm -f %AppNameLowercase%-linuxarm64-container 2>NUL || exit 0
docker rmi %AppNameLowercase%-linuxarm64-image 2>NUL || exit 0