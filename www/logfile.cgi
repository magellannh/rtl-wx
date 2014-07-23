#!/bin/sh
echo Content-type: text/html
echo
cat misc/header.htm
cat misc/logfile.htm
# If called without parameters, just instruct the wmr server to dump current info
if [ -z "$QUERY_STRING" ]; then
	echo "<div align="left"><H2 style=\"text-indent:130px;\">Current Log File Contents</H2></div>"
	echo "<PRE>"
	cat rtl-wx.log
elif [ $QUERY_STRING = "l" ]; then
	../bin/rtl-wx -r l
	echo  "<div align="center"><H2>Clearing log file... Done</H2></div>"
	echo "<PRE>"
else
	echo "<div align="left"><H2>Unrecognized Parameter</H2></div>"
	echo "<PRE>"
fi
echo "</PRE>"
cat misc/trailer.htm
