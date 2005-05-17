mp-form.pl - Send magic packets with your web server

This is my trial of waking up PCs with a form via web server. 
The perl script reads in a text file which contains lines of the form
aa:bb:cc:dd:ee:ff 12.34.56.78 or
aa:bb:cc:dd:ee:ff foo.bar.com
aa:bb:cc:dd:ee:ff
optional a broadcast address mask can be prefixed:
192.168.1.255-aa:bb:cc:dd:ee:ff

The script is based on wake.pl by Ken Yap who is also the author of the Etherboot package. 
The script uses CGI.pm to parse form input and Socket.pm to send the magic packets, both 
modules should belong to your perl distribution. The script runs with linux as well as 
with NT, with NetWare you have to use the Socket.NLP from recent Perl build #334, with 
Perl build #333 and earlier I got an error from the Socket.pm. You should also update
your Perl to minimum #331 from CPAN: http://www.cpan.org/ports/index.html#netware.
This script uses only broadcast so you don't need root rights to use it.

To install the script copy it to your ../cgi-bin directory. Then for Linux do a chmod 755 
to the script. Modify the lines which point to the location of the maclist. If you do not 
have a maclist, the form could save new entries to a list (with unix perhaps you have to 
create an empty file). Check also the first line of mp-form.pl and modify it to point to 
your perl5 location. For NetWare copy the script to /novonyx/suitespot/docs/perlroot.

Older version: If you have problems running the script with CGI.pm you could try the older 
version mp-form1.pl which uses cgi-lib.pl to parse form input. You can get cgi-lib.pl 
from http://cgi-lib.berkeley.edu/. With NetWare you should also use mp-form1.pl as using 
CGI.pm consumes much memory. The older version is also included in the zip archive. 

A modified version of the original script is now also included which runs with older 
Getopt::Std.pm as shipped with NetWare. This script you could use from command line 
or from cron. With NetWare copy the script to /perl/scripts; then call from console with:
perl wakeup.pl <parameters>.

Homepage of the script: http://www.gknw.de/mpform.html
