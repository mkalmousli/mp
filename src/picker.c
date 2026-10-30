// picker.c - searchable service dropdown + raw keyboard input
#include "mp.h"

static struct termios old;

void raw(int on) {
	struct termios t = old;
	if (on) { tcgetattr(0, &old); t = old; t.c_lflag &= ~(ICANON | ECHO | ISIG); }
	tcsetattr(0, TCSAFLUSH, on ? &t : &old);
	printf(on ? "\033[?25l" : "\033[?25h\033[2J\033[H");
}

/* One keypress: a char, KUP/KDN for arrows, 27 for bare ESC, 3 for ctrl-c/EOF. */
int key(void) {
	char c;
	struct pollfd p = {0, POLLIN, 0};
	if (read(0, &c, 1) <= 0 || c == 3) return 3;
	if (c != 27 || poll(&p, 1, 30) <= 0) return c;
	if (read(0, &c, 1) && read(0, &c, 1)) return c == 'A' ? KUP : c == 'B' ? KDN : 0;
	return 0;
}

static int on_path(const char *cmd) {
	char *p = strdup(getenv("PATH") ? getenv("PATH") : ""), *d, *t, f[1100];
	int ok = 0;
	for (d = strtok_r(p, ":", &t); d && !ok; d = strtok_r(NULL, ":", &t))
		snprintf(f, sizeof f, "%s/%s", d, cmd), ok = !access(f, X_OK);
	free(p);
	return ok;
}

/* Type to filter, arrows + enter to choose; no match + enter uses the typed text. 0 = cancelled. */
int pick(char *out) {
	char q[64] = "";
	int m[64], n, sel = 0, c;
	for (;;) {
		n = 0;
		for (int i = 0; known[i] && n < 64; i++)
			if (strcasestr(known[i], q)) m[n++] = i;
		if (sel >= n) sel = n ? n - 1 : 0;
		printf("\033[2J\033[H" BLD CYN "  new profile " RST DIM "- choose a service\n\n" RST "  search: " BLD "%s" RST "_\n\n", q);
		for (int i = 0; i < n; i++) {
			const char *s = known[m[i]], *st = on_path(s) ? GRN "installed" : DIM "not found";
			printf(i == sel ? REV " > %-14s %s" RST "\n" : "   " YEL "%-14s" RST " %s" RST "\n", s, st);
		}
		if (!n) printf(DIM "   no match - enter runs '%s' as a custom command\n" RST, q);
		printf("\n" DIM "  type to search  up/down move  enter select  esc cancel\n" RST);
		fflush(stdout);
		c = key();
		if (c == 3 || c == 27) return 0;
		if (c == KUP && sel > 0) sel--;
		if (c == KDN && sel < n - 1) sel++;
		if (c == 127 || c == 8) { if (*q) q[strlen(q) - 1] = 0; }
		else if (c >= 32 && c < 127 && strlen(q) < 60) strncat(q, (char[]){c, 0}, 1);
		if (c == '\n' && (n || *q)) { snprintf(out, 64, "%s", n ? known[m[sel]] : q); return 1; }
	}
}
