
#include "log_api.h"
#include <unistd.h>
#include <stdio.h>


int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <log_message>\n", argv[0]);
		return 1;
	}

	// Log the message with INFO level
	while (1) {
		LogInfo("Log message: %s", argv[1]);
		sleep(5);
	}

	return 0;
}