#!/usr/bin/env python3
"""Parse libdragon DCA5 and try LZ4 block decompress."""
from pathlib import Path

def read_varint(buf, i):
    x = 0
    shift = 0
    while True:
        b = buf[i]
        i += 1
        x |= (b & 0x7F) << shift
        if not (b & 0x80):
            return x, i
        shift += 7

p = Path(r"C:/Users/PC/Documents/pyrite64/testdc/filesystem/box.t3dm")
d = p.read_bytes()
print("file", len(d), d[:4])
assert d[:3] == b"DCA" and d[3] == ord("5")
flags = d[4]
i = 5
cmp_size, i = read_varint(d, i)
orig_size, i = read_varint(d, i)
margin, i = read_varint(d, i)
if i & 1:
    i += 1
print("flags", hex(flags), "algo", (flags >> 4) & 3, "cmp", cmp_size, "orig", orig_size, "hdr", i)
payload = d[i:]
print("payload", len(payload), payload[:16].hex())
try:
    import lz4.block
    out = lz4.block.decompress(payload, uncompressed_size=orig_size)
    print("lz4.block OK", len(out), out[:4])
    Path(r"C:/Users/PC/Documents/pyrite64/testdc/build-dc/box_raw.t3dm").write_bytes(out)
except Exception as e:
    print("lz4.block failed", e)
    try:
        from lz4 import frame
        out = frame.decompress(payload)
        print("lz4.frame OK", len(out), out[:4])
    except Exception as e2:
        print("lz4.frame failed", e2)
