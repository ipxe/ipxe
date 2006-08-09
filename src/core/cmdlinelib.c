#include "cmdlinelib.h"
#include <console.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>

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
        char string[CMDL_OUTPUT_SIZE];
        va_list ap;

        va_start(ap, format);
        ret = vsprintf(string, format, ap);
        cmdl_addoutput_str(cmd, string);
        va_end(ap);
        return ret;
}


void cmdl_addoutput_str(cmd_line* cmd, char output[CMDL_OUTPUT_SIZE])
{
	if(cmdl_check(cmd) && output != NULL){
		if(!cmd->has_output){
			cmdl_clearoutput(cmd);
		}
		strncat(cmd->output, output, CMDL_OUTPUT_SIZE);
		cmd->has_output = 1;
	}
}

char* cmdl_getoutput(cmd_line* cmd)
{
	if(cmdl_check(cmd) && cmd->has_output){
		cmd->has_output = 0;
		return cmd->output;
	}else{
		return "";
	}
}

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

void cmdl_addchar(cmd_line* cmd, char in)
{
	if(in >= 32){
		if(cmdl_check(cmd)){
			cmd->buffer[cmd->cursor] = in;
			cmdl_movecursor(cmd, CMDL_RIGHT);
		}
	}else{
		switch(in){
			case 0x08: /* Backspace */

				cmdl_delat(cmd, cmd->cursor);
				cmdl_movecursor(cmd, CMDL_LEFT);
				break;
			case 0x0a:
			case 0x0d: /* Enter */
				cmdl_exec(cmd);
				break;
		}
	}
}

void cmdl_exec(cmd_line* cmd)
{
	char* command;
	cmdl_printf(cmd, "%s %s\n", cmd->prompt, cmd->buffer);

	command = cmdl_parse_getcmd(cmd);
	if(strlen(command) != 0){
		if(strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0){
			cmdl_setexit(cmd, 1);
		}else if(strcmp(command, "help") == 0){
			cmdl_printf(cmd, "Don't panic\n");
		}else{
			cmdl_printf(cmd, "%s: unknown command\n", command);
		}
	}
	free(command);
	
	cmdl_clearbuffer(cmd);
}

char* cmdl_parse_getcmd(cmd_line* cmd){
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
}

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

void cmdl_clearoutput(cmd_line* cmd)
{
	if(cmdl_check(cmd)){
		int i;
		for(i=0; i < CMDL_OUTPUT_SIZE; i++){
			cmd->output[i] = 0;
		}
	}
}

void cmdl_movecursor(cmd_line* cmd, int direction)
{
	if(cmdl_check(cmd)){
		switch(direction){
			case CMDL_LEFT:
				if(cmd->cursor > 0){
					cmd->cursor--;
				}
				break;
			case CMDL_RIGHT:
				if(cmd->cursor < CMDL_BUFFER_SIZE - 1){
					cmd->cursor++;
				}
				break;
		}
	}
}

void cmdl_delat(cmd_line* cmd, int at)
{
	if(cmdl_check(cmd) && at < CMDL_BUFFER_SIZE - 1 && at >= 0){
		int i;
		for(i = at; i < CMDL_BUFFER_SIZE - 1; i++){
			cmd->buffer[i] = cmd->buffer[i + 1];
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
	
	// Initiate the command line
	
	this = (cmd_line*)malloc(sizeof(cmd_line));
	
	if(this == NULL){
		return NULL;
	}
	

	/* Allocate output buffer */
	
	this->output = (char*)malloc(CMDL_OUTPUT_SIZE);
	if(this->output == NULL){
		free(this);
		return NULL;
	}
	
	for(i = 0; i < CMDL_OUTPUT_SIZE; i++){
		this->output[i] = 0;
	}

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
	
	/* Initiate cursor position */
	
	this->cursor = 0;
	this->has_output = 0;
	this->exit = 0;
	
	return this;
}

void cmdl_free(cmd_line* cmd)
{
	free(cmd);
}

