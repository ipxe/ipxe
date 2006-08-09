/* Command line library */
#ifndef CMDLINELIB_H
#define CMDLINELIB_H

#define CMDL_BUFFER_SIZE 256
//#define CMDL_OUTPUT_SIZE 256
#define CMDL_PROMPT_SIZE 8
#define CMDL_MAX_TAB_COMPLETE_RESULT 256

typedef int (*cmdl_putchar_t)(int);
typedef int (*cmdl_printf_t)( const char *format, ... );
typedef int (*cmdl_getchar_t)();

#ifndef NULL
#define NULL    ((void *)0)
#endif

enum{
	CMDL_LEFT,
	CMDL_RIGHT
};

enum{
	CMDLK_FW=6,
	CMDLK_BW=2,
	CMDLK_BS=8,
	CMDLK_HOME=2,
	CMDLK_END=5,
	CMDLK_DELTOEND=11,
	CMDLK_DELARG=23,
	CMDLK_ENTER=0x0d,
	CMDLK_RETURN=0x0a,
	CMDLK_TAB=9
};

typedef struct{
	
	// buffers

	//char* output;
	char* buffer;
	char* prompt;

	// options and values

	int cursor;
	//int has_output;
	int exit;
	int refresh;
	int tabstate;
	int insert;

	// callbacks
	
	cmdl_putchar_t putchar;
	cmdl_getchar_t getchar;
	cmdl_printf_t printf;

}cmd_line;

typedef struct{
	int argc;
	char **argv;
}cmdl_param_list;

void cmdl_setputchar(cmd_line* cmd, cmdl_putchar_t in);
void cmdl_setgetchar(cmd_line* cmd, cmdl_getchar_t in);
void cmdl_setprintf(cmd_line* cmd, cmdl_printf_t in);

//void cmdl_builtin_help(cmd_line* cmd, char* command);

void cmdl_parsechar(cmd_line* cmd, char in);

void cmdl_addreplace(cmd_line* cmd, char in);
void cmdl_addinsert(cmd_line* cmd, char in);
void cmdl_enterloop(cmd_line* cmd);
void cmdl_exec(cmd_line* cmd);
void cmdl_setexit(cmd_line* cmd, int exit);
int cmdl_getexit(cmd_line* cmd);
void cmdl_clearoutput(cmd_line* cmd);
void cmdl_clearbuffer(cmd_line* cmd);
int cmdl_printf(cmd_line* cmd, const char *format, ...);
char* cmdl_getoutput(cmd_line* cmd);
//void cmdl_addoutput_str(cmd_line* cmd, char output[CMDL_OUTPUT_SIZE]);
void cmdl_addstr(cmd_line* cmd, char* str);
int cmdl_movecursor(cmd_line* cmd, int direction);
char* cmdl_getbuffer(cmd_line* cmd);
void cmdl_addchar(cmd_line* cmd, char in);
int cmdl_check(cmd_line* cmd);
void cmdl_del(cmd_line* cmd);
cmd_line* cmdl_create();
void cmdl_free(cmd_line* cmd);
char *cmdl_getprompt(cmd_line* cmd);
void cmdl_setpropmt(cmd_line* cmd, char prompt[CMDL_PROMPT_SIZE]);
cmdl_param_list* cmdl_getparams(const char* command);
void cmdl_tabcomplete(cmd_line *cmd);

#endif

