<h1 align="center">mp</h1>
<p align="center"><b>Multi Profile: run any CLI under several isolated logins.</b><br>
Claude, Codex, gh, wrangler, aws and more. One small C binary, no dependencies.</p>

## Why

Most CLIs keep one login per config directory, so switching accounts means logging out and back in. `mp` gives each account its own directory and launches the real command inside it. Run two accounts in two terminals at once.

```
mp claude work      # create profile "work" for claude and run it (log in on first run)
mp gh personal      # same idea, different service

mp work -c          # later: the name is enough, and arguments pass through
mp personal pr list
```

## Install

Download a binary from the [latest release](https://github.com/mkalmousli/mp/releases/latest). Replace `linux-x86_64` with `linux-arm64`, `macos-arm64` or `macos-x86_64` as needed:

```
mkdir -p ~/.local/bin
curl -fsSL -o ~/.local/bin/mp https://github.com/mkalmousli/mp/releases/latest/download/mp-linux-x86_64
chmod +x ~/.local/bin/mp
```

Linux binaries are statically linked. Windows is not supported.

Or build from source with a C compiler:

```
./build.c install             # installs to ~/.local/bin (or $PREFIX/bin)
./build.c uninstall           # removes the binary; your profiles stay in ~/.mp
```

`./build.c build` builds `./mp` without installing it.

## Use

```
mp                         open the interactive TUI
mp NAME [args]             run profile NAME (its service is remembered)
mp SERVICE NAME [args]     create profile NAME for SERVICE and run it
mp ls                      list profiles
mp where [NAME]            show where files are stored
mp defaults NAME [on|off]  toggle the default config (claude only, no argument toggles)
mp mv OLD NEW              rename a profile
mp rm NAME [-y]            delete a profile (asks to confirm)
```

In the TUI, use the arrow keys or `j`/`k` to move, then `enter` to run, `n` for a new profile, `r` to rename, `c` to toggle the default config, `d` to delete and `q` to quit.

Profile names may use letters, digits, `-`, `_` and `.`, up to 60 characters. A profile named like a subcommand (`ls`, `rm`, ...) is shadowed by that subcommand when the arguments fit it, so avoid those names.

## How isolation works

Profiles live in `~/.mp/SERVICE/NAME` (override the location with `MP_DIR`). `mp` sets the environment, also sets `MP_PROFILE` to the profile name, and then replaces itself with the real command.

Services with a known config variable keep your real `HOME`:

| Service | Variables pointed at the profile |
|---|---|
| `claude` | `CLAUDE_CONFIG_DIR` |
| `codex` | `CODEX_HOME` |
| `gh` | `GH_CONFIG_DIR` |
| `glab` | `GLAB_CONFIG_DIR` |
| `wrangler` | `XDG_CONFIG_HOME` |
| `gcloud` | `CLOUDSDK_CONFIG` |
| `docker` | `DOCKER_CONFIG` |
| `kubectl` | `KUBECONFIG` |
| `aws` | `AWS_CONFIG_FILE`, `AWS_SHARED_CREDENTIALS_FILE` |
| `npm` | `NPM_CONFIG_USERCONFIG` |
| `git` | `GIT_CONFIG_GLOBAL` |

Any other command on your `PATH` also works. It gets a private `HOME` and private XDG directories inside the profile. Run `mp where NAME` to see exactly what a profile sets.

## Default config for Claude

When you create a `claude` profile in a terminal, `mp` offers a default config, which is currently a two-line status line:

```
 MP · work  │ ◆ Opus 5.5·high │ ▸ multiclaude ⎇ main │ $1.23 │ ⏱ 1h02m │ +12/-3
ctx ████░░░░░░ 42% 84.0k/200k │ cache 91% warm 2m │ 5h ████░ 70% ▲20 ↻2h29m │ wk █░░░░ 12% ↻4d15h
```

- Turn it on or off later with `mp defaults NAME` or `c` in the TUI. The change applies from the next launch.
- It is passed with `--settings` at each launch, so the profile's `settings.json` is never edited.
- Without a terminal (scripts, pipes) `mp` doesn't ask and leaves it off.
- Set `NO_COLOR` to disable colors.
- On macOS, `mp` must be on your `PATH` for the status line to run.

## License

[MIT](LICENSE)
