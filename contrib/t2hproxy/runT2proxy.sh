#!/bin/sh

# If the httpclient and logging jars are not in the standard directories
# edit and uncomment
# CP='-cp /usr/local/lib/commons-httpclient-2.0-rc1.jar:/usr/local/lib/commons-logging-api.jar:/usr/local/lib/commons-logging.jar'

# Edit and uncomment to use an alternate port
# PORT='-DT2hproxy.port=1069'
PREFIX='-DT2hproxy.prefix=http://localhost/'
# Edit and uncomment to use a proxy
# PROXY='-DT2hproxy.proxy=localhost:3128'
# These T2hproxy properties can be put in a file and read in all at once
# PROPERTIES='-DT2hproxy.properties=t2hproxy.prop

exec java -jar $CP $PORT $PREFIX $PROXY $PROPERTIES T2hproxy.jar
