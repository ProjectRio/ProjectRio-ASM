"""Read/patch/extend a GameCube DOL.

Header layout (0x100 bytes) -- text[7] then data[11] are contiguous, so all
18 sections index one flat array:

    0x00  u32 offset[18]     file offset of each section (0 = slot unused)
    0x48  u32 address[18]    load address
    0x90  u32 size[18]       byte size
    0xD8  u32 bssAddress
    0xDC  u32 bssSize
    0xE0  u32 entryPoint

Indices 0-6 are TEXT (executable), 7-17 are DATA. There is no relocation:
each section is loaded verbatim at its own address, so adding one cannot move
anything that already exists.
"""
import struct

TEXT_SLOTS = 7
DATA_SLOTS = 11
N_SLOTS = TEXT_SLOTS + DATA_SLOTS

OFF_OFFSETS = 0x00
OFF_ADDRS = 0x48
OFF_SIZES = 0x90
OFF_BSS_ADDR = 0xD8
OFF_BSS_SIZE = 0xDC
OFF_ENTRY = 0xE0


class Dol:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.raw = bytearray(f.read())
        self.path = path

    # ---- header accessors -------------------------------------------------
    def _u32(self, off):
        return struct.unpack_from(">I", self.raw, off)[0]

    def _set_u32(self, off, val):
        struct.pack_into(">I", self.raw, off, val & 0xFFFFFFFF)

    def offset(self, i):
        return self._u32(OFF_OFFSETS + i * 4)

    def address(self, i):
        return self._u32(OFF_ADDRS + i * 4)

    def size(self, i):
        return self._u32(OFF_SIZES + i * 4)

    @property
    def entry(self):
        return self._u32(OFF_ENTRY)

    @property
    def bss(self):
        return self._u32(OFF_BSS_ADDR), self._u32(OFF_BSS_SIZE)

    def sections(self):
        """[(index, kind, fileOffset, address, size)] for every used slot."""
        out = []
        for i in range(N_SLOTS):
            if self.size(i):
                kind = "text" if i < TEXT_SLOTS else "data"
                out.append((i, kind, self.offset(i), self.address(i), self.size(i)))
        return out

    def free_text_slots(self):
        return [i for i in range(TEXT_SLOTS) if not self.size(i)]

    def free_data_slots(self):
        return [i for i in range(TEXT_SLOTS, N_SLOTS) if not self.size(i)]

    # ---- address <-> file offset -----------------------------------------
    def addr_to_offset(self, addr):
        for i, _kind, off, a, size in self.sections():
            if a <= addr < a + size:
                return off + (addr - a)
        raise ValueError(f"0x{addr:08X} is not inside any DOL section")

    def read_word(self, addr):
        return self._u32(self.addr_to_offset(addr))

    def read(self, addr, n):
        o = self.addr_to_offset(addr)
        return bytes(self.raw[o:o + n])

    # ---- mutation ---------------------------------------------------------
    def patch_word(self, addr, new, expect=None):
        """Write one instruction/word. `expect` guards against patching an
        already-patched DOL or a wrong address -- always pass it."""
        off = self.addr_to_offset(addr)
        cur = self._u32(off)
        if expect is not None and cur != expect:
            raise ValueError(
                f"0x{addr:08X}: expected {expect:08X}, found {cur:08X}. "
                f"Refusing to patch (is this DOL already patched?)")
        self._set_u32(off, new)
        return cur

    def set_entry(self, addr):
        """Repoint the DOL entry point. BS2/the apploader jumps here instead of
        the game's __start, which is how a loader stub gets to run first."""
        self._set_u32(OFF_ENTRY, addr)

    def add_section(self, addr, data, kind="text", align=32):
        """Append `data` to the file and point a free section slot at it.
        Returns the slot index. Nothing existing moves -- DOL sections are
        independently addressed."""
        pool = self.free_text_slots() if kind == "text" else self.free_data_slots()
        if not pool:
            raise RuntimeError(f"no free {kind} section slots")
        slot = pool[0]

        while len(self.raw) % align:
            self.raw.append(0)
        file_off = len(self.raw)
        self.raw += data
        while len(self.raw) % align:
            self.raw.append(0)

        size = len(self.raw) - file_off
        self._set_u32(OFF_OFFSETS + slot * 4, file_off)
        self._set_u32(OFF_ADDRS + slot * 4, addr)
        self._set_u32(OFF_SIZES + slot * 4, size)
        return slot

    def save(self, path):
        with open(path, "wb") as f:
            f.write(self.raw)


# ---- PPC instruction encoders --------------------------------------------
def lis(rD, imm16):
    """lis rD, imm  ==  addis rD, r0, imm"""
    return (15 << 26) | (rD << 21) | (0 << 16) | (imm16 & 0xFFFF)


def addi(rD, rA, simm16):
    return (14 << 26) | (rD << 21) | (rA << 16) | (simm16 & 0xFFFF)


def ori(rD, rA, uimm16):
    return (24 << 26) | (rA << 21) | (rD << 16) | (uimm16 & 0xFFFF)


def branch(frm, to, link=False):
    d = to - frm
    if not (-0x2000000 <= d < 0x2000000):
        raise ValueError(f"branch 0x{frm:08X} -> 0x{to:08X} out of range")
    return (18 << 26) | (d & 0x03FFFFFC) | (1 if link else 0)


def bl(frm, to):
    return branch(frm, to, link=True)


def mr(rD, rS):
    """mr rD, rS  ==  or rD, rS, rS"""
    return (31 << 26) | (rS << 21) | (rD << 16) | (rS << 11) | (444 << 1)

