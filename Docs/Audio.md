# UNPAID: OVERTIME — Audio

Every sound in the game is synthesized procedurally in pure Python (no samples, no paid services) and imported
by the art pipeline. This page lists the sounds, where each one is used, how to regenerate them and how loud
each file is.

## Regenerate

```
Scripts\BuildArt.bat audio
```

Runs headless (editor closed, or at least `Lvl_Office` not open):

1. `Scripts/Editor/uo_audio_synth.py` writes 16-bit 44.1 kHz WAVs to `Content/UO/Audio/Source/` (mono for 3D
   emitters and one-shots, stereo for music and 2D beds), `manifest.json`, `loudness.md`, and refreshes the
   loudness table at the end of this page. All randomness is seeded, so the output is reproducible.
2. `Scripts/Editor/uo_audio.py` imports them as `/Game/UO/Audio/<Group>/SW_*`, sets looping, volume, sound class
   and attenuation per wave, builds the mix assets in `/Game/UO/Audio/Mix/` and the sound table
   `/Game/UO/Audio/DA_UOSoundSet`, then places the level audio in `Lvl_Office` and saves it.

The synth also runs standalone (any Python 3.9+, e.g. Unreal's bundled one) to iterate on sounds without the editor:

```
python Scripts/Editor/uo_audio_synth.py --only Footstep,Coffee     # regenerate some ids
python Scripts/Editor/uo_audio_synth.py --report-only              # loudness table only
```

`UO_AUDIO_SKIP_SYNTH=1` makes the audio stage reuse the existing WAVs (import + placement only).
A full level rebuild (`BuildArt.bat level`) places the level audio again by itself.

## How it fits together

| Piece | Where | What it does |
|---|---|---|
| Sound ids | `Core/UOAudioRules.h` (`UOSoundId::*`) | Code plays sounds by id, never by asset path. |
| Audio rules | `Core/UOAudioRules.cpp` | Pure, unit-tested decisions: floor material → surface, stride length, music per phase, station → interaction sounds, overtime blend, creak timing, evaluation reveal timeline. Tests: `UnpaidOvertime.Core.Audio.*`. |
| Sound table | `Audio/UOSoundSet` + `/Game/UO/Audio/DA_UOSoundSet` | id → wave variants, plus the volume mix and the sound classes. Loaded from the soft path in `DefaultGame.ini` (`[/Script/UnpaidOvertime.UOAudioSubsystem] SoundSetPath`). |
| Audio director | `Audio/UOAudioSubsystem` (world subsystem, client-side, cosmetic) | Random-variant playback, music crossfades, room tone, overtime drone/creaks/hum flicker, day-flow stingers, task chimes, evaluation reveal, volume mix. |
| Footsteps | `Audio/UOFootstepComponent` | On the player (`AUOCharacter`) and every coworker (`AUOAIEmployee`, 70 % volume): distance-based steps, floor trace → material name → carpet/tile, jump and land. |
| Interactions | `Gameplay/UOInteractionComponent` | Start one-shot + 3D hold loop at the station, completion sounds, item drop, "denied" buzz (local human only). Humans and AIs share it, so coworkers are heard working. |
| UI | `UI/UOUIStyle` | Hover/click/toggle sounds baked into the button and check box styles; slider ticks, menu whooshes, back and error sounds played through Slate (they keep playing while paused). |
| Volumes | `UOGameUserSettings` + Settings screen | Master, Music, Effects, Ambience, Interface sliders (live preview, saved on Apply). Applied as `SetSoundMixClassOverride` on `SM_UOVolumes` (leaf class volume = Master × category), pushed on world start and on change. |

Sound classes: `SC_Master` → `SC_Music`, `SC_SFX`, `SC_Ambience`, `SC_UI`, `SC_Voice` (Voice reserved, follows the
Effects slider). Music and UI classes are flagged as UI sounds so they keep playing under the pause menu; gameplay
sounds pause with the game.

### Level audio (`Lvl_Office`, placed by `uo_audio.place_level_audio`, folder `Audio/`)

All actors carry the tag `UOAudio` plus `UOAudio.<Kind>`; re-running the stage replaces them.

| Kind | Count | Sound | Notes |
|---|---|---|---|
| Hum | 22 | `Amb.Hum` | One per ~9 m cluster of ceiling light panels, `ATT_Hum`. Restarted at random offsets and detuned ±1.5 % at begin play so identical loops never sum in phase. In overtime they drop 7 % in pitch and flicker in volume. |
| Vent | 9 | `Amb.Vent` | At every ceiling vent. |
| Murmur | 1 | `Amb.OfficeMurmur` | Open plan centre, room-sized attenuation (`ATT_EmitterRoom`, near-field goes non-spatial). Drops to 20 % in overtime (the office empties). |
| Exterior | 10 | `Amb.Exterior` | Inside every other window (south and east façades). |
| Clock | 4 | `Amb.ClockTick` | At each wall clock. |
| Fridge / Vending | 2 / 1 | `Amb.Fridge`, `Amb.Vending` | Break room. |
| Radio | 1 | `Music.Menu` through `ATT_Radio` | Break room counter. Off by default; the subsystem turns it on during the shift only, band-limited (HP 380 Hz / LP 3.2 kHz) like a small speaker. |
| Reverb | 8 | `AudioVolume` + `RE_*` | One per room: open plan damped (0.75 s), lobby tile (1.5 s, bright), kitchen bright (1.15 s), corridor longer (1.9 s), small offices 0.55 s, supply closet 0.45 s. |

## Sound list

| Id | Variants | Used for | Class / space |
|---|---|---|---|
| `Footstep.Carpet` | 6 | Steps on carpet (open plan, offices, corridor, records) — player and AIs | SFX / 3D |
| `Footstep.Tile` | 6 | Steps on tile and hard floors (lobby, kitchen, supply, outside) | SFX / 3D |
| `Player.Jump` | 2 | Take-off (player only) | SFX / 3D |
| `Player.Land` | 2 | Landing after ≥ 0.3 s in the air | SFX / 3D |
| `Interact.Start` | 1 | Start of a hold (printer, coffee, inbox work) | SFX / 3D |
| `Interact.HoldLoop` | 1 | Generic "working" loop (delivering ink, filing, inbox work) | SFX / 3D loop |
| `Interact.Complete` | 1 | Generic completion (fallback) | SFX / 3D |
| `Interact.Denied` | 1 | Interaction not available / rejected (local player) | SFX / 2D |
| `Printer.Cartridge` | 1 | Ink delivered to a printer | SFX / 3D |
| `Printer.WhirLoop` | 1 | Printer work hold | SFX / 3D loop |
| `Printer.Feed` | 1 | Printer work done (paper feed) | SFX / 3D |
| `Coffee.GurgleLoop` | 1 | Coffee maker work hold | SFX / 3D loop |
| `Coffee.Beep` | 1 | Coffee done | SFX / 3D |
| `Drawer.Open` | 1 | Filing cabinet hold starts | SFX / 3D |
| `Drawer.Close` | 2 | Filing cabinet done | SFX / 3D |
| `Paper.Rustle` | 3 | Report taken from an inbox tray / inbox work done | SFX / 3D |
| `Supply.Rummage` | 2 | Ink taken from a supply closet | SFX / 3D |
| `Item.Pickup` | 1 | Layered on every item pickup | SFX / 3D |
| `Item.Drop` | 1 | Carried item dropped (Q) | SFX / 3D |
| `Task.Chime` | 1 | A sub-objective completed (full when it is yours, softer and lower for coworkers) | SFX / 2D |
| `Task.AllDone` | 1 | All work done — opens the evaluation reveal | SFX / 2D |
| `Day.ShiftBell` | 1 | Shift starts (3-tone PA chime) | SFX / 2D |
| `Day.OvertimeStinger` | 1 | Overtime starts (dissonant hit) | SFX / 2D |
| `Day.PowerDown` | 1 | Overtime starts (lights dim) | SFX / 2D |
| `Day.ClosingBell` | 1 | Day force-closed at the overtime cap — opens the evaluation reveal | SFX / 2D |
| `Eval.Star1..3` | 1 each | One rising ping per star earned (C6, E6, G6) | SFX / 2D |
| `Eval.Fired` | 1 | Low thud when the local employee is fired | SFX / 2D |
| `UI.Hover` / `UI.Click` / `UI.Toggle` | 1 each | Button hover, button press, choice buttons and check boxes | UI |
| `UI.Back` | 1 | Esc / back out of Setup or Settings | UI |
| `UI.SliderTick` | 1 | Every 5 % of slider travel | UI |
| `UI.Error` | 1 | Setup rejected by the server | UI |
| `UI.MenuOpen` / `UI.MenuClose` | 1 each | Pause and Settings open / pause closes | UI |
| `Amb.Hum` | 1 | Fluorescent light hum emitters | Ambience / 3D loop |
| `Amb.RoomTone` | 1 | HVAC room tone, always on (45 % in overtime) | Ambience / 2D stereo loop |
| `Amb.Vent` | 1 | Ceiling vent emitters | Ambience / 3D loop |
| `Amb.OfficeMurmur` | 1 | Distant voices + keyboards bed in the open plan | Ambience / 3D loop |
| `Amb.Fridge` / `Amb.Vending` | 1 each | Break room appliances | Ambience / 3D loop |
| `Amb.ClockTick` | 1 | Wall clocks | Ambience / 3D loop |
| `Amb.Exterior` | 1 | Faint traffic and birds at the windows | Ambience / 3D loop |
| `Amb.OvertimeDrone` | 1 | Overtime pad; fades in with the 4 s light fade, out over 3 s at the evaluation | Ambience / 2D stereo loop |
| `Amb.Creak` | 4 | Overtime creaks (1–2) and distant knocks (3–4), every 15–40 s, 7–15 m away from the listener | Ambience / 3D |
| `Music.Menu` | 1 | Main menu muzak (100 bpm, 16 bars, 38.4 s loop); also the break room radio during the shift | Music / 2D stereo loop |
| `Music.Dark` | 1 | Same score re-harmonised in minor, detuned, slowed to 86 bpm (44.7 s), tape wow, low-passed: overtime and evaluation | Music / 2D stereo loop |

Music by phase: main menu → `Music.Menu`; shift → no score (radio in the break room); overtime and evaluation →
`Music.Dark` (2.5 s crossfades).

## Debugging

- `uo.AudioDebug 1` (console, non-Shipping): logs every sound played (`UOAudio: play3d/play2d/ui ...`), loop stops
  and an `UOAudio status` line every 5 s (music, room tone, drone, emitters playing, radio).
- `Scripts\RunShots.bat` enables it and walks/jumps on tile and carpet, holds at a coffee maker, takes, delivers and
  drops ink, opens the pause menu, and reaches overtime and the evaluation, so `Saved\Logs\UnpaidOvertime.log`
  covers every category.
- Lifecycle lines are always logged: sound set loaded, volume mix pushed, emitters found, phase changes, music
  changes, radio, evaluation reveal.

## Quality notes (procedural limits)

Objective checks only (nobody has listened to these yet): every file peaks at or below −3 dBFS (ambience beds much
lower), every one-shot has an attack and a fade-out, loops are built circular and pass the seam check below.
Replace with recorded or licensed audio before release, in this order:

1. `Amb.OfficeMurmur` — formant-filtered saw "voices" read as babble at best; real distant office walla is far better.
2. `Footstep.*`, `Player.Jump/Land` — filtered-noise steps are serviceable but synthetic; shoe-on-carpet/tile
   recordings are a big upgrade and the most frequently heard sound.
3. `Paper.Rustle`, `Supply.Rummage`, `Drawer.*`, `Printer.*`, `Coffee.GurgleLoop` — modal/noise models; recognisable
   but not convincing up close.
4. `Amb.Exterior` (traffic + birds) and `Amb.Creak` — OK at a distance, thin in isolation.
5. Music — tasteful but simple FM; fine as placeholder muzak.

Holding up best: bells and chimes (`Day.ShiftBell`, `Task.Chime`, `Eval.Star*`), UI ticks, hums/fridge/vending
(tonal, periodic), the overtime drone and the stinger.

## Loudness table

<!-- loudness:begin -->
_Generated by `uo_audio_synth.py`. Loop seam = largest second difference across the wrap point / 99th percentile of the file's own second differences (<= ~1 is a smooth continuation, > 2 is flagged as a click). Game vol = wave volume set on import; Game RMS = file RMS + wave volume (before sound class, attenuation and player sliders)._

91 files, 317.9 s of audio, 0 flagged

| File | Ch | Dur (s) | Peak dBFS | RMS dBFS | Crest dB | Loop seam | Game vol | Game RMS dB | 3D | Check |
|---|---|---|---|---|---|---|---|---|---|---|
| Footstep_Carpet_01.wav | 1 | 0.32 | -3.0 | -19.5 | 16.5 | - | 0.50 | -25.5 | ATT_Footstep | ok |
| Footstep_Carpet_02.wav | 1 | 0.32 | -3.0 | -19.5 | 16.5 | - | 0.50 | -25.5 | ATT_Footstep | ok |
| Footstep_Carpet_03.wav | 1 | 0.32 | -3.0 | -19.4 | 16.4 | - | 0.50 | -25.4 | ATT_Footstep | ok |
| Footstep_Carpet_04.wav | 1 | 0.32 | -3.0 | -18.5 | 15.5 | - | 0.50 | -24.5 | ATT_Footstep | ok |
| Footstep_Carpet_05.wav | 1 | 0.32 | -3.0 | -18.2 | 15.2 | - | 0.50 | -24.2 | ATT_Footstep | ok |
| Footstep_Carpet_06.wav | 1 | 0.32 | -3.0 | -18.8 | 15.8 | - | 0.50 | -24.8 | ATT_Footstep | ok |
| Footstep_Tile_01.wav | 1 | 0.38 | -3.0 | -24.3 | 21.3 | - | 0.70 | -27.4 | ATT_Footstep | ok |
| Footstep_Tile_02.wav | 1 | 0.38 | -3.0 | -23.9 | 20.9 | - | 0.70 | -27.0 | ATT_Footstep | ok |
| Footstep_Tile_03.wav | 1 | 0.38 | -3.0 | -23.1 | 20.1 | - | 0.70 | -26.2 | ATT_Footstep | ok |
| Footstep_Tile_04.wav | 1 | 0.38 | -3.0 | -22.8 | 19.8 | - | 0.70 | -25.9 | ATT_Footstep | ok |
| Footstep_Tile_05.wav | 1 | 0.38 | -3.0 | -24.8 | 21.8 | - | 0.70 | -27.9 | ATT_Footstep | ok |
| Footstep_Tile_06.wav | 1 | 0.38 | -3.0 | -22.6 | 19.6 | - | 0.70 | -25.7 | ATT_Footstep | ok |
| Player_Jump_01.wav | 1 | 0.45 | -3.0 | -17.6 | 14.6 | - | 0.35 | -26.7 | ATT_Footstep | ok |
| Player_Jump_02.wav | 1 | 0.45 | -3.0 | -16.2 | 13.2 | - | 0.35 | -25.4 | ATT_Footstep | ok |
| Player_Land_01.wav | 1 | 0.55 | -3.0 | -18.0 | 15.0 | - | 0.60 | -22.4 | ATT_Footstep | ok |
| Player_Land_02.wav | 1 | 0.55 | -3.0 | -17.8 | 14.8 | - | 0.60 | -22.2 | ATT_Footstep | ok |
| Interact_Start.wav | 1 | 0.12 | -3.0 | -21.5 | 18.5 | - | 0.45 | -28.4 | ATT_Interaction | ok |
| Interact_HoldLoop.wav | 1 | 2.00 | -6.0 | -21.8 | 15.8 | 0.06 | 0.35 | -30.9 | ATT_Interaction | ok |
| Interact_Complete.wav | 1 | 0.30 | -3.0 | -17.4 | 14.4 | - | 0.50 | -23.5 | ATT_Interaction | ok |
| Interact_Denied.wav | 1 | 0.42 | -3.0 | -15.4 | 12.4 | - | 0.35 | -24.5 | 2D | ok |
| Printer_Cartridge.wav | 1 | 0.55 | -3.0 | -25.0 | 22.0 | - | 0.85 | -26.4 | ATT_Interaction | ok |
| Printer_WhirLoop.wav | 1 | 3.00 | -6.0 | -13.5 | 7.5 | 0.24 | 0.40 | -21.5 | ATT_Interaction | ok |
| Printer_Feed.wav | 1 | 1.50 | -3.0 | -15.7 | 12.7 | - | 0.55 | -20.9 | ATT_Interaction | ok |
| Coffee_GurgleLoop.wav | 1 | 4.00 | -6.0 | -20.9 | 14.9 | 0.24 | 0.55 | -26.1 | ATT_Interaction | ok |
| Coffee_Beep.wav | 1 | 0.75 | -3.0 | -10.8 | 7.8 | - | 0.25 | -22.9 | ATT_Interaction | ok |
| Drawer_Open.wav | 1 | 0.85 | -3.0 | -19.7 | 16.7 | - | 0.60 | -24.1 | ATT_Interaction | ok |
| Drawer_Close_01.wav | 1 | 0.90 | -3.0 | -21.3 | 18.3 | - | 0.65 | -25.1 | ATT_Interaction | ok |
| Drawer_Close_02.wav | 1 | 0.90 | -3.0 | -21.5 | 18.5 | - | 0.65 | -25.2 | ATT_Interaction | ok |
| Paper_Rustle_01.wav | 1 | 0.57 | -3.0 | -22.1 | 19.1 | - | 0.60 | -26.6 | ATT_Interaction | ok |
| Paper_Rustle_02.wav | 1 | 0.69 | -3.0 | -23.2 | 20.2 | - | 0.60 | -27.6 | ATT_Interaction | ok |
| Paper_Rustle_03.wav | 1 | 0.81 | -3.0 | -22.0 | 19.0 | - | 0.60 | -26.5 | ATT_Interaction | ok |
| Supply_Rummage_01.wav | 1 | 1.35 | -3.0 | -23.1 | 20.1 | - | 0.70 | -26.2 | ATT_Interaction | ok |
| Supply_Rummage_02.wav | 1 | 1.55 | -3.0 | -23.8 | 20.8 | - | 0.70 | -26.9 | ATT_Interaction | ok |
| Item_Pickup.wav | 1 | 0.30 | -3.0 | -19.8 | 16.8 | - | 0.50 | -25.8 | ATT_Interaction | ok |
| Item_Drop.wav | 1 | 0.50 | -3.0 | -21.4 | 18.4 | - | 0.60 | -25.9 | ATT_Interaction | ok |
| Task_Chime.wav | 1 | 1.60 | -3.0 | -16.4 | 13.4 | - | 0.45 | -23.3 | 2D | ok |
| Task_AllDone.wav | 2 | 2.60 | -3.0 | -19.6 | 16.6 | - | 0.55 | -24.8 | 2D | ok |
| Day_ShiftBell.wav | 2 | 4.20 | -3.0 | -17.4 | 14.4 | - | 0.60 | -21.8 | 2D | ok |
| Day_OvertimeStinger.wav | 2 | 4.50 | -3.0 | -20.5 | 17.5 | - | 0.70 | -23.6 | 2D | ok |
| Day_PowerDown.wav | 1 | 3.00 | -3.0 | -17.5 | 14.5 | - | 0.55 | -22.7 | 2D | ok |
| Day_ClosingBell.wav | 2 | 4.00 | -3.0 | -23.9 | 20.9 | - | 0.60 | -28.3 | 2D | ok |
| Eval_Star1.wav | 1 | 1.30 | -3.0 | -15.6 | 12.6 | - | 0.45 | -22.6 | 2D | ok |
| Eval_Star2.wav | 1 | 1.30 | -3.0 | -16.4 | 13.4 | - | 0.45 | -23.4 | 2D | ok |
| Eval_Star3.wav | 1 | 1.30 | -3.0 | -15.4 | 12.4 | - | 0.45 | -22.4 | 2D | ok |
| Eval_Fired.wav | 1 | 2.40 | -3.0 | -18.4 | 15.4 | - | 0.70 | -21.5 | 2D | ok |
| Eval_Stamp.wav | 1 | 1.10 | -3.0 | -20.4 | 17.4 | - | 0.70 | -23.5 | 2D | ok |
| Boss_Question.wav | 1 | 1.40 | -3.0 | -13.8 | 10.8 | - | 0.45 | -20.7 | 2D | ok |
| Boss_Tick.wav | 1 | 0.09 | -6.0 | -20.8 | 14.8 | - | 0.35 | -29.9 | 2D | ok |
| Boss_Correct.wav | 1 | 1.10 | -3.0 | -16.8 | 13.8 | - | 0.45 | -23.7 | 2D | ok |
| Boss_Wrong.wav | 1 | 0.70 | -4.0 | -14.3 | 10.3 | - | 0.40 | -22.2 | 2D | ok |
| Vote_Cast.wav | 1 | 0.45 | -4.0 | -19.8 | 15.8 | - | 0.50 | -25.8 | 2D | ok |
| Role_Employee.wav | 2 | 2.60 | -3.0 | -18.1 | 15.1 | - | 0.55 | -23.3 | 2D | ok |
| Role_Killer.wav | 2 | 3.20 | -3.0 | -19.8 | 16.7 | - | 0.60 | -24.2 | 2D | ok |
| Sabotage_PrinterJam.wav | 1 | 1.50 | -3.0 | -14.6 | 11.6 | - | 0.75 | -17.1 | ATT_Interaction | ok |
| Sabotage_PowerCut.wav | 1 | 1.80 | -3.0 | -17.8 | 14.8 | - | 0.70 | -20.9 | ATT_Interaction | ok |
| Sabotage_Lock.wav | 1 | 1.00 | -3.0 | -19.3 | 16.3 | - | 0.75 | -21.8 | ATT_Interaction | ok |
| Sabotage_FileCorrupt.wav | 1 | 1.30 | -3.0 | -17.7 | 14.7 | - | 0.55 | -22.9 | ATT_Interaction | ok |
| Sabotage_PropMove.wav | 1 | 1.00 | -3.0 | -20.1 | 17.1 | - | 0.65 | -23.8 | ATT_Interaction | ok |
| Terminal_Open.wav | 1 | 0.50 | -4.0 | -17.0 | 13.0 | - | 0.45 | -24.0 | 2D | ok |
| Terminal_Beep.wav | 1 | 0.12 | -6.0 | -12.0 | 6.0 | - | 0.35 | -21.1 | 2D | ok |
| Meeting_Gavel.wav | 1 | 1.20 | -3.0 | -24.5 | 21.5 | - | 0.85 | -25.9 | 2D | ok |
| Meeting_Bell.wav | 2 | 2.20 | -3.0 | -15.4 | 12.4 | - | 0.55 | -20.6 | 2D | ok |
| Kill_Sting.wav | 2 | 3.00 | -3.0 | -17.7 | 14.7 | - | 0.60 | -22.2 | 2D | ok |
| Meeting_VoteTick.wav | 1 | 0.22 | -6.0 | -19.0 | 13.0 | - | 0.40 | -27.0 | 2D | ok |
| Meeting_TimerTick.wav | 1 | 0.32 | -5.0 | -22.6 | 17.6 | - | 0.45 | -29.5 | 2D | ok |
| Meeting_RPSReveal.wav | 1 | 0.90 | -3.0 | -16.3 | 13.3 | - | 0.55 | -21.5 | 2D | ok |
| Meeting_Probation.wav | 1 | 1.40 | -3.0 | -19.5 | 16.5 | - | 0.65 | -23.2 | 2D | ok |
| Meeting_Forgery.wav | 1 | 1.60 | -3.0 | -15.4 | 12.4 | - | 0.60 | -19.8 | 2D | ok |
| UI_Hover.wav | 1 | 0.05 | -6.0 | -19.5 | 13.5 | - | 0.22 | -32.7 | 2D | ok |
| UI_Click.wav | 1 | 0.10 | -3.0 | -20.2 | 17.2 | - | 0.45 | -27.1 | 2D | ok |
| UI_Back.wav | 1 | 0.18 | -3.0 | -16.7 | 13.7 | - | 0.40 | -24.7 | 2D | ok |
| UI_Toggle.wav | 1 | 0.08 | -3.0 | -20.5 | 17.5 | - | 0.40 | -28.4 | 2D | ok |
| UI_SliderTick.wav | 1 | 0.03 | -6.0 | -20.3 | 14.3 | - | 0.20 | -34.3 | 2D | ok |
| UI_Error.wav | 1 | 0.40 | -3.0 | -11.5 | 8.5 | - | 0.30 | -21.9 | 2D | ok |
| UI_MenuOpen.wav | 1 | 0.42 | -4.0 | -17.9 | 13.9 | - | 0.35 | -27.0 | 2D | ok |
| UI_MenuClose.wav | 1 | 0.42 | -4.0 | -19.0 | 15.0 | - | 0.35 | -28.1 | 2D | ok |
| Amb_Hum.wav | 1 | 6.00 | -12.0 | -19.0 | 7.0 | 0.82 | 0.30 | -29.5 | ATT_Hum | ok |
| Amb_RoomTone.wav | 2 | 20.00 | -14.0 | -26.4 | 12.4 | 0.44 | 0.35 | -35.6 | 2D | ok |
| Amb_Vent.wav | 1 | 12.00 | -14.0 | -26.5 | 12.5 | 0.23 | 0.35 | -35.6 | ATT_EmitterSmall | ok |
| Amb_OfficeMurmur.wav | 1 | 30.00 | -10.0 | -29.5 | 19.5 | 0.17 | 0.70 | -32.6 | ATT_EmitterRoom | ok |
| Amb_Fridge.wav | 1 | 8.00 | -12.0 | -20.0 | 8.0 | 0.40 | 0.45 | -26.9 | ATT_EmitterSmall | ok |
| Amb_Vending.wav | 1 | 8.00 | -12.0 | -17.9 | 5.9 | 0.38 | 0.40 | -25.9 | ATT_EmitterSmall | ok |
| Amb_ClockTick.wav | 1 | 2.00 | -10.0 | -37.9 | 27.9 | 0.00 | 0.40 | -45.8 | ATT_Clock | ok |
| Amb_Exterior.wav | 1 | 30.00 | -12.0 | -25.0 | 13.0 | 0.01 | 0.40 | -33.0 | ATT_EmitterRoom | ok |
| Amb_OvertimeDrone.wav | 2 | 24.00 | -8.0 | -17.4 | 9.4 | 0.29 | 0.45 | -24.3 | 2D | ok |
| Amb_Creak_01.wav | 1 | 3.00 | -4.0 | -25.5 | 21.5 | - | 0.75 | -28.0 | ATT_Creak | ok |
| Amb_Creak_02.wav | 1 | 3.60 | -4.0 | -24.9 | 20.9 | - | 0.75 | -27.4 | ATT_Creak | ok |
| Amb_Creak_03.wav | 1 | 2.20 | -4.0 | -24.7 | 20.7 | - | 0.75 | -27.2 | ATT_Creak | ok |
| Amb_Creak_04.wav | 1 | 2.20 | -4.0 | -25.3 | 21.3 | - | 0.75 | -27.8 | ATT_Creak | ok |
| Music_Menu.wav | 2 | 38.40 | -3.0 | -15.5 | 12.5 | 0.01 | 0.50 | -21.5 | 2D | ok |
| Music_Dark.wav | 2 | 44.65 | -3.0 | -16.0 | 13.0 | 0.24 | 0.50 | -22.0 | 2D | ok |
<!-- loudness:end -->
