# Korean Patch

Translates Geometry Dash into Korean, drawn in a bundled font that carries a black
outline like the game's own lettering.

Text is swapped at the single point every label in the game passes through, so menus,
popups and buttons are all covered by the same path. Anything without a translation is
left in English, untouched.

## Coverage

Every one of the 4,872 strings in Geometry Dash 2.2 is covered: the 547 achievements and
their descriptions, the official level names, every settings page with its help text,
every editor trigger, the vault dialogue, the loading screens and the endscreen quips.

The string list is not from memory. It is the dump published by GDL, the Russian
localisation project, which pulled it out of the game itself. Working from that is what
makes coverage countable rather than a guess.

Repeated sentences are templates rather than 4,872 separate lines: one rule covers every
"Complete '<level>' in Normal mode", every shard rank, every Path of <element>. Strings
holding a number or a name are templates too, since a label carrying one never matches as
fixed text.

Installed mods are covered too, not just the game. Geode's own mod manager, BetterEdit,
Tinker, BetterInfo and NodeIDs: their buttons, their settings, and the description under
every setting. Those strings were read out of each mod's source with `tools/extract.py`
rather than copied off screenshots, so what is covered is again countable.

What is not covered is every other mod, and there is no list to work from for those - each
one would have to be read the same way. The Gemini setting exists for that gap.

Mod names and their developers are never translated. They are names other people chose.

Translations live in `translations/ko.json`. Adding a line there is all it takes to cover
one more piece of text.

Both fonts carry the whole common Hangul set, not only the characters the translations
happen to use, so Korean that this mod did not write - a level someone named in Korean,
say - still draws.

Titles keep their gold. GD draws headings in a gold font and body text in a white one, so
the bundled font is baked in both colours and a translated label keeps whichever its
original used.

## Machine translation

Off by default. Turn it on with your own Gemini key and text with no translation yet is
sent to Gemini; answers land in `config/learned.json`, which is plain JSON you can open
and edit.

Treat it as a way of gathering candidates rather than a finished translation. Every label
in the game arrives through one function, so the mod cannot tell an interface string from
a level someone named - the prompt asks Gemini to leave proper nouns alone, and answers
that come back unchanged are dropped, but some will still slip through. Read the file,
delete what is wrong, and anything good is worth moving into `translations/ko.json` where
everyone gets it.

Requests stop at 300 a session and three at a time. Your key is stored in plain text on
the device, as mod settings are.

## Updating

**Check for updates** in the settings pulls the newest release from GitHub and installs it.
Restart the game afterwards to apply it.

## Living with other Korean patches and texture packs

Text that is **already Korean** is left alone, so a Korean pack that got there first keeps
its wording instead of the two fighting over the same label.

If your texture pack already supplies a font that can draw Korean, turn off
**Use the bundled font** in the settings. The translation still applies, but the label
keeps the pack's font, so one screen never ends up with two different typefaces.

The bundled font lives under this mod's own name, so a texture pack cannot overwrite it
and it cannot overwrite the game's.

## Fonts

Two are bundled and **Font** in the settings picks between them. Jua is the rounded face
most Korean packs use; Dunggeunmo is a pixel font, which sits better next to a pixel
texture pack. The choice applies to screens opened after it, so step out and back in to
see it change.


Both are baked into bitmap atlases by `tools/make_atlas.py`, which draws every glyph with
a black outline so the text reads against any background, the same way the game's own
lettering does. Three resolutions are written for each face and the game picks one.

Neo둥근모 (c) 2017-2021 Eunbin Jeong, licensed under the SIL Open Font License 1.1, which
this derivative inherits. The original Dunggeunmo bitmap font was released into the public
domain by 김중태.

배달의민족 주아 (c) 2014 Woowa Brothers, also under the SIL Open Font License 1.1.
