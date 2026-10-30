#!/bin/sh
tail -n +3 "$0" | cc -x c -o /tmp/mp-build - && exec /tmp/mp-build "$@"; exit 1
// build.c - build, install and uninstall mp.   ./build.c [build|install|uninstall]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sh(const char *fmt, const char *a) {
	char cmd[2048];
	snprintf(cmd, sizeof cmd, fmt, a);
	printf("\033[36m$ %s\033[0m\n", cmd);
	return system(cmd);
}

int main(int argc, char **argv) {
	const char *act = argc > 1 ? argv[1] : "build";
	const char *home = getenv("HOME");
	char bin[512];
	snprintf(bin, sizeof bin, "%s/.local/bin", getenv("PREFIX") ? getenv("PREFIX") : home);
	if (getenv("PREFIX")) snprintf(bin, sizeof bin, "%s/bin", getenv("PREFIX"));

	if (!strcmp(act, "build")) return sh("cc -O2 -Wall -o mp %s", "src/*.c");
	if (!strcmp(act, "install")) return sh("mkdir -p '%s'", bin) || sh("cc -O2 -Wall -o mp %s", "src/*.c") ||
	                                    (snprintf(bin + strlen(bin), 16, "/mp"), sh("install -m755 mp '%s'", bin));
	if (!strcmp(act, "uninstall")) return (snprintf(bin + strlen(bin), 16, "/mp"), sh("rm -f '%s'", bin));
	fprintf(stderr, "usage: build.c [build|install|uninstall]\n");
	return 1;
}
