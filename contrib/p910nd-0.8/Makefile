# Comment out the second command and uncomment the first command
# below if you don't want to use libwrap (hosts.{allow,deny} access control)

# If you don't have it in /var/log/subsys, uncomment and define
#CFLAGS+=-DLOCKFILE_DIR=\"/var/log\"
LIBWRAP=-lwrap

p910nd:	p910nd.c
#	$(CC) -Wall $(CFLAGS) -o $@ p910nd.c
	$(CC) -Wall $(CFLAGS) -DUSE_LIBWRAP -o $@ p910nd.c $(LIBWRAP)
