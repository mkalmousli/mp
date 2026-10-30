// store.c - profile storage (~/.mp/SERVICE/NAME) and isolated execution
#include "mp.h"

Prof ps[512];
int np;
char root[512];

int valid(const char *s) {
	if (!*s || s[0] == '.' || strlen(s) > 60) return 0;
	for (; *s; s++)
		if (!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.", *s)) return 0;
	return 1;
}

void mkpath(const char *p) {
	char b[1024];
	snprintf(b, sizeof b, "%s", p);
	for (char *c = b + 1; *c; c++)
		if (*c == '/') { *c = 0; mkdir(b, 0700); *c = '/'; }
	mkdir(b, 0700);
}

static int visible(const struct dirent *d) { return d->d_name[0] != '.'; }

void load(void) {
	struct dirent **s, **n;
	np = 0;
	int ns = scandir(root, &s, visible, alphasort);
	for (int i = 0; i < ns; i++) {
		char d[1024];
		snprintf(d, sizeof d, "%s/%s", root, s[i]->d_name);
		int nn = scandir(d, &n, visible, alphasort);
		for (int j = 0; j < nn && np < 512; j++, np++) {
			snprintf(ps[np].svc, 64, "%s", s[i]->d_name);
			snprintf(ps[np].name, 64, "%s", n[j]->d_name);
		}
	}
}

void del(const Prof *p) {
	char cmd[1200];
	snprintf(cmd, sizeof cmd, "rm -rf '%s/%s/%s'", root, p->svc, p->name);
	if (valid(p->svc) && valid(p->name) && system(cmd)) perror("rm");
}

int rename_prof(const Prof *p, const char *to) {
	char a[1100], b[1100];
	if (!valid(to)) return -1;
	for (int i = 0; i < np; i++)
		if (!strcmp(ps[i].name, to)) return -1;
	snprintf(a, sizeof a, "%s/%s/%s", root, p->svc, p->name);
	snprintf(b, sizeof b, "%s/%s/%s", root, p->svc, to);
	return rename(a, b);
}

/* ---------- run a command inside a profile ---------- */
void pdir(char *out, const char *svc, const char *name) { snprintf(out, 1024, "%s/%s/%s", root, svc, name); }

/* Ask whether a new profile gets the default config: enter/y yes, n/esc no, ctrl-c aborts. */
static int offer_defaults(const char *svc, const char *name) {
	int c;
	if (!isatty(0) || !isatty(1)) return 0;
	raw(1);
	printf(CYN "\nuse the default config for new %s profile %s? " RST DIM "(status line, change later with: mp defaults %s) " RST "[Y/n] ", svc, name, name);
	fflush(stdout);
	do c = key(); while (c <= 0 || c > 127 || !strchr("yYnN\r\n\033\003", c));
	raw(0);
	if (c == 3) exit(130);
	return c == 'y' || c == 'Y' || c == '\n' || c == '\r';
}

void run(const char *svc, const char *name, char **argv) {
	char dir[1024];
	struct stat st;
	if (!valid(svc) || !valid(name)) { fprintf(stderr, RED "invalid service/profile name\n" RST); exit(1); }
	pdir(dir, svc, name);
	int fresh = stat(dir, &st) != 0;
	int want = fresh && has_defaults(svc) && offer_defaults(svc, name);
	mkpath(dir);
	if (want) set_defaults(svc, name, 1);
	apply_env(svc, dir, 0);
	setenv("MP_PROFILE", name, 1);
	if (defaults_on(svc, name)) {
		int n = 0;
		while (argv[n]) n++;
		char **av = malloc((n + 3) * sizeof *av);
		av[0] = argv[0];
		av[1] = "--settings";
		av[2] = (char *)defaults_json();
		memcpy(av + 3, argv + 1, n * sizeof *av);
		argv = av;
	}
	execvp(svc, argv);
	fprintf(stderr, RED "mp: cannot run '%s'\n" RST, svc);
	exit(127);
}
