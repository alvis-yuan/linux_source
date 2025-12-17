#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arg.h"

/*
 * Version
 */
#define VERSION_MAJOR (0U)
#define VERSION_MINOR (0U)
#define VERSION_PATCH (1U)

/*
 * Short options
 */
#define OPTSTR ":hv"

/*
 * Long options
 */
enum {
	LOPT_HELP = 1,
	LOPT_VERSION,
	LOPT_DAEMON
};

static const struct option LONGOPTS[] = {
	{"help",    0, NULL, LOPT_HELP   },
	{"version", 0, NULL, LOPT_VERSION},
	{"daemon",  0, NULL, LOPT_DAEMON },
	{NULL,      0, NULL, 0           }
};

static const char *USAGE =
	"\nUsage: %s [OPTION]"
	"\n"
	"\nOPTION"
	"\n  -h, --help       Show this information and exit."
	"\n  -v, --version    Show the version and exit."
	"\n      --daemon     Run as daemon."
;

#define ERROR(...) \
do { \
	printf(__VA_ARGS__); \
	exit(1); \
} while (0)

#define ERROR_USAGE(...) \
do { \
	printf(__VA_ARGS__); \
	show_usage(argv[0]); \
} while (0)

arg_param_t arg_param = {
	.daemon = false
};

static void show_usage(const char *name)
{
	char *str;

	/* Get the program name */
	str = strrchr(name, '/');
	printf(USAGE, (str) ? (str + 1) : name);
	printf("\n\n");
	exit(1);
}

static void show_version(void)
{
	printf("Version %u.%u.%u\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	exit(1);
}

void arg_parse(int argc, char *argv[])
{
	int c;

	opterr = 0; /* Prevent error messages for unrecognized options */

	while ((c = getopt_long(argc, argv, OPTSTR, LONGOPTS, NULL)) != -1) {
		switch (c) {
		case 'h':
		case LOPT_HELP:
			show_usage(argv[0]);
			break;

		case 'v':
		case LOPT_VERSION:
			show_version();
			break;

		case LOPT_DAEMON:
			arg_param.daemon = true;
			break;

		case ':':
			ERROR_USAGE("Missing argument for option \"%s\".\n", argv[optind - 1]);
		case '?':
			ERROR_USAGE("Unrecognized option \"%s\".\n", argv[optind - 1]);
		default:
			ERROR("Unexpected code %d returned by getopt_long().\n\n", c);
		}
	}

	if (optind < argc)
		ERROR_USAGE("Unrecognized argument \"%s\".\n", argv[optind]);
}
