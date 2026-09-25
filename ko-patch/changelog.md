# v5.0.0
 - Fixed a crash when entering a level that has a text object in it. A label GD draws in a shared batch has no texture of its own, and swapping its font reached for one that was not there. Those are the level creator's own words anyway, so the patch leaves them alone now
 - Korean is drawn at the size the English was. The bundled font's line is shorter than the game's, so swapping fonts at the same scale made the text shrink; the scale is now corrected by exactly that difference
 - The Insane difficulty achievements: 결의, 광기의 문턱, 미쳐 가는 중, 광기, 제정신 보류 중, 목소리가 들려, 머리가 아파, 넌 미쳤어, 광기 너머
 - The Copy button also reads every installed mod's description and settings straight out of the mods themselves, so the ones with no public source are no longer invisible
 - The achievement screen can now be read in one press. The Copy button walks the game's own achievement table first, so all 546 titles and descriptions come out at once instead of fifty-five pages of scrolling
 - 1,231 more strings, read out of the source of Geode itself and of the 85 installed mods that have one, rather than waiting for them to appear on screen. Geode's mod manager (최근 갱신순, 설치 안 됨, 충돌하는 모드, 안전 모드); every mod's description and settings; Globed's rooms, voice chat and moderation (방 만들기, 음성 대화, 거리 음성); Eclipse Menu and QOLMod's hacks (자동 클릭, 코인 자동 줍기, 금고 모두 열기, 초당 클릭 수); Attempt Playback's whole manager; Demons In Between's 25 tiers; More Object Info's portal and ring names (중력 전환 포털, 분홍 점프 패드); BetterInfo's filters; GDDL, Jukebox, Object Workshop, Level Thumbnails, Object Groups and the rest
 - The single words that had been slipping through because they are one word long: the months, the medal tiers (청동, 은, 금, 백금, 다이아), the easing curves (2차, 3차, 사인, 탄성, 베지어), 연결됨, 연결 끊김, 기다리는 중, 관전, 배율, 마찰, 경사
 - The 33 vault achievements, which were never in the string list at all: 금고에 'lenny' 를 넣어 비밀 찾기, 비밀의 금고에서 'glubfub' 수수께끼를 풀어 열쇠지기의 코인 훔치기, 메인 메뉴에서 플레이어 750명 부수기

# v4.9.0
 - Names are left in English now. A level called "Silence" was coming out "무음", because a level name, a song title and a username go through the same label as everything else, and the table happened to hold that word. The official level names go with them - Stereo Madness, Polargeist, Deadlocked - which is what players call them anyway. They still read correctly inside a sentence: 'Stereo Madness' 연습 모드로 완료하기
 - The icon kit's tab words that collided the most (Play, Waves, Robots, Random, Install) are out of the table for the same reason

# v4.8.0
 - 65 more, from a second collector run: the loading screens (Geode 자료를 읽는 중), the icon kit's own names (큐브 95, 죽음 효과 2, 비행선 불꽃), the date on a level (2026년 8월 1일), the chest and quest timers (22시간, 12일), 주간 #475, and Globed's loading line
 - None of these were in the string list either

# v4.7.0
 - The collected list no longer leaks your API key. A string setting's value passes through a label like any other text, so the Gemini key was being written into it. Anything long with no spaces and a mix of letters and digits is now refused
 - The list was also unreadable: Geode draws a line by adding one word at a time, so " in the", " in the settings", " in the settings picks" all went in separately. Only the finished sentence is kept now
 - 63 more strings, found from that list: the List reward and Insane difficulty achievements, "Chest History", "Creator Contest", "Music Artist", "Hue:", and the descriptions of the mods in your list

# v4.6.0
 - Coloured words no longer smear across the line. "상자를 10" was green where only "상자" should have been: GD measures how far a <cg> tag reaches in bytes and then paints that many letters, and a Hangul letter is three bytes, so every colour ran three times too far. Every coloured Korean sentence in the game had this. The patch now takes the tags off before handing the text over and paints the letters itself, counting letters

# v4.5.0
 - The official level names are spelled by sound again - 스테레오 매드니스, 폴라가이스트, 지오메트리컬 도미네이터. They are what players call the levels to each other

# v4.4.0
 - The official level names now say what they mean instead of spelling out the English sound. 스테레오 광란, 다시 궤도 위로, 극지의 혼령, 메마름, 기지 또 기지, 놓지 못해, 도약, 시간 기계, 순환, 엇걸음, 난장 펑크, 만물 이론, 전기 인간의 모험, 클럽 걸음, 전기역학, 육각의 힘, 폭주 연산, 기하의 지배자, 교착, 손가락 질주 - and Meltdown, SubZero and the Tower with them
 - The names carry through everywhere they are used: the achievements built from them ("극지의 혼령!"), the coin goals, the completion lines

# v4.3.0
 - Demons In Between, Click Sounds and Edit Tools translated. 85 of your 120 enabled mods now have their text in the table

# v4.2.0
 - Achievements are fixed. "Polargeist!" stayed English because the achievement titles do not come from the game's string list at all - they come from a table of their own, which the coverage count never looked at. All 292 achievements are now checked, name, goal and the line you get once you have it: 876 strings, none left
 - Eclipse Menu, Icon Kit Switcher, Editor History, Layout Generator, Art Importer, Better Unlock Info, Level Storage API and the Cheat API

# v4.1.0
 - A new setting: "번역 안 된 글 적어 두기". Turn it on, walk through the menus you care about, then press "모은 글 복사". Every piece of English that reached the screen with no translation is written to config/missing.txt and copied to the clipboard - paste it and I can translate exactly what you saw. It is how to reach the mods that never published their source
 - The list leaves out save keys, sprite names, URLs and other people's mod names, so what is left is text a person actually reads

# v4.0.0
 - 29 of the game's own texts turned out never to have been translated at all. A template meant for "1 to 10 of 50" was quietly catching them - "Move this level to the top of the levels list?" became "Move this level~the top / 총 the levels list?" - and the coverage count was reading that as translated. The trigger help for Move, Rotate, Pickup, Keyframe, Count, SFX, Shock Line, Radial Blur, Grayscale, Pinch, Motion Blur, Hue Shift, Sepia and Touch, the Groups and Colour panels, the quest explainer and the terms of use are all Korean now
 - A template can mark a blank as digits-only, so it stops swallowing sentences
 - The shop dialog and the buy confirmations

# v3.9.0
 - Korean particles now pick themselves. "큐브 을(를)" reads "큐브를" and "비행선 을(를)" reads "비행선을" - the patch looks at the last letter that actually lands in the sentence, through the colour tags, and chooses. Numbers too: 1개를, 2개를
 - "5 불 조각 모으기" is now "불 조각 5개 모으기"

# v3.8.0
 - Fixed "이 Collect 5 Fire Shards 을(를) 열려면 Cube". Korean puts the words in a different order than English, so a template can now number its blanks - {0} and {1} - and fill them out of order
 - A piece cut out of a sentence is now translated too, not just looked up whole, so "Collect 5 Fire Shards" comes out Korean inside the unlock message
 - "2 weeks 전" is now "2주 전"
 - A sentence whose line breaks were flattened to spaces before it reached us now matches its template

# v3.7.0
 - Fixed "Increase Maximum Levels" coming out as "레벨 Increase Maximum개". A short template like "{} Levels" is meant to catch "3 Levels", but the blank accepts anything, so it swallowed the whole option name. A blank in a short template now refuses a run of English words
 - EditorMusic, Lasso Select, Bendy Duration Lines, Level Thumbnails and PersistenceAPI translated

# v3.6.0
 - The other 43 installed mods translated: Object Groups, Globed, GDDL Integration, Backups, PlatformerSaves, More Object Info, Attempt Replay, Allium, Achievements Reimagined, Integrated Demonlist, Misc Bugfixes, Requested Ratings, Save Buttons, Fake Rate, GDShare, Geometrize2GD, OMG, Better Progression, Infinity Particles, Auto Like, Auto Rate, Improved Transform Controls, Icon On Sliders, Improved Playtest, Named Editor Layers, Recent Objects and the rest
 - 7,024 entries now

# v3.5.0
 - Fixed Korean popup text running off the edge of the screen. GD decides where to break a line by walking the sentence one byte at a time and asking the font how wide that byte is; a Hangul letter is three bytes and none of them are in the font's table, so every Korean sentence measured as zero wide and never wrapped. The patch now measures the sentence with a real label and breaks the lines itself before handing it over
 - GDUtils and More Icons translated, and 43 more installed mods read for their text

# v3.4.0
 - QOLMod translated whole: every module name, every description, every popup, the keybind and shortcut editors, the colour and gradient pickers
 - Object Workshop, Jukebox, Overcharged Main Levels, Golden Best, Named Editor Layers, Editor Trail in Game, Robot/Ship Fire Color, Texture Loader, Editor Level ID API and matcool's editor mods
 - Around 1100 more lines of Korean; 6070 entries in all

# v3.3.0
 - Installed mods are translated too. Geode's own mod manager, BetterEdit, Tinker, BetterInfo and NodeIDs - their buttons, settings and every setting description

# v3.2.0
 - Other people's mod names are left alone. "Save Buttons" was coming out as "Buttons 저장" and "Overcharged Main Levels" as "레벨 Overcharged Main개" - the mod now asks Geode for every installed mod's name and developer and never touches them

# v3.1.0
 - Fixed English words being translated inside English sentences, like "모두 more clicking" in another mod's description. The label is built a word at a time, so the mod now notices English being appended to its own Korean and puts the sentence back
 - Shortened the button labels that were being squeezed to fit

# v3.0.1
 - Fixed "Normal mode" showing up as "아니오rmal mode". Multi-line text is split into several labels before it reaches the mod, so fragments of words were being looked up; it is translated whole now, before the split

# v3.0.0
 - The whole game is Korean now. All 4,872 strings in 2.2: every achievement and its description, the official level names, every settings page, every editor trigger, the vault dialogue and the loading screens

# v2.1.0
 - Text the game breaks across two lines is now recognised, so wrapped buttons and achievement descriptions translate too
 - Added the achievement screen's categories, the level stats window and the Visual settings

# v2.0.1
 - The update button could stop responding entirely after one press. It holds onto the download now, and always says something back

# v2.0.0
 - Letters now carry a black outline and are drawn a third larger, so Korean reads at the same weight as the game's own lettering
 - Added the mod manager's own screens and the buttons that were still English
 - Editor: the exit dialog's Save and Play / Save and Exit, the object counter with no songs loaded, level length in seconds, and the side panel other editor mods add

# v1.8.0
 - Optional Gemini translation for text the table does not cover, collected into an editable file

# v1.7.0
 - Fonts now carry the full common Hangul set, so Korean the mod did not write also draws

# v1.6.0
 - Added 배달의민족 주아 as a second font, selectable in the settings

# v1.5.0
 - Added the vault, quests, the settings screen, level info and the icon colour picker

# v1.4.0
 - Gold headings stay gold instead of turning into plain white text

# v1.3.0
 - Added a **Check for updates** button that installs the newest release from GitHub

# v1.2.0
 - Translation now covers the world map, gauntlets, level lists, the editor and its dialogs
 - Added sentence templates, so achievement lines translate whatever level name they carry
 - Words lifted out of a template are translated too, instead of being left in English

# v1.1.0
 - Bolder and bigger Korean text
 - Leave text alone when it is already Korean, so other Korean patches are not fought over
 - Added a setting to keep the game's own font instead of the bundled one

# v1.0.0
 - Korean translation for menus, the pause screen, options and common buttons
 - Text rendered in a Dunggeunmo pixel font, bundled as a bitmap font
