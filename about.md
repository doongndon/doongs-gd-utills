# Doong's GD Utils

A small quality-of-life pack:

- **Info HUD** — attempts, best percent, live percent, attempt timer and CPS in any screen corner.
- **Instant restart** — skips the death animation delay.
- **Pause menu level info** — level ID, object count, attempts, plus a Copy ID button.
- **Crash safety** — forces a save when the app goes to the background, so nothing is lost if
  the system kills the game while it is suspended.
- **Deco tools** — a Deco button in the editor. *Scatter* randomly jitters the position,
  rotation and scale of the selection for natural looking decoration; *Ramp* changes rotation
  and scale gradually across the selection, left to right. Both register a normal undo step.

- **Daily challenge** — a Daily button on the main menu. Every day the server picks one level per difficulty (a
  main level, or a random rated level from Easy up to Easy Demon) and everyone using the mod races
  to beat it in the fewest attempts. Attempts are counted from the
  first time you open the level that day, practice runs included; clears in practice mode or from a
  start position do not count. Results are taken on trust - it is a game, not a tournament. Needs a
  server address in the settings (the server is in `daily-server/` of the repository).

- **Check for updates** — a button in the settings that pulls the newest release from GitHub
  and installs it. Restart the game to apply.

Everything is toggleable in the mod settings.
