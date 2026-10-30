// tui.c - interactive terminal UI
#include "mp.h"

static void draw(int sel) {
	printf("\033[2J\033[H" BLD CYN "  mp " RST DIM "multi profile\n\n" RST);
	for (int i = 0; i < np; i++)
		printf(i == sel ? REV " > %-16s %-16s" RST "\n" : "   " BLD "%-16s" RST " " YEL "%s" RST "\n", ps[i].name, ps[i].svc);
	if (!np) printf(DIM "   no profiles - press n to create one\n" RST);
	if (np) { char d[1024]; pdir(d, ps[sel].svc, ps[sel].name); printf("\n" DIM "  files: %s\n" RST, d); }
	if (np && has_defaults(ps[sel].svc)) printf(DIM "  default config: %s\n" RST, defaults_on(ps[sel].svc, ps[sel].name) ? "on" : "off");
	printf("\n" DIM "  up/down move  " GRN "enter" DIM " run  " GRN "n" DIM " new  " CYN "r" DIM " rename  " YEL "c" DIM " default config  " RED "d" DIM " delete  q quit\n" RST);
	fflush(stdout);
}

void tui(void) {
	int sel = 0, c;
	char a[64], b[64];
	raw(1);
	for (load();; load()) {
		if (sel >= np) sel = np ? np - 1 : 0;
		draw(sel);
		c = key();
		if (c == 'q' || c == 3) break;
		if ((c == 'k' || c == KUP) && sel > 0) sel--;
		if ((c == 'j' || c == KDN) && sel < np - 1) sel++;
		if (c == 'd' && np) {
			printf("\n" RED "  delete %s? [y/N] " RST, ps[sel].name);
			fflush(stdout);
			if (key() == 'y') del(&ps[sel]);
		}
		if (c == 'c' && np && has_defaults(ps[sel].svc)) set_defaults(ps[sel].svc, ps[sel].name, !defaults_on(ps[sel].svc, ps[sel].name));
		if (c == 'r' && np) {
			raw(0);
			printf(CYN "new name for %s: " RST, ps[sel].name);
			if (scanf("%63s", b) == 1) rename_prof(&ps[sel], b);
			raw(1);
		}
		if (c == '\n' && np) {
			raw(0);
			char *av[] = {ps[sel].svc, NULL};
			run(ps[sel].svc, ps[sel].name, av);
		}
		if (c == 'n' && pick(a)) {
			raw(0);
			printf(CYN "profile name for %s: " RST, a);
			if (scanf("%63s", b) == 1 && valid(a) && valid(b)) { char *av[] = {a, NULL}; run(a, b, av); }
			raw(1);
		}
	}
	raw(0);
}
