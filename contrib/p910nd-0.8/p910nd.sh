#!/bin/sh
#
# p910nd.sh	This shell script takes care of starting and stopping
#               p910nd (port 9100+n printer daemon)
#		This script only controls the one on port 9101.
#		You can start others if you wish.
#

# Todo: Make it fully LSB

# See how we were called.
case "$1" in
  start)
	# Start daemons.
	echo -n "Starting p910nd: "
	# default port is 1 so it will appear as p9101d on a ps
	start_daemon p910nd
	echo
	;;
  stop)
	# Stop daemons.
	echo -n "Shutting down p910nd: "
	killproc p9101d
	echo
	rm -f /var/run/p9101.pid
        ;;
  status)
	status p9101d
	;;
  restart)
	$0 stop
	$0 start
	;;
  *)
	echo "Usage: p910nd {start|stop|restart|status}"
	exit 1
esac

exit 0
