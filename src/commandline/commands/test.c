#include "command.h"
#include "console.h"

void test_req(){}

static int cmd_test_exec ( int argc, char **argv ) {
	int i;

	printf("Hello, world!\nI got the following arguments passed to me: \n");
	for(i = 0; i < argc; i++){
		printf("%d: \"%s\"\n", i, argv[i]);
	}
	return 0;
}

struct command test_command __command = {
	.name = "test",
	.usage = "A test command\nIt does nothing at all\n\nExample:\n\ttest",
	.desc = "Does nothing",
	.exec = cmd_test_exec,
};

