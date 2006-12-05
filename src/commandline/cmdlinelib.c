#include "cmdlinelib.h"
#include "command.h"
#include <gpxe/tables.h>
#include <console.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>

static struct command cmd_start[0] __table_start ( commands );
static struct command cmd_end[0] __table_end ( commands );

void cmdl_setgetchar(cmd_line* cmd, cmdl_getchar_t in)
{
	cmd->getchar = in;
}

void cmdl_setputchar(cmd_line* cmd, cmdl_putchar_t in)
{
	cmd->putchar = in;
}

void cmdl_setprintf(cmd_line* cmd, cmdl_printf_t in)
{
	cmd->printf = in;
}
      
int cmdl_getexit(cmd_line* cmd)
{
	if(cmdl_check(cmd) && !cmd->exit){
		return 0;
	}else{
		return 1;
	}

}

void cmdl_setexit(cmd_line* cmd, int exit)
{
	if(cmdl_check(cmd)){
		cmd->exit = exit;
	}
}

int cmdl_printf(cmd_line* cmd, const char *format, ...)
{
	int ret;
        char string[CMDL_BUFFER_SIZE];
        va_list ap;

        va_start(ap, format);
        ret = vsprintf(string, format, ap);
        cmdl_addstr(cmd, string);
        va_end(ap);
        return ret;
}

void cmdl_addstr(cmd_line* cmd, char* str)
{
	unsigned int i;
	for(i = 0; i < strlen(str); i++){
		cmdl_addchar(cmd, str[i]);
	}
}

/*void cmdl_addoutput_str(cmd_line* cmd, char output[CMDL_OUTPUT_SIZE])
{
	if(cmdl_check(cmd) && output != NULL){
		if(!cmd->has_output){
			cmdl_clearoutput(cmd);
		}
		strncat(cmd->output, output, CMDL_OUTPUT_SIZE);
		cmd->has_output = 1;
	}
}*/

/*char* cmdl_getoutput(cmd_line* cmd)
{
	if(cmdl_check(cmd) && cmd->has_output){
		cmd->has_output = 0;
		return cmd->output;
	}else{
		return "";
	}
}*/

void cmdl_setpropmt(cmd_line* cmd, char prompt[CMDL_PROMPT_SIZE])
{
	if(cmdl_check(cmd) && prompt != NULL){
		strncat(cmd->prompt, prompt, CMDL_PROMPT_SIZE);
	}
}

char *cmdl_getprompt(cmd_line* cmd)
{
	if(cmdl_check(cmd)){
		return cmd->prompt;
	}else{
		return "";
	}
}

char* cmdl_getbuffer(cmd_line* cmd){
	if(cmdl_check(cmd)){
		return cmd->buffer;
	}else{
		return "";
	}
}

void cmdl_enterloop(cmd_line* cmd)
{
	while(!cmdl_getexit(cmd)){
		if(cmd->refresh){
			cmd->printf("%s %s", cmd->prompt, cmd->buffer);
			cmd->refresh = 0;
		}
//		cmd->printf("Got %d\n", cmd->getchar());
		cmdl_parsechar(cmd, cmd->getchar());
	}
}

void cmdl_addreplace(cmd_line* cmd, char in)
{
	if(cmd->cursor < CMDL_BUFFER_SIZE - 2){
		cmd->buffer[cmd->cursor] = in;
		cmd->cursor++;
		cmd->putchar((int)in);
	}
}

void cmdl_addinsert(cmd_line* cmd, char in)
{
	int i;
	int to;
	if(cmd->cursor < CMDL_BUFFER_SIZE - 2 && cmd->cursor >= 0){
		if(strlen(cmd->buffer) < CMDL_BUFFER_SIZE - 2){
			to = strlen(cmd->buffer);
		}else{
			to = CMDL_BUFFER_SIZE - 2;
		}
			for(i=to; i > cmd->cursor; i--){
				cmd->buffer[i] = cmd->buffer[i-1];
			}
			cmd->buffer[cmd->cursor] = in;

			for(i=cmd->cursor; i < to; i++){
				cmd->putchar(cmd->buffer[i]);
			}
			
			for(i=cmd->cursor; i < to - 1; i++){
				cmd->putchar(CMDLK_BS);
			}
			cmd->cursor++;
			//cmdl_movecursor(cmd, CMDL_RIGHT);
	}
}

void cmdl_addchar(cmd_line* cmd, char in){
	if(cmd->insert){
		cmdl_addinsert(cmd, in);
	}else{
		cmdl_addreplace(cmd, in);
	}
}

void cmdl_parsechar(cmd_line* cmd, char in)
{
	if(cmdl_check(cmd)){
		if(in >= 32){
			cmdl_addchar(cmd, in);
		}else{
			switch(in){
				case CMDLK_BS:
					if(cmdl_movecursor(cmd, CMDL_LEFT)){
						cmdl_del(cmd);
					}
					break;

				case CMDLK_RETURN:
					cmd->putchar('\n');
					cmdl_exec(cmd);
					cmd->refresh = 1;
					break;

				case CMDLK_BW:
					cmdl_movecursor(cmd, CMDL_LEFT);
					break;

				case CMDLK_FW:
					//cmdl_movecursor(cmd, CMDL_RIGHT);
					break;
				
				case CMDLK_TAB:
					cmdl_tabcomplete(cmd);
					break;

			}
		}
	}
}

void cmdl_tabcomplete(cmd_line *cmd)
{
	struct command *ccmd;
	int count=0;
	char* result[CMDL_MAX_TAB_COMPLETE_RESULT];

	for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
		if(!strncmp(ccmd->name, cmd->buffer, strlen(cmd->buffer))){
			if(count <= CMDL_MAX_TAB_COMPLETE_RESULT){
				result[count++] = (char*)(ccmd->name);
			}
		}
	}
	

	if( count == 1 ){
		cmdl_addstr(cmd, (char*)(result[0] + strlen(cmd->buffer)));
		cmd->tabstate = 0;
		cmdl_addchar(cmd, ' ');
	} else if( count > 1 ) {
		int i, i2, minlen=CMDL_BUFFER_SIZE, same=1;
		char last;

		for(i = 0; i < count; i ++) {
			if(minlen > (int)strlen( result[i] ) ){
				minlen = strlen(result[i]);
			}
		
		}
		if((int)strlen(cmd->buffer) < minlen){
			for(i = strlen(cmd->buffer); i < minlen; i++){
				last = result[0][i];
				for(i2 = 1; i2 < count; i2 ++) {
					if(result[i2][i] != last){
						same = 0;
						break;
					}
				}
				if(same){
					cmdl_addchar(cmd, last);
				}
				
			}
		}
		cmd->tabstate++;
	}
	
	if(count > 1 && cmd->tabstate > 1){
		int i;
		cmd->tabstate = 0;
		cmd->refresh = 1;
		cmd->putchar('\n');
		for(i = 0; i < count; i ++){
			cmd->printf("%s\t", result[i]);
		}
		cmd->putchar('\n');
	}

	

}


void cmdl_exec(cmd_line* cmd)
{
	cmdl_param_list* params;
	int unknown=1;
	struct command *ccmd;

	params = cmdl_getparams(cmd->buffer);
	
	if(params == NULL){
		cmdl_clearbuffer(cmd);
		return;
	}

	if(params->argc > 0){
		if(!strcmp(params->argv[0], "exit") || !strcmp(params->argv[0], "quit")){
			cmdl_setexit(cmd, 1);
/*		}else if(!strcmp(params->argv[0], "help")){
			if(params->argc > 1){
				cmdl_builtin_help(cmd, params->argv[1]);
			}else{
				cmdl_builtin_help(cmd, "");
			}*/
		}else{
			for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
				if(!strcmp(ccmd->name, params->argv[0])){
					unknown = 0;
					ccmd->exec(params->argc, params->argv);
					break;
				}
			}
			if(unknown){
				cmd->printf("%s: unknown command\n", params->argv[0]);
			}
		}
	}

	free(params);	
	cmdl_clearbuffer(cmd);
}

/*void cmdl_builtin_help(cmd_line* cmd, char* command){
	struct command *ccmd;
	int unknown = 1;
	if(strcmp(command, "") == 0){
		cmd->printf("Built in commands:\n\n\thelp\t\tCommand usage help (\"help help\" for more info)\n\texit, quit\t\tExit the command line and boot\n\nCompiled in commands:\n\n");

		for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
			cmd->printf ("\t%s\t\t%s\n", ccmd->name, ccmd->desc );
		}
	}else{
		if(!strcmp(command, "help")){
			cmd->printf("help - The help command\n\nUsage: help <command>\n\n\tExample:\n\t\thelp help\n");
		}else if(!strcmp(command, "exit") || !strcmp(command, "quit")){
			cmd->printf("exit, quit - The quit command\n\nUsage:\nquit or exit\n\n\tExample:\n\t\texit\n");
		}else{
			for ( ccmd = cmd_start ; ccmd < cmd_end ; ccmd++ ) {
				if(!strcmp(ccmd->name, command)){
					unknown = 0;
					cmd->printf ("\t%s - %s\n\nUsage:\n%s\n", ccmd->name, ccmd->desc, ccmd->usage );
					break;
				}
				if(unknown){
					cmd->printf("\"%s\" isn't compiled in (does it exist?).\n", command);
				}
			}
		}
		
	}
}*/

cmdl_param_list* cmdl_getparams(const char* command){
	cmdl_param_list* this;
	char *result = NULL;
	int count=0;
	char *command2;
	
	this = (cmdl_param_list*)malloc(sizeof(cmdl_param_list));
	
	if(this == NULL){
		return NULL;
	}

	command2 = malloc(strlen(command) + 1);
	
	this->argc=0;

	strcpy(command2, command);
	result = strtok(command2, " ");
	
	while( result != NULL ) {
		this->argc++;
		result = strtok( NULL, " ");
	}
	
	this->argv = (char**)malloc(sizeof(char*) * this->argc);
	if(this->argv == NULL){
		free(this);
		return NULL;
	}
	
	
	strcpy(command2, command);
	result = strtok(command2, " ");
	
	while( result != NULL && this->argc > count) {
		this->argv[count] = (char*)malloc(sizeof(char) * (strlen(result) + 1));
		if(this->argv[count] == NULL){
			free(this);
			return NULL;
		}
		strcpy(this->argv[count], result);
		count++;
		result = strtok( NULL, " ");
	}   
	free(command2);	
	return this;
}

/*char* cmdl_parse_getcmd(cmd_line* cmd){
	int i;
	char* ret;
	ret = (char*)malloc(1);
	ret[0] = 0;

	for(i=0; i < CMDL_BUFFER_SIZE - 1; i++){
		if(cmd->buffer[i + 1] == ' ' || cmd->buffer[i + 1] == '\0'){
			free(ret);
			ret = (char*)malloc(i+1);
			strncat(ret, cmd->buffer, i+1);
			break;
		}
	}
	return ret;
}*/

void cmdl_clearbuffer(cmd_line* cmd)
{
	if(cmdl_check(cmd)){
		int i;
		cmd->cursor = 0;
		for(i=0; i < CMDL_BUFFER_SIZE; i++){
			cmd->buffer[i] = 0;
		}
	}
}

/*void cmdl_clearoutput(cmd_line* cmd)
{
	if(cmdl_check(cmd)){
		int i;
		for(i=0; i < CMDL_OUTPUT_SIZE; i++){
			cmd->output[i] = 0;
		}
	}
}*/

int cmdl_movecursor(cmd_line* cmd, int direction)
{
	if(cmdl_check(cmd)){
		switch(direction){
			case CMDL_LEFT:
				if(cmd->cursor > 0){
					cmd->cursor--;
					cmd->putchar(CMDLK_BS);
				}else{
					return 0;
				}
				break;
			case CMDL_RIGHT:
				if(cmd->cursor < CMDL_BUFFER_SIZE - 2){
					cmd->cursor++;
					cmd->putchar(' ');
				}else{
					return 0;
				}
				break;
		}
	}
	return 1;
}

void cmdl_del(cmd_line* cmd)
{
	if(cmdl_check(cmd) && cmd->cursor < CMDL_BUFFER_SIZE - 2 && cmd->cursor >= 0){
		int i;
		for(i = cmd->cursor; i < (int)strlen(cmd->buffer); i++){
			cmd->buffer[i] = cmd->buffer[i + 1];
			if(!cmd->buffer[i]){
				cmd->putchar(' ');
			}else{
				cmd->putchar(cmd->buffer[i]);
			}
		}
		for(i = cmd->cursor; i < (int)strlen(cmd->buffer) + 1; i++){
			cmd->putchar(CMDLK_BS);
		}
	}
}


int cmdl_check(cmd_line* cmd)
{
	if(
		cmd != NULL && 
		cmd->buffer != NULL &&
		cmd->prompt != NULL &&
		cmd->cursor >= 0 && 
		cmd->cursor < CMDL_BUFFER_SIZE - 1 &&
		cmd->buffer[CMDL_BUFFER_SIZE - 1] == 0 &&
		cmd->prompt[CMDL_PROMPT_SIZE - 1] == 0
	){
		return 1;
	}else{
		return 0;
	}
}

cmd_line* cmdl_create()
{
	cmd_line* this;
	int i;
	
	/* Initiate the command line */
	
	this = (cmd_line*)malloc(sizeof(cmd_line));
	
	if(this == NULL){
		return NULL;
	}
	

	/* Allocate output buffer */
	
	/*this->output = (char*)malloc(CMDL_OUTPUT_SIZE);
	if(this->output == NULL){
		free(this);
		return NULL;
	}*/
	
/*	for(i = 0; i < CMDL_OUTPUT_SIZE; i++){
		this->output[i] = 0;
	}*/

	/* Allocate command line buffer */
	
	this->buffer = (char*)malloc(CMDL_BUFFER_SIZE);
	if(this->buffer == NULL){
		free(this);
		return NULL;
	}
	
	for(i = 0; i < CMDL_BUFFER_SIZE; i++){
		this->buffer[i] = 0;
	}
	
	/* Allocate prompt buffer */
	
	this->prompt = (char*)malloc(CMDL_PROMPT_SIZE);
	if(this->prompt == NULL){
		free(this);
		return NULL;
	}
	
	for(i = 0; i < CMDL_PROMPT_SIZE; i++){
		this->prompt[i] = 0;
	}
	
	/* Initiate cursor position etc.*/
	
	this->cursor = 0;
	//this->has_output = 0;
	this->exit = 0;
	this->refresh = 1;
	this->tabstate = 0;
	this->insert = 0;

	/* set callbacks to NULL */

	this->getchar = NULL;
	this->putchar = NULL;
	this->printf = NULL;

	/* List the commands */

	struct command *cmd;

	printf ( "Available commands: ");
	for ( cmd = cmd_start ; cmd < cmd_end ; cmd++ ) {
		printf("%s ", cmd->name);
	}
	printf("exit\n\n");

	return this;
}

void cmdl_free(cmd_line* cmd)
{
	free(cmd);
}

