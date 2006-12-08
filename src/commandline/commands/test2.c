#include <vsprintf.h>
#include <gpxe/command.h>

void test2_req(){}

static int cmd_test2_exec ( int argc, char **argv ) {
	int i;

	printf("Hello, world!\nI got the following arguments passed to me: \n");
	for(i = 0; i < argc; i++){
		printf("%d: \"%s\"\n", i, argv[i]);
	}
	return 0;
}

struct command test2_command __command = {
	.name = "test2",
	.exec = cmd_test2_exec,
};

