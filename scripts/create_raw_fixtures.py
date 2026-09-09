"""Deterministic TIFF fixtures; only Python's standard library is required."""
import struct, zlib
from pathlib import Path

def make(path, w=37, h=29, orientation=1, tiled=False, pages=1, gray=False):
    data=bytearray(b'II'+struct.pack('<HI',42,0)); prev=4
    for page in range(pages):
        blocks=[]; offsets=[]; sizes=[]; bw=256 if gray else 16 if tiled else w; bh=256 if gray else 16 if tiled else 7
        for y in range(0,h,bh):
            for x in range(0,w,bw):
                rows=bh if tiled else min(bh,h-y)
                raw=bytearray()
                for yy in range(rows):
                    for xx in range(bw):
                        sx,sy=x+xx,y+yy
                        if gray: raw.append(((sx//32+sy//32)%2)*150+50)
                        else: raw.extend(((sx*7+page*11)%256,(sy*9)%256,(sx+sy*3)%256))
                blob=zlib.compress(raw) if gray else raw
                offsets.append(len(data)); sizes.append(len(blob)); data.extend(blob)
        tags={256:(4,[w]),257:(4,[h]),258:(3,[8] if gray else [8,8,8]),259:(3,[8 if gray else 1]),262:(3,[1 if gray else 2]),274:(3,[orientation]),277:(3,[1 if gray else 3]),284:(3,[1])}
        if tiled: tags.update({322:(4,[bw]),323:(4,[bh]),324:(4,offsets),325:(4,sizes)})
        else: tags.update({273:(4,offsets),278:(4,[bh]),279:(4,sizes)})
        entries=[]
        for tag,(typ,vals) in sorted(tags.items()):
            raw=struct.pack('<'+('H' if typ==3 else 'I')*len(vals),*vals)
            if len(raw)>4:
                ptr=len(data); data.extend(raw); raw=struct.pack('<I',ptr)
            entries.append(struct.pack('<HHI',tag,typ,len(vals))+raw.ljust(4,b'\0'))
        if len(data)%2: data.append(0)
        ptr=len(data); struct.pack_into('<I',data,prev,ptr)
        data.extend(struct.pack('<H',len(entries))+b''.join(entries)); prev=len(data); data.extend(b'\0'*4)
    path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)

def generate_fixtures():
    out=Path(__file__).resolve().parent.parent/'src/androidTest/assets/raw'
    for orientation in range(1,9):
        for tiled in (False,True): make(out/f'{"tile" if tiled else "strip"}-{orientation}.tif',orientation=orientation,tiled=tiled)
    make(out/'pages.tif',pages=3)
    make(out/'pixel.tif',w=1,h=1)

if __name__ == '__main__':
    generate_fixtures()
