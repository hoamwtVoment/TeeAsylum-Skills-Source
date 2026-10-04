#!/usr/bin/env python3
"""Embed Asylum samples and a hidden 4:3 jumpscare quad without changing tiles.

Requires ffmpeg only for decoding the PNG; map IO uses the Python stdlib.
All original item/data indices are retained. Re-running updates our assets,
instead of duplicating sound items, envelopes or quad layers.
"""
import argparse
import itertools
import pathlib
import struct
import subprocess
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
INT_MAX = 2**31 - 1
NAME = "asylum_jumpscare"
GRAY_NAME = "asylum_time_stop_gray"
GRAY_BASE = 2147467264
GRAY_WINDUP = 3420
GRAY_END = GRAY_WINDUP + 5000
JUMP_START = INT_MAX - 2560
JUMP_FADE = 1280
IMAGE, ENVELOPE, GROUP, LAYER, ENVPOINTS, SOUND = 2, 3, 4, 5, 6, 7
REMOVED_SYNTH_SAMPLES = {"asylum_" + name for name in (
    "bonk vine_boom fanfare drumroll sad boing error scream choo_choo kaching "
    "shing rustle wub alarm heartbeat thunder record_scratch airhorn glitch").split()}


def ints(values):
    return struct.pack(f"<{len(values)}i", *values)


def values(data):
    return list(struct.unpack(f"<{len(data) // 4}i", data))


def name_ints(name, count):
    # Teeworlds StrToInts adds 128 to each byte and stores it most significant first.
    raw = name.encode().ljust(count * 4, b"\0")[:count * 4]
    result = []
    for offset in range(0, len(raw), 4):
        number = int.from_bytes(bytes((byte + 128) & 255 for byte in raw[offset:offset + 4]), "big")
        result.append(number if number < 2**31 else number - 2**32)
    result[-1] &= ~255
    return result


def decode_name(numbers):
    raw = b"".join((number & 0xffffffff).to_bytes(4, "big") for number in numbers)
    return bytes((byte - 128) & 255 for byte in raw[:-1]).split(b"\0", 1)[0].decode()


class DataFile:
    def __init__(self, path):
        raw = path.read_bytes()
        magic, version, size, swap, nt, ni, nd, item_size, data_size = struct.unpack_from("<4s8i", raw)
        if magic != b"DATA" or version != 4 or size + 16 != len(raw):
            raise ValueError(f"{path}: expected a complete DATA v4 map")
        offset = 36 + nt * 12
        item_offsets = struct.unpack_from(f"<{ni}i", raw, offset)
        offset += ni * 4
        data_offsets = struct.unpack_from(f"<{nd}i", raw, offset) + (data_size,)
        offset += nd * 4
        self.data_sizes = list(struct.unpack_from(f"<{nd}i", raw, offset))
        offset += nd * 4
        self.items = []
        for item_offset in item_offsets:
            type_id, length = struct.unpack_from("<II", raw, offset + item_offset)
            start = offset + item_offset + 8
            self.items.append([type_id >> 16, type_id & 65535, raw[start:start + length]])
        offset += item_size
        self.blocks = [raw[offset + data_offsets[i]:offset + data_offsets[i + 1]] for i in range(nd)]
        for i, block in enumerate(self.blocks):
            if len(zlib.decompress(block)) != self.data_sizes[i]:
                raise ValueError(f"{path}: bad data block {i}")

    def get(self, index):
        return zlib.decompress(self.blocks[index])

    def put(self, data, index=None):
        if index is None:
            index = len(self.blocks)
            self.blocks.append(b"")
            self.data_sizes.append(0)
        self.blocks[index] = zlib.compress(data, 9)
        self.data_sizes[index] = len(data)
        return index

    def typed(self, kind):
        return [item for item in self.items if item[0] == kind]

    def add(self, kind, payload):
        item = [kind, max((i[1] for i in self.typed(kind)), default=-1) + 1, payload]
        self.items.append(item)
        return item

    def save(self, path):
        ordered = sorted(self.items, key=lambda item: item[0])
        type_headers = []
        count = 0
        for kind, entries in itertools.groupby(ordered, key=lambda item: item[0]):
            entries = list(entries)
            type_headers += [kind, count, len(entries)]
            count += len(entries)
        item_offsets, item_blocks = [], bytearray()
        for kind, identifier, payload in ordered:
            item_offsets.append(len(item_blocks))
            item_blocks += struct.pack("<II", kind << 16 | identifier, len(payload)) + payload
        data_offsets, data_blocks = [], bytearray()
        for block in self.blocks:
            data_offsets.append(len(data_blocks))
            data_blocks += block
        tables = ints(type_headers + item_offsets + data_offsets + self.data_sizes)
        body = tables + item_blocks + data_blocks
        header = struct.pack("<4s8i", b"DATA", 4, len(body) + 20,
                             len(tables) + len(item_blocks) + 20,
                             len(type_headers) // 3, len(ordered), len(self.blocks), len(item_blocks), len(data_blocks))
        temp = path.with_suffix(".map.tmp")
        temp.write_bytes(header + body)
        DataFile(temp)  # Validate before replacing the source.
        temp.replace(path)


def remove_synthesized_samples(data):
    """Remove our old samples and their bytes while preserving unrelated indices."""
    original = data.typed(SOUND)
    removed = [item for item in original
               if data.get(values(item[2])[2]).rstrip(b"\0").decode() in REMOVED_SYNTH_SAMPLES]
    if not removed:
        return
    kept = [item for item in original if item not in removed]
    remap = {old: kept.index(item) for old, item in enumerate(original) if item in kept}
    # Our generated clips never had sound layers. Remap any unrelated mapper sources.
    for item in data.typed(LAYER):
        layer = values(item[2])
        if layer[1] in (9, 10) and layer[6] >= 0:
            if layer[6] not in remap:
                raise ValueError("A sound layer still references a removed synthesized sample")
            layer[6] = remap[layer[6]]
            item[2] = ints(layer)
    used = {index for item in kept for index in values(item[2])[2:4] if index >= 0}
    for item in removed:
        sample = values(item[2])
        for index in sample[2:4]:
            if index >= 0 and index not in used:
                # Keep indices stable but erase obsolete encoded audio/name contents.
                data.put(b"\0", index)
        data.items.remove(item)
    for index, item in enumerate(kept):
        item[1] = index


def embed(map_file, image_file, sounds):
    data = DataFile(map_file)
    remove_synthesized_samples(data)
    for sample in sounds:
        match = next((item for item in data.typed(SOUND)
                      if data.get(values(item[2])[2]).rstrip(b"\0").decode() == sample.stem), None)
        if match:
            sound = values(match[2])
            sound[1] = 0
            sound[3] = data.put(sample.read_bytes(), sound[3] if sound[3] >= 0 else None)
            sound[4] = sample.stat().st_size
            match[2] = ints(sound)
        else:
            name = data.put(sample.stem.encode() + b"\0")
            audio = data.put(sample.read_bytes())
            data.add(SOUND, ints([1, 0, name, audio, sample.stat().st_size]))
    # No sound sources: loading a map must never automatically play this pack.
    width, height = struct.unpack(">II", image_file.read_bytes()[16:24])
    rgba = subprocess.check_output(["ffmpeg", "-v", "error", "-i", str(image_file),
                                    "-f", "rawvideo", "-pix_fmt", "rgba", "-"])
    if len(rgba) != width * height * 4:
        raise ValueError("Invalid image decode")
    images = data.typed(IMAGE)
    match = next((item for item in images if data.get(values(item[2])[4]).rstrip(b"\0") == NAME.encode()), None)
    if match:
        image = values(match[2])
        image[1:4] = [width, height, 0]
        image[5] = data.put(rgba, image[5])
        match[2] = ints(image)
        image_index = images.index(match)
    else:
        image_index = len(images)
        data.add(IMAGE, ints([1, width, height, 0, data.put(NAME.encode() + b"\0"), data.put(rgba)]))

    envelopes = data.typed(ENVELOPE)
    if any(values(item[2])[0] > 2 for item in envelopes):
        raise ValueError("Bezier-envelope maps are not supported by this legacy project")
    env_item = next((item for item in envelopes if decode_name(values(item[2])[4:12]) == NAME), None)
    points_item = data.typed(ENVPOINTS)
    points_item = points_item[0] if points_item else data.add(ENVPOINTS, b"")
    # Instant appearance at the triggered clock, then a 1.28s linear fade.
    # Keep the final point exactly at the int32 limit as originally requested.
    points = ints([0, 0, 1024, 1024, 1024, 0,
                   JUMP_START, 1, 1024, 1024, 1024, 1024,
                   JUMP_START + JUMP_FADE, 0, 1024, 1024, 1024, 0,
                   INT_MAX, 0, 1024, 1024, 1024, 0])
    if env_item:
        envelope_index = envelopes.index(env_item)
        payload = values(env_item[2])
        if payload[3] == 4:
            start = payload[2]
            old_points = bytearray(points_item[2])
            old_points[start * 24:(start + 4) * 24] = points
            points_item[2] = bytes(old_points)
        else:
            # Migrate the old 3-point envelope without moving any mapper/gray
            # envelope's point indices. Subsequent runs reuse these 4 points.
            payload[2] = len(points_item[2]) // 24
            payload[3] = 4
            points_item[2] += points
            env_item[2] = ints(payload)
    else:
        envelope_index = len(envelopes)
        start = len(points_item[2]) // 24
        points_item[2] += points
        data.add(ENVELOPE, ints([2, 4, start, 4] + name_ints(NAME, 8) + [1]))

    # The square photograph is intentionally stretched to a mysterious 4:3.
    # Runtime color alpha (160/255) makes the whole quad semi-transparent.
    corners = [-800 * 1024, -600 * 1024, 800 * 1024, -600 * 1024,
               -800 * 1024, 600 * 1024, 800 * 1024, 600 * 1024, 0, 0]
    quad = ints(corners + [255, 255, 255, 160] * 4 +
                [0, 0, 1024, 0, 0, 1024, 1024, 1024] + [-1, 0, envelope_index, 0])
    layers, groups = data.typed(LAYER), data.typed(GROUP)
    existing = next((item for item in groups if len(values(item[2])) >= 15
                     and decode_name(values(item[2])[12:15]) == "AsylumFX"), None)
    if existing:
        layer = layers[values(existing[2])[5]]
        payload = values(layer[2])
        data.put(quad, payload[5])
        payload[6] = image_index
        layer[2] = ints(payload)
    else:
        quad_index = data.put(quad)
        layer_index = len(layers)
        data.add(LAYER, ints([0, 3, 0, 2, 1, quad_index, image_index] + name_ints("Jumpscare", 3)))
        # Appended foreground group, parallax 0: fixed to the viewport, not a tee.
        data.add(GROUP, ints([3, 0, 0, 0, 0, layer_index, 1, 0, 0, 0, 0, 0] + name_ints("AsylumFX", 3)))
    embed_time_stop_gray(data)
    data.save(map_file)
    print(f"{map_file.relative_to(ROOT)}: {len(data.typed(SOUND))} samples, 4:3 quad, {map_file.stat().st_size} bytes")


def embed_time_stop_gray(data):
    envelopes = data.typed(ENVELOPE)
    envelope = next((item for item in envelopes if decode_name(values(item[2])[4:12]) == GRAY_NAME), None)
    points_item = data.typed(ENVPOINTS)[0]
    points = ints([0, 0, 1024, 1024, 1024, 0,
                   GRAY_BASE, 1, 1024, 1024, 1024, 0,
                   GRAY_BASE + GRAY_WINDUP, 0, 1024, 1024, 1024, 1024,
                   GRAY_BASE + GRAY_END, 1, 1024, 1024, 1024, 1024,
                   GRAY_BASE + GRAY_END + 256, 0, 1024, 1024, 1024, 0,
                   INT_MAX, 0, 1024, 1024, 1024, 0])
    if envelope:
        index = envelopes.index(envelope)
        start = values(envelope[2])[2]
        updated = bytearray(points_item[2])
        updated[start * 24:(start + 6) * 24] = points
        points_item[2] = bytes(updated)
    else:
        index = len(envelopes)
        start = len(points_item[2]) // 24
        points_item[2] += points
        data.add(ENVELOPE, ints([2, 4, start, 6] + name_ints(GRAY_NAME, 8) + [1]))
    # Large viewport-relative flat grey quad. No generated image/texture needed.
    corners = [-32768 * 1024, -24576 * 1024, 32768 * 1024, -24576 * 1024,
               -32768 * 1024, 24576 * 1024, 32768 * 1024, 24576 * 1024, 0, 0]
    quad = ints(corners + [80, 80, 80, 120] * 4 + [0, 0, 1024, 0, 0, 1024, 1024, 1024] + [-1, 0, index, 0])
    group = next(item for item in data.typed(GROUP) if len(values(item[2])) >= 15
                 and decode_name(values(item[2])[12:15]) == "AsylumFX")
    existing = next((item for item in data.typed(LAYER) if values(item[2])[1] == 3
                     and decode_name(values(item[2])[7:10]) in ("TimeStopGray", "TimeStopGra", "TimeStop")), None)
    if existing:
        payload = values(existing[2])
        data.put(quad, payload[5])
        payload[7:10] = name_ints("TimeStop", 3)
        existing[2] = ints(payload)
    else:
        # Our foreground group is last. Its layers are appended contiguously.
        data.add(LAYER, ints([0, 3, 0, 2, 1, data.put(quad), -1] + name_ints("TimeStop", 3)))
        payload = values(group[2])
        payload[6] += 1
        group[2] = ints(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", nargs="*", type=pathlib.Path)
    args = parser.parse_args()
    maps = args.maps or sorted((ROOT / "data/maps").glob("*.map")) + sorted((ROOT / "data/maps7").glob("*.map"))
    for path in maps:
        embed(path.resolve(), ROOT / "assets/asylum/jumpscare-rock.png", sorted((ROOT / "assets/asylum/sounds").glob("*.opus")))


if __name__ == "__main__":
    main()
