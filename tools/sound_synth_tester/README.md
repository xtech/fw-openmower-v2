# Sound Synth Tester

Host-side tool to experiment with note sequences using the **same** `Synth` that
runs in the firmware (`src/drivers/sound/sound_synth.cpp`) — no flashing needed.

## Build

```sh
./build.sh
```

Requires a host `g++` (C++17). Produces `./sound_synth_tester`.

## Usage

```sh
./sound_synth_tester "freq:dur[:lfoHzx10[:lfoDepth]] [note2 ...]" [--volume N] [--repeat N] [--waveform W] [--unison N] [--detune HZ]
./sound_synth_tester --list
```

* `freq` — fundamental frequency in Hz (`0` = silence/pause)
* `dur` — note duration in ms
* `lfoHzx10` — LFO rate × 10 (e.g. `20` = 2.0 Hz), default `0` = off
* `lfoDepth` — LFO frequency deviation in Hz, default `0`
* `--volume N` — volume 0–100 (default 80)
* `--repeat N` — repeat the whole sequence N times (default 1)
* `--waveform W` — oscillator waveform: `sine`, `square`, `triangle`, `saw` (default `sine`)
* `--unison N` — stack N detuned voices (odd: `1`/`3`/`5`/`7`, default `1`)
* `--detune HZ` — frequency spread between unison voices in Hz (default `0`)
* `--list` — print the current ROM default sequences (copy & tweak)

Output is **raw mono int16 PCM @ 16 kHz** on stdout. Pipe it into a player:

```sh
# ALSA
./sound_synth_tester "440:200 660:350" | aplay -f S16_LE -r 16000 -c 1

# PulseAudio
./sound_synth_tester "880:150 0:50 880:150" | paplay --raw --format=s16le --rate=16000 --channels=1

# ffmpeg
./sound_synth_tester "523:90 659:90 784:250" | ffplay -autoexit -f s16le -ar 16000 -ac 1 -i -

# emergency siren, 3× (~24 s)
./sound_synth_tester "950:8000:20:220" --repeat 3 | aplay -f S16_LE -r 16000 -c 1

# power-up riser (square / saw)
./sound_synth_tester "130:2200:1:900" --waveform square | aplay -f S16_LE -r 16000 -c 1
./sound_synth_tester "130:2200:1:900" --waveform saw --unison 3 --detune 6 | aplay -f S16_LE -r 16000 -c 1
./sound_synth_tester "130:2200:1:900" --waveform square --unison 5 --detune 8 | aplay -f S16_LE -r 16000 -c 1

# optional: save to WAV instead of playing
./sound_synth_tester "440:200 660:350" | sox -t raw -r 16000 -e signed -b 16 -c 1 - out.wav
```

## Notes

* Max **8 notes** per sequence (matches `kMaxNotes` in the firmware).
* The right channel is always silent (the MAX98357A is left-channel-only);
  the tool therefore emits mono.
* A found sequence can be pasted 1:1 into `kDefaultSoundDefs` in
  `src/drivers/sound/sound_definition.hpp`.
