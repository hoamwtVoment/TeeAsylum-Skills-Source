#!/usr/bin/env python3
"""Validate packed map resources and the near-int32-limit jumpscare contract."""
import hashlib
import pathlib
import struct
import tempfile
import unittest
from unittest.mock import patch

from asylum_map_assets import (ROOT, INT_MAX, NAME, IMAGE, ENVELOPE, GROUP, LAYER,
                               ENVPOINTS, SOUND, DataFile, decode_name, embed, values, REMOVED_SYNTH_SAMPLES,
                               GRAY_NAME, GRAY_BASE, GRAY_WINDUP, GRAY_END, JUMP_START, JUMP_FADE)

MAPS = sorted((ROOT / "data/maps").glob("*.map")) + sorted((ROOT / "data/maps7").glob("*.map"))
SOUNDS = sorted((ROOT / "assets/asylum/sounds").glob("*.opus"))


class PackedMapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT / "build").mkdir(exist_ok=True)

    def test_all_samples_embedded_and_match_sources(self):
        self.assertEqual(len(SOUNDS), 35)
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                sounds = {data.get(values(item[2])[2]).rstrip(b"\0").decode(): values(item[2])
                          for item in data.typed(SOUND)}
                for source in SOUNDS:
                    sample = sounds[source.stem]
                    self.assertEqual(sample[1], 0)  # Embedded, never external.
                    self.assertEqual(sample[4], source.stat().st_size)
                    self.assertEqual(data.get(sample[3]), source.read_bytes())
                    self.assertIn(b"OpusHead", source.read_bytes())

    def test_synthesized_audio_removed_from_items_and_raw_blocks(self):
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                names = {data.get(values(item[2])[2]).rstrip(b"\0").decode() for item in data.typed(SOUND)}
                self.assertFalse(names & REMOVED_SYNTH_SAMPLES)
                skill_names = {source.stem for source in SOUNDS}
                self.assertTrue(skill_names.issubset(names))
                # Tour maps additionally retain their original embedded BGM.
                extras = names - skill_names
                self.assertTrue(extras.issubset({"asylum_lobby_propaganda", "asylum_10hourburstman", "asylum_10hourburstman_phase2", "BGM1", "BGM2"}), extras)
                self.assertIn("vine_boom", names)
                # Deleting only the sound table would leave hidden old Opus blobs.
                count = sum(b"OpusHead" in data.get(index) for index in range(len(data.blocks)))
                self.assertEqual(count, len(names))

    def test_no_autoplay_sound_sources_added(self):
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                sound_names = {item[1]: data.get(values(item[2])[2]).rstrip(b"\0").decode()
                               for item in data.typed(SOUND)}
                for layer in data.typed(LAYER):
                    payload = values(layer[2])
                    if payload[1] == 10:
                        self.assertIn(sound_names[payload[6]], {"asylum_lobby_propaganda", "asylum_10hourburstman", "asylum_10hourburstman_phase2"})

    def test_the_world_variants_finish_before_the_full_stop(self):
        clips = sorted((ROOT / "assets/asylum/sounds").glob("the_world_*.opus"))
        self.assertEqual(len(clips), 12)
        durations = []
        for clip in clips:
            raw = clip.read_bytes()
            self.assertIn(b"OpusHead", raw)
            preskip = struct.unpack_from("<H", raw, raw.index(b"OpusHead") + 10)[0]
            offset, granule = 0, 0
            while offset < len(raw):
                self.assertEqual(raw[offset:offset + 4], b"OggS")
                granule = struct.unpack_from("<Q", raw, offset + 6)[0]
                segments = raw[offset + 26]
                offset += 27 + segments + sum(raw[offset + 27:offset + 27 + segments])
            duration = (granule - preskip) / 48000
            durations.append(duration)
            self.assertGreater(duration, 4.4)
            self.assertLessEqual(duration, (1000 + GRAY_WINDUP) / 1000)
        self.assertEqual(min(durations), max(durations))  # Volume only, never trimmed.

    def test_foreground_quad_stretched_four_by_three_and_transparent(self):
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                group = next(values(item[2]) for item in data.typed(GROUP)
                             if len(values(item[2])) >= 15 and decode_name(values(item[2])[12:15]) == "AsylumFX")
                self.assertEqual(group[3:5], [0, 0])  # Fixed viewport parallax.
                self.assertEqual(group[5], len(data.typed(LAYER)) - 2)  # Two foreground quads.
                self.assertEqual(group[6], 2)
                layer = values(data.typed(LAYER)[group[5]][2])
                self.assertEqual(layer[1], 3)
                quad = values(data.get(layer[5]))
                width, height = quad[2] - quad[0], quad[5] - quad[1]
                self.assertEqual(width * 3, height * 4)
                self.assertEqual(quad[13:26:4], [160] * 4)
                image = values(data.typed(IMAGE)[layer[6]][2])
                self.assertEqual(data.get(image[4]).rstrip(b"\0"), NAME.encode())

    def test_timeline_hidden_until_final_seconds_and_ends_at_limit(self):
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                envelope = next(values(item[2]) for item in data.typed(ENVELOPE)
                                if decode_name(values(item[2])[4:12]) == NAME)
                self.assertEqual(envelope[0:2], [2, 4])
                self.assertEqual(envelope[3], 4)
                self.assertEqual(envelope[12], 1)
                points = values(data.typed(ENVPOINTS)[0][2])[envelope[2] * 6:(envelope[2] + 4) * 6]
                self.assertEqual(points[::6], [0, JUMP_START, JUMP_START + JUMP_FADE, INT_MAX])
                self.assertEqual(points[5::6], [0, 1024, 0, 0])
                self.assertEqual(points[1::6], [0, 1, 0, 0])  # Instant appear, linear fade, hidden.

    def test_jumpscare_alpha_decreases_from_full_to_zero(self):
        samples = [max(0.0, 1.0 - elapsed / JUMP_FADE) for elapsed in range(0, JUMP_FADE + 1, 20)]
        self.assertEqual(samples[0], 1.0)
        self.assertEqual(samples[-1], 0.0)
        self.assertTrue(all(a > b for a, b in zip(samples, samples[1:])))

    def test_grey_quad_fades_in_slowly_and_out_fast(self):
        for path in MAPS:
            with self.subTest(map=path):
                data = DataFile(path)
                envelope = next(values(item[2]) for item in data.typed(ENVELOPE)
                                if decode_name(values(item[2])[4:12]) == GRAY_NAME)
                self.assertEqual(envelope[3], 6)
                points = values(data.typed(ENVPOINTS)[0][2])[envelope[2] * 6:(envelope[2] + 6) * 6]
                self.assertEqual(points[::6], [0, GRAY_BASE, GRAY_BASE + GRAY_WINDUP,
                                              GRAY_BASE + GRAY_END, GRAY_BASE + GRAY_END + 256, INT_MAX])
                self.assertEqual(points[5::6], [0, 0, 1024, 1024, 0, 0])
                self.assertEqual(points[1::6], [0, 1, 0, 1, 0, 0])
                layer = next(values(i[2]) for i in data.typed(LAYER) if values(i[2])[1] == 3
                             and decode_name(values(i[2])[7:10]) == "TimeStop")
                self.assertEqual(layer[6], -1)  # Texture-free, gray vertex colors.
                quad = values(data.get(layer[5]))
                self.assertEqual(quad[10:14], [80, 80, 80, 120])

    def test_float_precision_and_tick_clock_margins(self):
        # Emulate legacy clients' 32-bit float seconds over the 1.28s fade.
        offset = (INT_MAX - 2560) * 50 // 1000
        float32 = lambda number: struct.unpack("<f", struct.pack("<f", number))[0]
        for elapsed in range(65):
            millis = float32((offset + elapsed) / 50) * 1000
            self.assertGreater(millis, JUMP_START - 128)
            self.assertLess(millis, INT_MAX)
        self.assertLess(offset + 60, 2**31)

    def test_repacking_is_idempotent(self):
        # Reuse the already-decoded raw pixels: this test needs no ffmpeg.
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            for path in MAPS:
                with self.subTest(map=path):
                    data = DataFile(path)
                    image = next(values(item[2]) for item in data.typed(IMAGE)
                                 if data.get(values(item[2])[4]).rstrip(b"\0") == NAME.encode())
                    copy = pathlib.Path(directory) / path.name
                    copy.write_bytes(path.read_bytes())
                    before = hashlib.sha256(copy.read_bytes()).digest()
                    with patch("asylum_map_assets.subprocess.check_output", return_value=data.get(image[5])):
                        embed(copy, ROOT / "assets/asylum/jumpscare-rock.png", SOUNDS)
                    self.assertEqual(hashlib.sha256(copy.read_bytes()).digest(), before)


if __name__ == "__main__":
    unittest.main()
