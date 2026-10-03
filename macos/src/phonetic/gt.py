#!/usr/bin/env python3
"""Ground truth: type literal text through a .keylayout on the REAL macOS UCKeyTranslate.
usage: gt.py LAYOUT TESTFILE      TESTFILE lines:  typed-text<TAB>expected   (# = comment)
       typed text is literal US-QWERTY input; '\\n' not supported; a trailing space flushes.
Prints PASS/FAIL per line, plus the output and the trailing dead-key state."""
import sys, os, ctypes, ctypes.util, time, unicodedata, re, secrets

Carbon = ctypes.cdll.LoadLibrary("/System/Library/Frameworks/Carbon.framework/Carbon")
CF = ctypes.cdll.LoadLibrary(ctypes.util.find_library("CoreFoundation"))
CF.CFStringCreateWithCString.restype = ctypes.c_void_p
CF.CFStringCreateWithCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint32]
CF.CFURLCreateWithFileSystemPath.restype = ctypes.c_void_p
CF.CFURLCreateWithFileSystemPath.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_bool]
CF.CFArrayGetCount.restype = ctypes.c_long; CF.CFArrayGetCount.argtypes = [ctypes.c_void_p]
CF.CFArrayGetValueAtIndex.restype = ctypes.c_void_p; CF.CFArrayGetValueAtIndex.argtypes = [ctypes.c_void_p, ctypes.c_long]
CF.CFStringGetCStringPtr.restype = ctypes.c_char_p; CF.CFStringGetCStringPtr.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
CF.CFStringGetCString.restype = ctypes.c_bool; CF.CFStringGetCString.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_long, ctypes.c_uint32]
CF.CFDataGetBytePtr.restype = ctypes.c_void_p; CF.CFDataGetBytePtr.argtypes = [ctypes.c_void_p]
CF.CFDataGetLength.restype = ctypes.c_long; CF.CFDataGetLength.argtypes = [ctypes.c_void_p]
U8 = 0x08000100
Carbon.TISCreateInputSourceList.restype = ctypes.c_void_p; Carbon.TISCreateInputSourceList.argtypes = [ctypes.c_void_p, ctypes.c_bool]
Carbon.TISRegisterInputSource.restype = ctypes.c_int; Carbon.TISRegisterInputSource.argtypes = [ctypes.c_void_p]
Carbon.TISGetInputSourceProperty.restype = ctypes.c_void_p; Carbon.TISGetInputSourceProperty.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
Carbon.LMGetKbdType.restype = ctypes.c_uint8
Carbon.UCKeyTranslate.restype = ctypes.c_int32
Carbon.UCKeyTranslate.argtypes = [ctypes.c_void_p, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint32, ctypes.c_uint32,
                                  ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32), ctypes.c_ulong,
                                  ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_uint16)]

def cfstr(s): return ctypes.c_void_p(CF.CFStringCreateWithCString(None, s.encode(), U8))
def topy(p):
    if not p: return None
    q = CF.CFStringGetCStringPtr(p, U8)
    if q: return q.decode()
    b = ctypes.create_string_buffer(512)
    return b.value.decode() if CF.CFStringGetCString(p, b, 512, U8) else None
kName = ctypes.c_void_p.in_dll(Carbon, "kTISPropertyLocalizedName")
kData = ctypes.c_void_p.in_dll(Carbon, "kTISPropertyUnicodeKeyLayoutData")

# US-QWERTY: char -> (virtual key, shift)
BASE = {'a':0,'s':1,'d':2,'f':3,'h':4,'g':5,'z':6,'x':7,'c':8,'v':9,'b':11,'q':12,'w':13,'e':14,'r':15,
        'y':16,'t':17,'1':18,'2':19,'3':20,'4':21,'6':22,'5':23,'=':24,'9':25,'7':26,'-':27,'8':28,'0':29,
        ']':30,'o':31,'u':32,'[':33,'i':34,'p':35,'l':37,'j':38,"'":39,'k':40,';':41,'\\':42,',':43,'/':44,
        'n':45,'m':46,'.':47,'`':50,' ':49}
SHIFTED = {'!':'1','@':'2','#':'3','$':'4','%':'5','^':'6','&':'7','*':'8','(':'9',')':'0','_':'-','+':'=',
           '{':'[','}':']','|':'\\',':':';','"':"'",'<':',','>':'.','?':'/','~':'`'}
def keys_for(text):
    out = []
    for ch in text:
        if ch in BASE: out.append((BASE[ch], 0))
        elif ch.isalpha() and ch.lower() in BASE: out.append((BASE[ch.lower()], 2))
        elif ch in SHIFTED: out.append((BASE[SHIFTED[ch]], 2))
        else: raise ValueError("cannot type %r" % ch)
    return out

def load(path):
    uniq = "GT_" + secrets.token_hex(8)               # random: parallel runs never collide
    dest = os.path.expanduser("~/Library/Keyboard Layouts/%s.keylayout" % uniq)
    with open(path, encoding="utf-8") as f:
        t = f.read()
    t = re.sub(r'name="[^"]*"', 'name="%s"' % uniq, t, count=1)
    t = re.sub(r'id="-?\d+"', 'id="-%d"' % (20000 + secrets.randbelow(9000)), t, count=1)
    # O_EXCL|O_NOFOLLOW: never overwrite an existing file or write through a symlink
    fd = os.open(dest, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o644)
    try:                                              # cleanup covers write/register/sleep too
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(t)
        Carbon.TISRegisterInputSource(CF.CFURLCreateWithFileSystemPath(None, cfstr(dest), 0, False))
        time.sleep(0.5)
        arr = Carbon.TISCreateInputSourceList(None, True)
        for i in range(CF.CFArrayGetCount(arr)):
            s = CF.CFArrayGetValueAtIndex(arr, i)
            if topy(Carbon.TISGetInputSourceProperty(s, kName)) == uniq:
                d = Carbon.TISGetInputSourceProperty(s, kData)
                if not d: continue
                n = CF.CFDataGetLength(d); b = ctypes.create_string_buffer(n)
                ctypes.memmove(b, CF.CFDataGetBytePtr(d), n)
                return b
        return None
    finally:
        os.remove(dest)

def type_text(u, text):
    ds = ctypes.c_uint32(0); kt = Carbon.LMGetKbdType(); out = []
    for vk, mods in keys_for(text):
        buf = (ctypes.c_uint16 * 64)(); ln = ctypes.c_ulong(0)
        Carbon.UCKeyTranslate(u, vk, 0, mods, kt, 0, ctypes.byref(ds), 64, ctypes.byref(ln), buf)
        out.append(bytes(buf)[:2 * ln.value].decode("utf-16-le"))
    return "".join(out), ds.value

def main():
    layout, tests = sys.argv[1], sys.argv[2]
    u = load(layout)
    if u is None:
        print("LOAD FAIL — macOS rejected the layout"); sys.exit(2)
    npass = nfail = 0
    for line in open(tests, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"): continue
        typed, _, want = line.partition("\t")
        got, dead = type_text(u, typed if typed.endswith(" ") else typed + " ")  # Space ends the word
        norm = lambda s: unicodedata.normalize("NFC", s).rstrip(" ")   # trailing space = flush key
        ok = norm(got) == norm(want)
        npass += ok; nfail += (not ok)
        print("%s  %-22r -> %-18r %s" % ("PASS" if ok else "FAIL", typed, got,
              "" if ok else "(want %r, dead=%d)" % (want, dead)))
    print("\n%d passed, %d failed" % (npass, nfail))
    sys.exit(1 if nfail else 0)

if __name__ == "__main__":
    main()
