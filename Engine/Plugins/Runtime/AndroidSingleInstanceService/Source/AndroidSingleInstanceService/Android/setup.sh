NDKDIR=$NDKROOT;
SDKDIR=$ANDROID_HOME;
destdir=local.properties

if [ -f "$destdir" ]
then 
    echo -e "ndk.dir=$NDKDIR\nsdk.dir=$SDKDIR" > "$destdir"
    echo "local.properties has been updated"
else
    echo -e "ndk.dir=$NDKDIR\nsdk.dir=$SDKDIR" > "$destdir"
    echo "local.properties has been created"
fi
