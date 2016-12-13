/* For copyright information, see olden_v1.0/COPYRIGHT */

#include <stdlib.h>
#include "tree.h"

int level = 25, iters = 100;

int dealwithargs(int argc, char *argv[]) {

	if (argc > 2)
		iters = atoi(argv[2]);

	if (argc > 1)
		level = atoi(argv[1]);

	return level;
}
