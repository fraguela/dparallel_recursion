/* For copyright information, see olden_v1.0/COPYRIGHT */

#include "tsp.h"
#include <stdio.h>
#include <sys/time.h>

#define chatting printf

extern int flag;

void print_tree(Tree t) {
	Tree left, right;
	double x, y;

	if (!t)
		return;
	x = t->x;
	y = t->y;
	chatting("x=%f,y=%f\n", x, y);
	left = t->left;
	right = t->right;
	print_tree(left);
	print_tree(right);
}

void print_list(Tree t) {
	Tree tmp;
	double x, y;

	if (!t)
		return;
	x = t->x;
	y = t->y;
	chatting("%f %f\n", x, y);
	for (tmp = t->next; tmp != t; tmp = tmp->next) {
		x = tmp->x;
		y = tmp->y;
		chatting("%f %f\n", x, y);
	}
}

int main(int argc, char *argv[]) {
	Tree t;
	int num;
	struct timeval tt, t0, t1;

	extern int dealwithargs();

	num = dealwithargs(argc, argv);

	chatting("Building tree of size %d\n", num);
	gettimeofday(&t0, NULL);
	t = build_tree(num, 0, 0.0, 1.0, 0.0, 1.0);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &tt);
	printf("  alloc time: %f\n", (tt.tv_sec * 1000000 + tt.tv_usec) / 1000000.0);
	if (!flag)
		chatting("Past build\n");
	if (flag)
		chatting("newgraph\n");
	if (flag)
		chatting("newcurve pts\n");

	gettimeofday(&t0, NULL);
	tsp(t, 150);

	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &tt);
	printf("compute time: %f\n", (tt.tv_sec * 1000000 + tt.tv_usec) / 1000000.0);

	if (flag)
		print_list(t);
	if (flag)
		chatting("linetype solid\n");
  return 0;
}
