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
  start position do not count. Results are taken on trust - it is a game, not a tournament. It uses the
  official server out of the box; the server code is in `daily-server/` of the repository.
- **Roguelike daily** — one more slot in the daily challenge. The game stops at 25, 50 and 75%
  and offers three cards: help cards (Shield, Slow) cost points, curses (Mirror, Mini, Rush,
  Dark, Quake, Ghost, Silence, Blackout) earn them. Pick one, and the run carries on after a
  three-second countdown. Dying loses every card - the next attempt starts empty. Score is 100
  plus your cards; highest score wins, fewer attempts breaks ties. Everyone gets the same cards
  that day.

- **Check for updates** — a button in the settings that pulls the newest release from GitHub
  and installs it. Restart the game to apply.

Everything is toggleable in the mod settings.
