#!/bin/sh

ROOT=/usr/local/lib/FlightGear/Scenery
OUTPUTDIR=./maps

mkdir -p $OUTPUTDIR

CHUNK=$1

# TMP1=`echo $CHUNK | awk -Fw '{print $2}'`
# LON=`echo $TMP1 | awk -Fn '{print $1}'`
# LAT=`echo $TMP1 | awk -Fn '{print $2}'`

for i in ${ROOT}/${CHUNK}/*; do
    # echo $i
    TMP1=`basename $i $ROOT/$CHUNK`
    TMP2=`echo $TMP1 | awk -Fw '{print $2}'`
    SIGN="-"
    if test -z $TMP2; then
        TMP2=`echo $TMP1 | awk -Fe '{print $2}'`
	SIGN=""
    fi
    LON=`echo $TMP2 | awk -Fn '{print $1}'`
    LAT=`echo $TMP2 | awk -Fn '{print $2}'`

    # echo "lon = $LON  lat = $LAT  sign = $SIGN"

    if [ -f ${TMP1}.png ]; then
	echo "${TMP1}.png exits, skipping"
    else
        echo ./Map --verbose \
	    --lat=${LAT} --lon=${SIGN}${LON} --autoscale \
	    --size=256 --disable-airports --disable-navaids \
	    --output=${OUTPUTDIR}/${TMP1}.png

        ./Map --verbose \
	    --lat=${LAT} --lon=${SIGN}${LON} --autoscale \
	    --size=256 --disable-airports --disable-navaids \
	    --output=${OUTPUTDIR}/${TMP1}.png
    fi

    # xv ${TMP1}.png
done
