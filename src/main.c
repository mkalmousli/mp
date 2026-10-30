// main.c - cli entry point: mp NAME [args...]
#include "mp.h"

static void usage(void) {
	printf(BLD CYN "mp" RST " - Multi Profile\n\n"
	       "  " GRN "mp" RST " NAME [args]          run profile NAME (its service is remembered)\n"
	       "  " GRN "mp" RST " SERVICE NAME [args]  create profile NAME for SERVICE and run it\n"
	       "  " GRN "mp" RST "                      open the interactive TUI\n"
	       "  " GRN "mp ls" RST "                   list profiles\n"
	       "  " GRN "mp where" RST " [NAME]        show where files are stored\n"
	       "  " GRN "mp defaults" RST " NAME [on|off]  toggle the default config (claude: status line)\n"
	       "  " GRN "mp mv" RST " OLD NEW           rename a profile\n"
	       "  " GRN "mp rm" RST " NAME [-y]         delete a profile (asks to confirm)\n\n"
	       DIM "  e.g. mp wrangler hopwire   (once)   then   mp hopwire deploy\n" RST);
}

static void list(void) {
	for (int i = 0; i < np; i++) printf(BLD "%-16s" RST DIM " -> " RST YEL "%s\n" RST, ps[i].name, ps[i].svc);
	if (!np) printf(DIM "no profiles yet\n" RST);
}

static void where(const Prof *p) {
	char d[1024];
	printf(BLD "store" RST "  %s " DIM "(override with MP_DIR)\n" RST, root);
	if (!p) return;
	pdir(d, p->svc, p->name);
	printf(BLD "%s" RST " -> " YEL "%s" RST "\n" BLD "files" RST "  %s\n" BLD "env" RST "\n", p->name, p->svc, d);
	apply_env(p->svc, d, 1);
	if (has_defaults(p->svc)) printf(BLD "defaults" RST " %s\n", defaults_on(p->svc, p->name) ? "on" : "off");
}

static Prof *find(const char *name) {
	for (int i = 0; i < np; i++)
		if (!strcmp(ps[i].name, name)) return &ps[i];
	return NULL;
}

/* Subcommand words never fall through to "SERVICE NAME", which would create a junk profile. */
static int is_cmd(const char *s) {
	static const char *cmds[] = {"ls", "where", "mv", "rm", "defaults", "statusline", NULL};
	for (int i = 0; cmds[i]; i++)
		if (!strcmp(cmds[i], s)) return 1;
	return 0;
}

static int bad_cmd(int argc, char **argv) {
	if (argc >= 3 && !find(argv[2])) fprintf(stderr, RED "no such profile: %s\n" RST, argv[2]);
	else usage();
	return 1;
}

static int defaults(Prof *p, int argc, char **argv) {
	if (!has_defaults(p->svc)) { fprintf(stderr, RED "no default config for %s\n" RST, p->svc); return 1; }
	if (argc == 4 && strcmp(argv[3], "on") && strcmp(argv[3], "off")) { usage(); return 1; }
	int on = argc == 4 ? !strcmp(argv[3], "on") : !defaults_on(p->svc, p->name);
	set_defaults(p->svc, p->name, on);
	printf(BLD "%s" RST " default config %s " DIM "(applies from the next launch)\n" RST, p->name, on ? GRN "on" RST : RED "off" RST);
	return 0;
}

int main(int argc, char **argv) {
	if (argc == 2 && !strcmp(argv[1], "statusline")) return statusline();
	if (getenv("MP_DIR")) snprintf(root, sizeof root, "%s", getenv("MP_DIR"));
	else snprintf(root, sizeof root, "%s/.mp", getenv("HOME") ? getenv("HOME") : ".");
	mkpath(root);
	load();
	Prof *p = argc > 1 ? find(argv[1]) : NULL, *q = argc > 2 ? find(argv[2]) : NULL;
	if (argc == 1) tui();
	else if (!strcmp(argv[1], "ls")) list();
	else if (!strcmp(argv[1], "where") && (argc == 2 || (argc == 3 && q))) where(q);
	else if ((argc == 3 || argc == 4) && !strcmp(argv[1], "defaults") && q) return defaults(q, argc, argv);
	else if (argc == 4 && !strcmp(argv[1], "mv") && q) {
		if (rename_prof(q, argv[3])) { fprintf(stderr, RED "cannot rename %s to %s (invalid or taken)\n" RST, q->name, argv[3]); return 1; }
	}
	else if ((argc == 3 || (argc == 4 && !strcmp(argv[3], "-y"))) && !strcmp(argv[1], "rm") && q) {
		if (argc == 3) { printf(RED "delete %s? [y/N] " RST, q->name); if (getchar() != 'y') return 1; }
		del(q);
	}
	else if (p) { char svc[64]; snprintf(svc, 64, "%s", p->svc); argv[1] = svc; run(svc, p->name, argv + 1); }
	else if (is_cmd(argv[1])) return bad_cmd(argc, argv);
	else if (argc >= 3) { char *name = argv[2]; argv[2] = argv[1]; run(argv[1], name, argv + 2); }
	else usage();
	return 0;
}
