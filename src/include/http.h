#ifndef HTTP_H
#define HTTP_H

extern int http(const char *url,
	       int (*fnc)(unsigned char *, unsigned int, unsigned int, int));

#endif /* HTTP_H */
