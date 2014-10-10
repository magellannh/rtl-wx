#!/bin/sh
echo Content-type: text/html
echo
cat misc/header.htm
cat misc/index.htm
if [ -z "$QUERY_STRING" ] || [ $QUERY_STRING = "showLatestData" ]; then
	echo "<div align="left"><H2 style=\"text-indent:165px;\">Latest Sensor Data</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r x
elif [ $QUERY_STRING = "showSensorStatus" ]; then
	echo "<div align="left"><H2 style=\"text-indent:175px;\">Current Sensor Status</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r d
	echo "</PRE>"
elif [ $QUERY_STRING = "showEnergyData" ]; then
	echo "<div align="left"><H2 style=\"text-indent:175px;\">Recent Energy Sensor Data (watts)</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r e
	echo "</PRE>"
elif [ $QUERY_STRING = "clearLockCodes" ]; then
	../bin/rtl-wx -r r
	echo  "<div align="center"><H2>All sensor lock codes have been cleared</H2></div>"
	echo "<PRE>"
elif [ $QUERY_STRING = "showMaxMinData" ]; then
	echo "<div align="left"><H2 style=\"text-indent:75px;\">Current Max/Min Sensor Information</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r m
elif [ $QUERY_STRING = "resetMaxMinData" ]; then
	../bin/rtl-wx -r n
	echo  "<div align="center"><H2>Max/Min data has been reset</H2></div>"
	echo "<PRE>"
else
	echo "<div align="left"><H2>index.cgi: Invalid Parameter</H2></div>"
fi
cat misc/trailer.htm

