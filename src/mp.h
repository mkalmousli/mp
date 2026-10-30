// mp.h - shared types, colors and storage API
#pragma once
#define _GNU_SOURCE
#include <dirent.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define RST "\033[0m"
#define BLD "\033[1m"
#define DIM "\033[2m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define REV "\033[7;36m"


enum { KUP = 256, KDN };

typedef struct { char svc[64], name[64]; } Prof;
extern Prof ps[512];
extern int np;
extern char root[512];

int valid(const char *s);
void mkpath(const char *p);
void load(void);
void del(const Prof *p);
int rename_prof(const Prof *p, const char *to);
void run(const char *svc, const char *name, char **argv);
void pdir(char *out, const char *svc, const char *name);
void apply_env(const char *svc, const char *dir, int dry);
extern const char *known[];
int has_defaults(const char *svc);
int defaults_on(const char *svc, const char *name);
void set_defaults(const char *svc, const char *name, int on);
const char *defaults_json(void);
int statusline(void);
void raw(int on);
int key(void);
int pick(char *out);
void tui(void);
