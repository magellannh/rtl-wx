#!/bin/sh
echo Content-type: text/html
echo
cat misc/header.htm
cat misc/deviceinfo.htm
if [ -z "$QUERY_STRING" ] || [ $QUERY_STRING = "showConfiguration" ]; then
	echo "<div align="left"><H2 style=\"text-indent:140px;\">Current Configuration</H2></div>"
	echo "<PRE>"
	../bin/rtl-wx -r i
	echo "</PRE>"
	echo "(NOTE: Edit rtl-wx.conf and reload it to change these settings.)"
	echo "<PRE>"
elif [ $QUERY_STRING = "reloadConfigFile" ]; then
	echo "<div align="center"><H2>"
	../bin/rtl-wx -r c
	echo "Configuration file reloaded</H2></div>"
elif [ $QUERY_STRING = "showWebcamImage" ]; then
	echo "<div align="center"><H2>Latest Webcam Image</H2></div>"
	echo "<div align="center"><img border="0" src="webcam.jpg">"
  	echo "<div align="center">"
  	echo Captured on: `date -r webcam.jpg "+%a %b %d %Y at %l:%M %p"`
elif [ $QUERY_STRING = "captureWebcamImage" ]; then
	echo "<div align="center"><H2>Taking Webcam Image Snapshot..."
	../bin/rtl-wx -r w
	echo "Done</H2></div>"
elif [ $QUERY_STRING = "rebootDevice" ]; then
	echo "<div align="center"><H2><p>Confirm Reboot Device</p><a href="deviceinfo.cgi?reboot-really">Click here to reboot now</a></H2></div>"
elif [ $QUERY_STRING = "reboot-really" ]; then
#	echo "<div align="center"><H2><p>Rebooting Device...</p>Should be back in 2 minutes.</H2></div>"
#	sudo reboot
	echo "<div align="center"><H2><p>Sorry, Reboot functionality disabled in sdr-wx/www/deviceinfo.cgi.</H2></div>"
else
	echo "<div align="left"><H2>deviceinfo.cgi: Invalid Parameter</H2></div>"
fi
echo "</PRE>"
cat misc/trailer.htm

