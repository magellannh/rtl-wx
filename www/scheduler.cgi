#!/bin/sh
echo Content-type: text/html
echo
cat misc/header.htm
cat misc/scheduler.htm
if [ -z "$QUERY_STRING" ]; then
	echo "<div align="left"><H2 style=\"text-indent:140px;\">Current Scheduler Status</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r a
	echo "</PRE>"
	echo "(Note: Edit rtl-wx.conf and reload it to change action frequencies.)"
elif [ $QUERY_STRING = "a" ]; then
	echo "<div align="left"><H2 style=\"text-indent:140px;\">Current Scheduler Status</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r a
	echo "</PRE>"
	echo "(Note: Edit rtl-wx.conf and reload it to change action frequencies.)"
elif [ $QUERY_STRING = "s" ]; then
	echo "<div align="center"><H2>"
	../bin/rtl-wx -r s
	echo "Data snapshot saved</H2></div>"
elif [ $QUERY_STRING = "c" ]; then
	echo "<div align="center"><H2>"
	../bin/rtl-wx -r c
	echo "Configuration file reloaded</H2></div>"
elif [ $QUERY_STRING = "f" ]; then
	echo "<div align="center"><H2>"
	../bin/rtl-wx -r f
	echo "Tag files processed</H2></div>"
elif [ $QUERY_STRING = "u" ]; then
	echo "<div align="center"><H2>"
	../bin/rtl-wx -r u
	echo "FTP upload completed</H2></div>"
elif [ $QUERY_STRING = "w" ]; then
	echo "<div align="center"><H2>Taking Webcam Image Snapshot..."
	../bin/rtl-wx -r w
	echo "Done</H2></div>"
else
	echo "scheduler.cgi: Unrecognized parameter encountered"
fi
cat misc/trailer.htm
