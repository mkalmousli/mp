// statusline.c - claude status line: session JSON on stdin, two colored lines on stdout
#include "mp.h"
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <time.h>

static char in[1 << 16];
static int first, nocolor;

/* ---------- minimal JSON lookup over `in` ---------- */
static const char *ws(const char *s) {
	while (*s && strchr(" \t\r\n", *s)) s++;
	return s;
}

/* Pointer just past the value at s, NULL if malformed. */
static const char *skip(const char *s) {
	int depth = 0;
	do {
		s = ws(s);
		if (!*s) return NULL;
		if (*s == '"') {
			for (s++; *s != '"'; s++) {
				if (!*s) return NULL;
				if (*s == '\\' && !*++s) return NULL;
			}
			s++;
		}
		else if (*s == '{' || *s == '[') depth++, s++;
		else if (*s == '}' || *s == ']') depth--, s++;
		else if (depth) s++;
		else while (*s && !strchr(",}] \t\r\n", *s)) s++;
	} while (depth > 0);
	return s;
}

/* Value at a dotted path like "model.display_name"; NULL if absent or null. */
static const char *dig(const char *path) {
	const char *s = ws(in);
	while (*path) {
		size_t n = strcspn(path, ".");
		if (*s != '{') return NULL;
		for (s = ws(s + 1);; s = ws(s + 1)) {
			if (*s != '"') return NULL;
			const char *k = s + 1, *e = skip(s);
			if (!e) return NULL;
			size_t klen = e - 1 - k;
			s = ws(e);
			if (*s != ':') return NULL;
			s = ws(s + 1);
			if (klen == n && !strncmp(k, path, n)) break;
			if (!(s = skip(s))) return NULL;
			s = ws(s);
			if (*s != ',') return NULL;
		}
		path += n + (path[n] == '.');
	}
	return !*s || *s == 'n' ? NULL : s;
}

static double num(const char *path, double def) {
	const char *v = dig(path);
	return v && strchr("-0123456789", *v) ? strtod(v, NULL) : def;
}

/* String at path; escapes that could reach the terminal as control chars become '?'. */
static char *str(const char *path, char *out, size_t size) {
	const char *v = dig(path);
	size_t n = 0;
	if (v && *v == '"')
		for (v++; *v && *v != '"' && n + 1 < size; v++) {
			char c = *v;
			if (c == '\\') {
				if (!(c = *++v)) break;
				if (c == 'u') for (int i = 0; i < 4 && v[1]; i++) v++;
				if (!strchr("\"\\/", c)) c = '?';
			}
			out[n++] = c;
		}
	out[n] = 0;
	return out;
}

/* ---------- output helpers ---------- */
static void on(const char *code) { if (!nocolor) printf("\033[%sm", code); }
static void off(void) { if (!nocolor) printf(RST); }

static void paint(const char *code, const char *fmt, ...) {
	va_list ap;
	on(code);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	off();
}

static void part(void) {
	if (!first) paint("90", " │ ");
	first = 0;
}

static const char *level(double pct, double warn, double crit) { return pct >= crit ? "31" : pct >= warn ? "33" : "32"; }

static void bar(double pct, int width, const char *color) {
	pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
	int filled = pct / 100 * width + 0.5;
	on(color);
	for (int i = 0; i < filled; i++) printf("█");
	off();
	on("90");
	for (int i = filled; i < width; i++) printf("░");
	off();
}

static char *tokens(double n, char *out) {
	static const struct { double lim; const char *suf; } u[] = {{1e9, "B"}, {1e6, "M"}, {1e3, "k"}};
	for (int i = 0; i < 3; i++)
		if (n >= u[i].lim) {
			double v = n / u[i].lim;
			snprintf(out, 16, v >= 100 ? "%.0f%s" : "%.1f%s", v, u[i].suf);
			return out;
		}
	snprintf(out, 16, "%.0f", n);
	return out;
}

static char *duration(double seconds, char *out) {
	long sec = seconds > 0 ? seconds : 0, d = sec / 86400, h = sec % 86400 / 3600, m = sec % 3600 / 60;
	if (d) snprintf(out, 32, "%ldd%02ldh", d, h);
	else if (h) snprintf(out, 32, "%ldh%02ldm", h, m);
	else if (m) snprintf(out, 32, "%ldm", m);
	else snprintf(out, 32, "%lds", sec);
	return out;
}

/* Branch of cwd with a trailing * when dirty, "" outside a repo. Gives git one second. */
static void git_label(const char *cwd, char *out, size_t size) {
	char buf[4096];
	int fd[2], n = 0;
	*out = 0;
	if (!*cwd || pipe(fd)) return;
	pid_t pid = fork();
	if (pid < 0) { close(fd[0]); close(fd[1]); return; }
	if (!pid) {
		int null = open("/dev/null", O_WRONLY);
		dup2(fd[1], 1);
		if (null >= 0) dup2(null, 2);
		close(fd[0]);
		close(fd[1]);
		/* --no-optional-locks: runs in the background and may be killed, so never hold index.lock. */
		execlp("git", "git", "--no-optional-locks", "-C", cwd, "status", "--porcelain", "-b", "-uno", (char *)NULL);
		_exit(127);
	}
	close(fd[1]);
	struct pollfd p = {fd[0], POLLIN, 0};
	while (n < (int)sizeof buf - 1 && poll(&p, 1, 1000) > 0) {
		ssize_t r = read(fd[0], buf + n, sizeof buf - 1 - n);
		if (r <= 0) break;
		n += r;
	}
	buf[n] = 0;
	close(fd[0]);
	kill(pid, SIGKILL);
	waitpid(pid, NULL, 0);
	if (strncmp(buf, "## ", 3)) return;
	char *nl = strchr(buf, '\n'), *b = buf + 3, *dots;
	int dirty = nl && nl[1];
	if (nl) *nl = 0;
	if (!strncmp(b, "No commits yet on ", 18)) b += 18;
	if ((dots = strstr(b, "..."))) *dots = 0;
	snprintf(out, size, "%.200s%s", b, dirty ? "*" : "");
}

/* ---------- the two lines ---------- */
static void top(void) {
	char a[256], b[1024], t[32];
	const char *prof = getenv("MP_PROFILE");
	first = 1;
	part();
	if (prof) paint("1;7;34", " MP · %s ", prof);
	else paint("1;7;34", " MP ");

	part();
	paint("1;35", "◆ %s", *str("model.display_name", a, sizeof a) ? a : "?");
	if (*str("effort.level", a, sizeof a)) paint("90", "·%s", a);

	part();
	if (!*str("workspace.current_dir", b, sizeof b)) str("cwd", b, sizeof b);
	size_t len = strlen(b);
	while (len > 1 && b[len - 1] == '/') b[--len] = 0;
	const char *base = strrchr(b, '/');
	base = base ? base + 1 : b;
	paint("36", "▸ %s", *base ? base : "/");
	git_label(b, a, sizeof a);
	if (*a) paint(a[strlen(a) - 1] == '*' ? "33" : "32", " ⎇ %s", a);

	double cost = num("cost.total_cost_usd", -1);
	if (cost >= 0) { part(); paint("33", cost >= 100 ? "$%.0f" : "$%.2f", cost); }
	double ms = num("cost.total_duration_ms", 0);
	if (ms > 0) { part(); paint("90", "⏱ %s", duration(ms / 1000, t)); }
	double add = num("cost.total_lines_added", 0), rem = num("cost.total_lines_removed", 0);
	if (add > 0 || rem > 0) { part(); paint("32", "+%.0f", add); paint("31", "/-%.0f", rem); }
	printf("\n");
}

static void limit(const char *label, const char *win, double span, int width) {
	char path[64], t[32];
	snprintf(path, sizeof path, "rate_limits.%s.used_percentage", win);
	double used = num(path, -1);
	if (used < 0) return;
	snprintf(path, sizeof path, "rate_limits.%s.resets_at", win);
	double reset = num(path, 0), now = time(NULL);
	part();
	printf("%s ", label);
	bar(used, width, level(used, 60, 85));
	paint(level(used, 60, 85), " %.0f%%", used);
	if (reset > 0) {
		double left = (reset - now) / span;
		left = left < 0 ? 0 : left > 1 ? 1 : left;
		double ahead = used - (1 - left) * 100;
		if (ahead > 10) paint("31", " ▲%.0f", ahead);
	}
	if (reset > now) paint("90", " ↻%s", duration(reset - now, t));
}

static void bottom(int width) {
	char a[16], b[16], t[32];
	double size = num("context_window.context_window_size", 200000);
	/* total_input_tokens is cumulative for the session, so prefer the live context figures. */
	double used = num("context_window.current_usage.input_tokens", 0) +
	              num("context_window.current_usage.cache_creation_input_tokens", 0) +
	              num("context_window.current_usage.cache_read_input_tokens", 0);
	if (used <= 0) used = num("context_window.total_input_tokens", 0);
	double pct = num("context_window.used_percentage", size > 0 ? used / size * 100 : 0);
	const char *c = level(pct, 50, 80);
	first = 1;
	part();
	printf("ctx ");
	bar(pct, width, c);
	paint(c, " %.0f%%", pct);
	printf(" %s/%s", tokens(used, a), tokens(size, b));

	double hit = num("prompt_cache.hit_ratio", -1);
	if (hit >= 0) {
		double exp = num("prompt_cache.expires_at", 0), now = time(NULL);
		const char *warm = dig("prompt_cache.warm");
		part();
		printf("cache ");
		paint(level(100 - hit * 100, 30, 60), "%.0f%%", hit * 100);
		if (warm && *warm == 't' && exp > now) paint("32", " warm %s", duration(exp - now, t));
		else paint("31", " cold");
	}
	int small = width / 2 < 4 ? 4 : width / 2;
	limit("5h", "five_hour", 5 * 3600, small);
	limit("wk", "seven_day", 7 * 86400, small);
	printf("\n");
}

int statusline(void) {
	size_t n = 0;
	ssize_t r;
	while (n < sizeof in - 1 && (r = read(0, in + n, sizeof in - 1 - n)) > 0) n += r;
	in[n] = 0;
	nocolor = getenv("NO_COLOR") != NULL;
	const char *cols = getenv("COLUMNS");
	top();
	bottom(!cols || atoi(cols) >= 100 ? 10 : 5);
	return 0;
}
