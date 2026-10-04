#!/usr/bin/env python3
"""Convert only user-provided audio to map-compatible Opus. No synthesis."""
import argparse
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def encode(source, destination, gain=1.0):
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(source), "-vn", "-ar", "48000",
                    "-ac", "1", "-af", f"volume={gain:.8f}", "-c:a", "libopus", "-b:a", "64k",
                    "-application", "audio", str(destination)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bf-wav", type=pathlib.Path, help="optional user-provided BF WAV directory")
    parser.add_argument("--vine-boom", type=pathlib.Path, default=ROOT / "assets/asylum/source/vine-boom.mp3")
    parser.add_argument("--the-world", type=pathlib.Path, default=ROOT / "assets/asylum/source/the-world.mp3")
    args = parser.parse_args()
    destination = ROOT / "assets/asylum/sounds"
    destination.mkdir(parents=True, exist_ok=True)
    if args.bf_wav:
        sources = sorted(args.bf_wav.glob("*.wav"))
        if not sources:
            parser.error("No BF WAV files found")
        for source in sources:
            encode(source, destination / ("bf_" + source.stem.lower() + ".opus"))
    encode(args.vine_boom, destination / "vine_boom.opus")
    # Only gain changes. Each variant is the full, untrimmed user recording.
    for level in range(12):
        encode(args.the_world, destination / f"the_world_{level:02d}.opus", .15 + .85 * level / 11)
    print(f"Encoded user audio; {len(list(destination.glob('*.opus')))} map samples in {destination}")


if __name__ == "__main__":
    main()
