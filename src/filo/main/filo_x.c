#include <etherboot.h>

#include <fs.h>
#include <lib.h>
#include <sys_info.h>

#define ENTER '\r'
#define ESCAPE '\x1b'

#ifndef AUTOBOOT_FILE
#define autoboot() ((void) 0) /* nop */
#endif

#ifndef AUTOBOOT_DELAY
#define autoboot_delay() 0 /* success */
#endif

struct sys_info sys_info;

static void init(void)
{
    collect_sys_info(&sys_info);

    printf("%s version %s\n", program_name, program_version);

}

static void boot(const char *line)
{
    char file[256], *param;

    /* Split filename and parameter */
    memcpy(file, line,256);
//    file = strdup(line);
    param = strchr(file, ' ');
    if (param) {
	*param = '\0';
	param++;
    }
    if (elf_load(&sys_info, file, param) == LOADER_NOT_SUPPORT){
	if (linux_load(&sys_info, file, param) == LOADER_NOT_SUPPORT)
	    printf("Unsupported image format\n");
    }
//    free(file);
}

#ifdef AUTOBOOT_FILE
#if AUTOBOOT_DELAY

static inline int autoboot_delay(void)
{
    unsigned int timeout;
    int sec, tmp;
    int key;
    
    key = 0;

    printf("Press <Enter> for default boot, or <Esc> for boot prompt... ");
    for (sec = AUTOBOOT_DELAY; sec>0 && key==0; sec--) {
	printf("%d", sec);
	timeout = currticks() + TICKS_PER_SEC;
	while (currticks() < timeout) {
	    if (iskey()) {
		key = getchar();
		if (key==ENTER || key==ESCAPE)
		    break;
	    }
	}
	for (tmp = sec; tmp; tmp /= 10)
	    printf("\b \b");
    }
    if (key == 0) {
	printf("timed out\n");
	return 0; /* success */
    } else {
	putchar('\n');
	if (key == ESCAPE)
	    return -1; /* canceled */
	else
	    return 0; /* default accepted */
    }
}
#endif /* AUTOBOOT_DELAY */

static void autoboot(void)
{
    /* If Escape key is pressed already, skip autoboot */
    if (iskey() && getchar()==ESCAPE)
	return;

    if (autoboot_delay()==0) {
	printf("boot: %s\n", AUTOBOOT_FILE);
	boot(AUTOBOOT_FILE);
    }
}
#endif /* AUTOBOOT_FILE */

/* The main routine */
int filo(void)
{
    char line[256];

    /* Initialize */

    init();

    /* Try default image */
    autoboot();

    /* The above didn't work, ask user */
    while (iskey())
	getchar();
#ifdef AUTOBOOT_FILE
    strncpy(line, AUTOBOOT_FILE, sizeof(line)-1);
    line[sizeof(line)-1] = '\0';
#else
    line[0] = '\0';
#endif
    for (;;) {
	printf("boot: ");
	getline(line, sizeof line);
// BY LYH add "quit" to exit filo
	if (strcmp(line,"quit")==0) break;
//	if (memcmp(line,"quit",4)==0) break;
	if (line[0])
	    boot(line);
    }
   return 0;
}
