#
# Regular cron jobs for the gstreamer1.0-plugins-studio package
#
0 4	* * *	root	[ -x /usr/bin/gstreamer1.0-plugins-studio_maintenance ] && /usr/bin/gstreamer1.0-plugins-studio_maintenance
