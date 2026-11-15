"""Bethesda SE record serialization and canonical top-level group ordering."""

import struct as S

from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

def sub(sig,data):
    if len(data)>65535:raise ValueError('Unexpected oversized subrecord')
    return sig.encode()+S.pack('<H',len(data))+data

def record(sig,form,data,flags=0,version=44):
    return S.pack('<4sIIIIHH',sig.encode(),len(data),flags,form,0,version,0)+data

def text(s):return s.encode('ascii')+b'\0'

def group(label,kind,children):
    if isinstance(label,int):label=S.pack('<I',label)
    return S.pack('<4sI4siHHHH',b'GRUP',24+len(children),label,kind,0,0,0,0)+children

def emit_plugin(path,masters,groups,description,next_id):
    # HEDR count follows Bethesda/xEdit convention: all records and groups except TES4.
    def count(data):
        n=0;p=0
        while p<len(data):
            sig,size=S.unpack_from('<4sI',data,p);n+=1
            if sig==b'GRUP':n+=count(data[p+24:p+size]);p+=size
            else:p+=24+size
        assert p==len(data);return n
    body=b''.join(groups)
    header=sub('HEDR',S.pack('<fII',1.7,count(body),next_id))+sub('CNAM',text('Astral Sigil'))+sub('SNAM',text(description))
    for name in masters:header+=sub('MAST',text(name))+sub('DATA',bytes(8))
    path.write_bytes(record('TES4',0,header,0x200)+body)

def canonical_groups(blob):
    master=ROOT/'build/unified/Full/Data/Skyrim.esm'
    labels=[]
    with master.open('rb') as stream:
        while header:=stream.read(24):
            kind,size=S.unpack_from('<4sI',header)
            if kind==b'GRUP':labels.append(header[8:12]);stream.seek(size-24,1)
            else:stream.seek(size,1)
    size=S.unpack_from('<I',blob,4)[0]+24
    header=blob[:size];groups=[];position=size
    while position<len(blob):
        kind,size=S.unpack_from('<4sI',blob,position)
        assert kind==b'GRUP' and S.unpack_from('<I',blob,position+12)[0]==0
        label=blob[position+8:position+12];assert label in labels
        groups.append((label,blob[position:position+size]));position+=size
    assert position==len(blob) and len({k for k,b in groups})==len(groups)
    order=[k.decode() for k,b in groups]
    groups.sort(key=lambda item:labels.index(item[0]))
    after=[k.decode() for k,b in groups]
    assert after.index('IPCT')<after.index('IPDS')
    return header+b''.join(value for label,value in groups),{'before':order,'after':after}
