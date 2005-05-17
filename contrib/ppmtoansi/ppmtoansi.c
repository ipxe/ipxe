#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static int palette[8][3] = {
/* black,        red,          green,        yellow,      */ 
  {  0,  0,  0},{255,  0,  0},{  0,255,  0},{255,255,  0},
  {  0,  0,255},{255,  0,255},{  0,255,255},{255,255,255}};
/* blue,         magenta,      cyan,         white        */

static struct trans {
  struct trans *next;
  int          idx,r,g,b;
} *trans = NULL;

static int skipcomment(FILE *fp)
{
  int ch;
  
  for (;;) {
    ch = getc(fp);
    if (ch != '#')
      return(ch);
    while (ch != '\n' && ch != EOF)
      ch = getc(fp); }
}

static int readentry(FILE *fp,int format,int depth)
{
  int ch,i = 0;
  
  if (format == '3') {
    while ((ch = getc(fp)) == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
    if (ch < '0' || ch > '9') {
    error:
      fprintf(stderr,"Format error in input file\n");
      exit(1); }
    for (; ch >= '0' && ch <= '9'; ch = getc(fp))
      i = 10*i + ch - '0'; }
  else {
    if ((i = getc(fp)) > depth || i < 0)
      goto error; }
  return((i*256)/(depth+1));
}

static void packpixel(char *data,int c)
{
  int i = 0, n = 0;

  while (c--) {
    i = (i << 3) | (*data++ & 0x7);
    if ((n += 3) >= 8)
      putchar((i >> (n -= 8)) & 0xFF); }
  if (n)
    putchar(i << (8 - n));
  return;
}

static int dg(int i)
{
  int d;

  for (d = 0; i; d++, i /= 10);
  return(d);
}

static char *i2s(char *buf,int i)
{
/*  if (!i)
    *buf = '\000';
  else*/
    sprintf(buf,"%d",i);
  return(buf);
}

static void flushdata(int x,int y,int c,char *data)
{
  char b1[10],b2[10],b3[10],b4[10];
  int i,j,rle,v;

  for (i = j = v = 0; i < c; ) {
    for (rle = 0; i+rle < c && data[i] == data[i+rle]; rle++);
    if (rle > (i != j ? (v ? 4 : 6) : 0) +
	      ((v || (i != j)) ? 4+dg(rle)+dg(data[i])
	                       : 6+dg(x+i)+dg(y)+dg(rle)+dg(data[i]))) {
      if (i != j) {
	if (v)
	  printf("[%s-",i2s(b1,i-j));
	else
	  printf("[%s;%s;%s-",i2s(b1,x+j),i2s(b2,y),i2s(b3,i-j));
	packpixel(data+j,i-j); }
      if (v++ || (i != j))
	printf("[%s;%s+",i2s(b1,rle),i2s(b2,data[i]));
      else
	printf("[%s;%s;%s;%s+",i2s(b1,x+i),i2s(b2,y),
	       i2s(b3,rle),i2s(b4,data[i]));
      j = i += rle; }
    else
      i++; }
  if (j != c) {
    if (v)
      printf("[%s-",i2s(b1,c-j));
    else
      printf("[%s;%s;%s-",i2s(b1,x+j),i2s(b2,y),i2s(b3,c-j));
    packpixel(data+j,c-j); }
  return;
}

int main(int argc,char *argv[])
{
  extern int optind;
  extern char *optarg;
  FILE *infile = NULL;
  int  ch,i,j,dist,idx;
  int  format,width,height,depth;
  int  bg = 0,bgred = 0,bggreen = 0,bgblue = 0;
  int  xoffset = 0,yoffset = 0;
  int  w,h,r,g,b,c;
  struct trans *tp;
  char *buf;
  
  while ((i = getopt(argc,argv,"b:t:x:y:")) >= 0) switch(i) {
  case 'b':
    bg++;
    for (i = bgred = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 bgred = 10*bgred + optarg[i++] - '0');
    if (optarg[i++] != '/')
      goto usage;
    for (bggreen = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 bggreen = 10*bggreen + optarg[i++] - '0');
    if (optarg[i++] != '/')
      goto usage;
    for (bgblue = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 bgblue = 10*bgblue + optarg[i++] - '0');
    if (optarg[i])
    goto usage;
    break;
  case 't':
    if ((tp = malloc(sizeof(struct trans))) == NULL)
      goto usage;
    for (i = tp->r = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 tp->r = 10*tp->r + optarg[i++] - '0');
    if (optarg[i++] != '/')
      goto usage;
    for (tp->g = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 tp->g = 10*tp->g + optarg[i++] - '0');
    if (optarg[i++] != '/')
      goto usage;
    for (tp->b = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 tp->b = 10*tp->b + optarg[i++] - '0');
    if (optarg[i++] != ':')
      goto usage;
    if (optarg[i] == '-') {
      j = -1; i++; }
    else j = 1;
    for (tp->idx = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 tp->idx = 10*tp->idx + optarg[i++] - '0');
    tp->idx *= j;
    if (tp->idx < -1 || tp->idx >= 8)
      goto usage;
    if (optarg[i]) 
      goto usage;
    tp->next = trans;
    trans    = tp;
    break;
  case 'x':
    for (i = xoffset = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 xoffset = 10*xoffset + optarg[i++] - '0');
    if (optarg[i])
      goto usage;
    break;
  case 'y':
    for (i = yoffset = 0; optarg[i] >= '0' && optarg[i] <= '9';
	 yoffset = 10*yoffset + optarg[i++] - '0');
    if (optarg[i])
      goto usage;
    break;
  default:
  usage:
    fprintf(stderr,"Usage: %s [-b r/g/b] [-t r/g/b:idx] "
	    "[-x offset] [-y offset] [ppmfile]\n",argv[0]);
    exit(1); }
  if (argc-optind == 0)
    infile = stdin;
  else if (argc-optind == 1)
    infile = fopen(argv[optind],"r");
  if (!infile)
    goto usage;
  if ((ch = skipcomment(infile)) != 'P' ||
      ((format = getc(infile)) != '3' && format != '6') ||
      ((ch = getc(infile)) != '\n' && ch != '\r' && getc(infile) != '\n'))
    goto usage;
  for (width = 0; (ch = skipcomment(infile)) >= '0' && ch <= '9';
       width = 10*width + ch - '0');
  while (ch == ' ') ch = getc(infile);
  for (height = 0; ch >= '0' && ch <= '9'; ch = getc(infile))
    height = 10*height + ch - '0'; 
  if (ch != '\n' && ch != '\r' && getc(infile) != '\n')
    goto usage;
  for (depth = 0; (ch = skipcomment(infile)) >= '0' && ch <= '9';
       depth = 10*depth + ch - '0');
  if (ch != '\n' && ch != '\r' && getc(infile) != '\n')
    goto usage;
  if (!width || !height || !depth /* || depth > 255 */)
    goto usage;
  if ((buf = malloc(width)) == NULL)
    goto usage;
  for (h = 0; h < height; h++) {
    for (w = c = 0; w < width; w++) {
      r = readentry(infile,format,depth);
      g = readentry(infile,format,depth);
      b = readentry(infile,format,depth);
      idx = 255;
      if (bg && bgred == r &&
	  bggreen == g && bgblue == b)
	idx = -1;
      else for (tp = trans; tp; tp = tp->next)
	if (tp->r == r && tp->g == g && tp->b == b) {
	  idx = tp->idx;
	  break; }
      if (idx == 255)
	for (idx = -1, dist = 3*255*255, i = 8; i--;)
	  if ((j = (r-palette[i][0])*(r-palette[i][0]) +
	           (g-palette[i][1])*(g-palette[i][1]) +
	           (b-palette[i][2])*(b-palette[i][2])) < dist) {
	    dist = j; idx = i; }
      if (idx >= 0)
	buf[c++] = idx;
      else if (c) {
	flushdata(w-c+xoffset,h+yoffset,c,buf);
	c = 0; } }
    if (c)
      flushdata(w-c+xoffset,h+yoffset,c,buf); }
  exit(0);
}
