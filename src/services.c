// services.c - per-service isolation rules: which env vars point at a profile's files
#include "mp.h"

typedef struct { const char *cmd, *env, *rel; int file; } Rule;

/* Services listed here keep the real HOME and only redirect their own config. */
static const Rule rules[] = {
	{"claude", "CLAUDE_CONFIG_DIR", "config", 0}, {"codex", "CODEX_HOME", "config", 0},
	{"gh", "GH_CONFIG_DIR", "config", 0},         {"glab", "GLAB_CONFIG_DIR", "config", 0},
	{"wrangler", "XDG_CONFIG_HOME", "home/.config", 0}, {"gcloud", "CLOUDSDK_CONFIG", "config", 0},
	{"docker", "DOCKER_CONFIG", "config", 0},     {"kubectl", "KUBECONFIG", "kube/config", 1},
	{"aws", "AWS_CONFIG_FILE", "aws/config", 1},  {"aws", "AWS_SHARED_CREDENTIALS_FILE", "aws/credentials", 1},
	{"npm", "NPM_CONFIG_USERCONFIG", "npm/npmrc", 1}, {"git", "GIT_CONFIG_GLOBAL", "git/config", 1},
};

/* Offered by the TUI service picker; anything else on PATH also works (fake-HOME fallback). */
const char *known[] = {"claude", "codex", "gemini", "gh", "glab", "wrangler", "vercel", "netlify", "firebase", "supabase",
                       "aws", "gcloud", "az", "docker", "kubectl", "npm", "git", "heroku", "fly", "doctl", NULL};

/* ---------- default config: on when the profile dir holds a .defaults flag ---------- */
int has_defaults(const char *svc) { return !strcmp(svc, "claude"); }

static void flag(char *out, const char *svc, const char *name) {
	char d[1024];
	pdir(d, svc, name);
	snprintf(out, 1100, "%s/.defaults", d);
}

int defaults_on(const char *svc, const char *name) {
	char f[1100];
	flag(f, svc, name);
	return has_defaults(svc) && !access(f, F_OK);
}

void set_defaults(const char *svc, const char *name, int on) {
	char f[1100];
	flag(f, svc, name);
	if (!on) { unlink(f); return; }
	FILE *h = fopen(f, "w");
	if (h) fclose(h);
}

/* Layered per launch with --settings, so the profile's own settings.json stays untouched. */
const char *defaults_json(void) {
	static char out[2200];
	char self[1024];
	ssize_t n = readlink("/proc/self/exe", self, sizeof self - 1);
	self[n > 0 ? n : 0] = 0;
	if (n <= 0 || strpbrk(self, "'\"\\")) snprintf(self, sizeof self, "mp");
	snprintf(out, sizeof out, "{\"statusLine\":{\"type\":\"command\",\"command\":\"'%s' statusline\",\"refreshInterval\":5}}", self);
	return out;
}

static const char *XDG[] = {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"};
static const char *XSUB[] = {".config", ".local/share", ".cache", ".local/state"};

static void put(const char *env, const char *path, int file, int dry) {
	char d[1300], *s;
	if (dry) { printf("  " YEL "%s" RST DIM " = " RST "%s\n", env, path); return; }
	snprintf(d, sizeof d, "%s", path);
	if (file && (s = strrchr(d, '/'))) *s = 0;
	mkpath(d);
	setenv(env, path, 1);
}

/* Export (or with dry, print) the environment that isolates `svc` inside profile dir `dir`. */
void apply_env(const char *svc, const char *dir, int dry) {
	char v[1300];
	int hit = 0;
	for (size_t i = 0; i < sizeof rules / sizeof *rules; i++) {
		if (strcmp(rules[i].cmd, svc)) continue;
		hit = 1;
		snprintf(v, sizeof v, "%s/%s", dir, rules[i].rel);
		put(rules[i].env, v, rules[i].file, dry);
	}
	if (!hit) {
		snprintf(v, sizeof v, "%s/home", dir);
		put("HOME", v, 0, dry);
		for (int i = 0; i < 4; i++) {
			snprintf(v, sizeof v, "%s/home/%s", dir, XSUB[i]);
			put(XDG[i], v, 0, dry);
		}
	}
}
