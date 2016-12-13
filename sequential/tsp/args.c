/* For copyright information, see olden_v1.0/COPYRIGHT */

#include <stdlib.h>

int flag = 0;

int dealwithargs(int argc, char *argv[]) {
	int num = 15;

	if (argc > 2)
		flag = atoi(argv[2]);

	if (argc > 1)
		num = atoi(argv[1]);

	return num;
}
