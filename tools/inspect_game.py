"""Read-only inventory of Bethesda records and BSA assets; never writes to the game."""
from pathlib import Path
import argparse
import ctypes as C
import json
import struct as S
import zlib


def subrecords(data):
    pos, extended = 0, None
    while pos < len(data):
        sig, size = S.unpack_from('<4sH', data, pos)
        pos += 6
        if sig == b'XXXX':
            extended = S.unpack_from('<I', data, pos)[0]
            pos += size
            continue
        if extended is not None:
            size, extended = extended, None
        if pos + size > len(data):
            raise ValueError('Truncated subrecord')
        yield sig.decode('ascii'), data[pos:pos+size]
        pos += size


def records(path, signatures=None):
    """Stream flat records across group boundaries, skipping unrelated top groups."""
    with open(path, 'rb') as f:
        while header := f.read(24):
            if len(header) != 24:
                raise ValueError('Truncated record header')
            sig, size, flags, form, rev, version, unknown = S.unpack('<4sIIIIHH', header)
            if sig == b'GRUP':
                if form == 0 and signatures and flags.to_bytes(4, 'little').decode('ascii') not in signatures:
                    f.seek(size-24, 1)
                continue
            data = f.read(size)
            if len(data) != size:
                raise ValueError('Truncated record')
            sig = sig.decode('ascii')
            if signatures and sig not in signatures:
                continue
            if flags & 0x40000:
                expected = S.unpack_from('<I', data)[0]
                data = zlib.decompress(data[4:])
                if len(data) != expected:
                    raise ValueError('Compressed record size mismatch')
            yield dict(signature=sig, form_id=form, flags=flags & ~0x40000,
                       version=version, data=data)


def ztext(b):
    return b.rstrip(b'\0').decode('utf-8', errors='replace')


class BSA:
    def __init__(self, path):
        self.path = Path(path)
        with self.path.open('rb') as f:
            magic, self.version, offset, self.flags, folders, count, _, names_size, _ = S.unpack('<4s8I', f.read(36))
            if magic != b'BSA\0' or self.version not in (104, 105):
                raise ValueError('Only Skyrim BSA 104/105 supported')
            f.seek(offset)
            counts = []
            for _ in range(folders):
                row = f.read(24 if self.version == 105 else 16)
                counts.append(S.unpack_from('<I', row, 8)[0])
            entries = []
            for n in counts:
                name = ztext(f.read(f.read(1)[0]))
                for _ in range(n):
                    _, size, off = S.unpack('<QII', f.read(16))
                    entries.append((name, size, off))
            filenames = f.read(names_size).split(b'\0')
            if len(entries) != count:
                raise ValueError('BSA entry count mismatch')
            self.entries = {(folder + '\\' + ztext(name)).lower(): (size, off)
                            for (folder, size, off), name in zip(entries, filenames)}

    def extract(self, name, lz4_dll=None):
        size, offset = self.entries[name.lower()]
        compressed = bool(self.flags & 4) ^ bool(size & 0x40000000)
        size &= 0x3fffffff
        with self.path.open('rb') as f:
            f.seek(offset)
            data = f.read(size)
        if self.flags & 0x100:
            data = data[1+data[0]:]
        if not compressed:
            return data
        length = S.unpack_from('<I', data)[0]
        if self.version == 104:
            out = zlib.decompress(data[4:])
        else:
            if not lz4_dll:
                raise ValueError('Provide an existing liblz4.dll for SSE archives')
            lib = C.CDLL(str(lz4_dll))
            lib.LZ4F_createDecompressionContext.argtypes = [C.POINTER(C.c_void_p), C.c_uint]
            lib.LZ4F_createDecompressionContext.restype = C.c_size_t
            lib.LZ4F_decompress.argtypes = [C.c_void_p, C.c_void_p, C.POINTER(C.c_size_t), C.c_void_p, C.POINTER(C.c_size_t), C.c_void_p]
            lib.LZ4F_decompress.restype = C.c_size_t
            lib.LZ4F_isError.argtypes = [C.c_size_t]
            lib.LZ4F_isError.restype = C.c_uint
            lib.LZ4F_freeDecompressionContext.argtypes = [C.c_void_p]
            ctx = C.c_void_p()
            code = lib.LZ4F_createDecompressionContext(C.byref(ctx), 100)
            if lib.LZ4F_isError(code):
                raise ValueError('LZ4 context error')
            try:
                src = C.create_string_buffer(data[4:])
                dst = C.create_string_buffer(length)
                sn, dn = C.c_size_t(len(data)-4), C.c_size_t(length)
                code = lib.LZ4F_decompress(ctx, dst, C.byref(dn), src, C.byref(sn), None)
                if lib.LZ4F_isError(code) or code != 0 or dn.value != length:
                    raise ValueError('LZ4 frame failed or incomplete')
                out = dst.raw
            finally:
                lib.LZ4F_freeDecompressionContext(ctx)
        if len(out) != length:
            raise ValueError('BSA decompressed size mismatch')
        return out


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--game', type=Path, required=True)
    p.add_argument('--output', type=Path, default=Path('.local/inventory.json'))
    args = p.parse_args()
    rows = []
    for r in records(args.game/'Data/Skyrim.esm', {'WEAP', 'ENCH', 'MGEF', 'ARTO', 'STAT', 'PROJ'}):
        subs = list(subrecords(r['data']))
        edid = next((ztext(v) for k, v in subs if k == 'EDID'), '')
        if ('staff' in edid.lower() and 'firebolt' in edid.lower()) or 'firebolt' in edid.lower() or 'fireball01hand' in edid.lower():
            rows.append(dict(type=r['signature'], id=f"{r['form_id']:08X}", edid=edid,
                             subrecords={k: ztext(v) if k in ('EDID', 'MODL') else v.hex() for k, v in subs}))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(rows, indent=2), encoding='utf-8')
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()
