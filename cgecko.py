#!/usr/bin/env python3
"""cgecko.py — Converts a .c or .asm file into a C2/C3 Gecko code for GameCube modding.
See README.md for full documentation."""

import sys
import os
import re
import struct
import subprocess
import tempfile
import shutil
import argparse
import json
from typing import NoReturn

# ==============================================================================
# CONFIGURATION
# ==============================================================================

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

DEVKITPPC_BIN = os.path.normpath(os.path.join(
    os.environ.get("DEVKITPPC", os.path.join("C:\\devkitpro", "devkitPPC")),
    "bin"
))

def tool(name: str) -> str:
    exe = name + ".exe" if sys.platform == "win32" else name
    return os.path.join(DEVKITPPC_BIN, exe)

GCC     = tool("powerpc-eabi-gcc")
AS      = tool("powerpc-eabi-as")
LD      = tool("powerpc-eabi-ld")
OBJCOPY = tool("powerpc-eabi-objcopy")
OBJDUMP = tool("powerpc-eabi-objdump")
READELF = tool("powerpc-eabi-readelf")

# -ffixed-r14 through -ffixed-r31: prevent GCC from using callee-saved registers.
# Since our BACKUP/RESTORE already saves all GPRs, GCC's own prologue/epilogue
# for these registers is redundant. Marking them as fixed eliminates the
# compiler-generated sub-frame entirely — GCC only uses r3-r13 (caller-saved),
# which it never saves/restores, producing cleaner minimal output.
FIXED_REGS = [f"-ffixed-r{n}" for n in range(14, 32)]

GCC_FLAGS = [
    "-DGEKKO",
    "-mogc",
    "-mcpu=750",
    "-meabi",
    "-mhard-float",
    "-fomit-frame-pointer",
    "-ffunction-sections",
    "-fno-asynchronous-unwind-tables",
    "-fno-optimize-sibling-calls",
    "-Qn",
    "-Oz",
    "-Wno-attributes",
    f"-I{SCRIPT_DIR}",
    "-c",
] + FIXED_REGS

AS_FLAGS = [
    "-mregnames",
    "-mbig",
    f"-I{SCRIPT_DIR}",
]

# ==============================================================================
# CONSTANTS
# ==============================================================================

BLR_INSTR  = 0x4E800020
NOP_INSTR  = 0x60000000
TERMINATOR = 0x00000000
B_BASE     = 0x48000000

GPR_SAVE_OFFSET = 0x8
FPR_BASE_OFFSET = 0x88
FPR_SLOT_SIZE   = 8
FRAME_MIN       = 0x90

MFLR_R0 = (31 << 26) | (0 << 21) | (8 << 16) | (339 << 1)
MTLR_R0 = (31 << 26) | (0 << 21) | (8 << 16) | (467 << 1)

# ==============================================================================
# RAW GECKO INI PARSING
# ==============================================================================

_HEX_PAIR = re.compile(r'^[0-9A-Fa-f]{8} [0-9A-Fa-f]{8}$')

def parse_gecko_blocks(source: str) -> list[tuple[str, str]]:
    """Parse raw gecko code blocks from a .ini source file.
    Each block starts with a $Name header line. Hex lines are validated;
    unrecognized lines trigger a warning and are skipped.
    Returns a list of (name, gecko_code_string) pairs."""
    blocks: list[tuple[str, str]] = []
    header: str | None = None
    lines:  list[str]  = []

    def _flush():
        if header is None:
            return
        if not any(_HEX_PAIR.match(l) for l in lines if not l.startswith("*")):
            warn(f"Block '{header}' has no valid hex lines — skipping.")
            return
        h     = header.lstrip("$")
        bracket = h.find("[")
        name  = (h[:bracket] if bracket != -1 else h).strip()
        blocks.append((name, "\n".join([header] + lines)))

    for lineno, raw in enumerate(source.splitlines(), 1):
        stripped = raw.strip()
        if not stripped:
            continue
        if stripped.startswith("$"):
            _flush()
            header = stripped
            lines  = []
        elif header is None:
            warn(f"Line {lineno}: data before any $header — skipped.")
        elif stripped.startswith("*") or _HEX_PAIR.match(stripped):
            lines.append(stripped)
        else:
            warn(f"Line {lineno}: unrecognized '{stripped}' — skipped.")

    _flush()
    return blocks

# ==============================================================================
# COMMENT PARSING
# ==============================================================================

def _pat(key: str, value_re: str, flags=re.IGNORECASE | re.MULTILINE) -> re.Pattern:
    return re.compile(rf"(?://|#)\s*{key}\s*:\s*{value_re}", flags)

def _note_pat() -> re.Pattern:
    return re.compile(r"(?://|#)\s*(\*[^\n]*)", re.MULTILINE)

ADDRESS_PATTERN     = _pat("Address",     r"(0x[0-9A-Fa-f]{8})")
DATA_PATTERN        = _pat("Data",        r"(0x[0-9A-Fa-f]{8})")
AUTHOR_PATTERN      = _pat("Author",      r"(.+?)(?:\n|$)")
INSTRUCTION_PATTERN = _pat("Instruction", r"(.+?)(?:\n|$)")
STATE_PATTERN       = _pat("State",       r"(.+?)(?:\n|$)")
NOTE_PATTERN        = _note_pat()

def make_func_pattern(func_name: str) -> re.Pattern:
    return re.compile(
        r"^([^\S\n]*)"
        r"(?:__attribute__\s*\(\s*\(\s*naked\s*\)\s*\)\s*)?"
        r"((?:unsigned\s+)?(?:int|void|float|double|void\s*\*))"
        rf"\s+{re.escape(func_name)}\s*\(([^)]*)\)",
        re.MULTILINE
    )

def parse_address(source: str) -> int:
    matches = ADDRESS_PATTERN.findall(source)
    if not matches:
        die("No 'Address: 0x80XXXXXX' comment found in source file.")
    if len(matches) > 1:
        die(f"Multiple Address comments found: {matches}. Only one is allowed.")
    addr = int(matches[0], 16)
    if addr % 4 != 0:
        die(f"Address {hex(addr)} is not 4-byte aligned.")
    if not (0x80000000 <= addr <= 0x81FFFFFF):
        warn(f"Address {hex(addr)} is outside typical GameCube RAM (0x80000000-0x81FFFFFF).")
    return addr

def parse_author(source: str) -> str | None:
    m = AUTHOR_PATTERN.search(source)
    return m.group(1).strip() if m else None

def parse_instruction(source: str) -> str | None:
    m = INSTRUCTION_PATTERN.search(source)
    return m.group(1).strip() if m else None

# State values map to Project Rio scene IDs
STATE_MAP = {
    "boot": (0x280E877C, 0x00000000),
    "0":    (0x280E877C, 0x00000000),
    "menu": (0x280E877C, 0x00000004),
    "4":    (0x280E877C, 0x00000004),
    "game": (0x280E877C, 0x00000005),
    "5":    (0x280E877C, 0x00000005),
}

def parse_state(source: str) -> tuple[int, int] | None:
    m = STATE_PATTERN.search(source)
    if not m:
        return None
    key = m.group(1).strip().lower()
    if key not in STATE_MAP:
        die(f"Unknown State value '{key}'. Expected: boot, menu, game, 0, 4, or 5.")
    return STATE_MAP[key]

def parse_data_address(source: str) -> int | None:
    m = DATA_PATTERN.search(source)
    if not m:
        return None
    addr = int(m.group(1), 16)
    if addr % 4 != 0:
        die(f"Data address {hex(addr)} is not 4-byte aligned.")
    if not (0x80000000 <= addr <= 0x81FFFFFF):
        warn(f"Data address {hex(addr)} is outside typical GameCube RAM (0x80000000-0x81FFFFFF).")
    return addr

def parse_notes(source: str) -> list[str]:
    return [m.group(1).strip() for m in NOTE_PATTERN.finditer(source)]

# ==============================================================================
# INSTRUCTION ASSEMBLY
# ==============================================================================

def assemble_instruction(asm_text: str, tmpdir: str) -> int:
    """Assemble a single PPC instruction string to its 4-byte integer encoding."""
    asm_src = os.path.join(tmpdir, "instr.s")
    asm_obj = os.path.join(tmpdir, "instr.o")
    asm_bin = os.path.join(tmpdir, "instr.bin")

    with open(asm_src, "w") as f:
        f.write(f".text\n{asm_text}\n")

    result = subprocess.run(
        [AS] + AS_FLAGS + [asm_src, "-o", asm_obj],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        die(f"Failed to assemble instruction '{asm_text}':\n{result.stderr}")

    result = subprocess.run(
        [OBJCOPY, "-O", "binary", "--only-section", ".text", asm_obj, asm_bin],
        capture_output=True
    )
    if result.returncode != 0 or not os.path.isfile(asm_bin):
        die(f"Failed to extract bytes for '{asm_text}'.")

    with open(asm_bin, "rb") as f:
        data = f.read()

    if len(data) < 4:
        die(f"Instruction '{asm_text}' assembled to {len(data)} bytes (expected 4).")
    return struct.unpack(">I", data[:4])[0]

# ==============================================================================
# OUTPUT PATH & CONFIG
# ==============================================================================

CONFIG_PATH = os.path.join(SCRIPT_DIR, "config.json")
TXT_PATH    = os.path.join(SCRIPT_DIR, "codes.txt")

def load_config() -> dict:
    if os.path.isfile(CONFIG_PATH):
        with open(CONFIG_PATH, "r") as f:
            return json.load(f)
    return {}

def get_ini_path()     -> str | None: return load_config().get("ini_path")
def get_build_file()   -> str | None: return load_config().get("build_file")
def get_dolphin_path() -> str | None: return load_config().get("dolphin_path")
def get_iso_path()     -> str | None: return load_config().get("iso_path")
def get_launch()       -> bool:       return bool(load_config().get("launch dolphin", False))

# ==============================================================================
# GECKO OUTPUT FORMATTING
# ==============================================================================

def build_gecko_output(code_lines: list[str],
                       name:        str,
                       author:      str | None,
                       notes:       list[str],
                       cond_value:  int | None,
                       cond_addr:   int | None) -> str:
    out_lines = []

    header = f"${name}"
    if author is not None:
        header += f" [{author}]"
    out_lines.append(header)

    if cond_value is not None and cond_addr is not None:
        out_lines.append(f"{cond_addr:08X} {cond_value:08X}")

    out_lines.extend(code_lines)

    if cond_value is not None:
        out_lines.append("E2000001 00000000")

    for note in notes:
        out_lines.append(note)

    return "\n".join(out_lines)


def format_c2(inject_addr: int, payload: bytes) -> list[str]:
    assert len(payload) % 8 == 0
    code_type = 0xC3 if inject_addr >= 0x81000000 else 0xC2
    if code_type == 0xC3:
        print("[INFO] Address >= 0x81000000 — using C3 code type.")
    header = (code_type << 24) | (inject_addr & 0x00FFFFFF)
    lines  = [f"{header:08X} {len(payload)//8:08X}"]
    for i in range(0, len(payload), 8):
        w1, w2 = struct.unpack(">II", payload[i:i+8])
        lines.append(f"{w1:08X} {w2:08X}")
    return lines


def format_04(inject_addr: int, instr: int) -> list[str]:
    header = (0x04 << 24) | (inject_addr & 0x00FFFFFF)
    return [f"{header:08X} {instr:08X}"]


def format_06(data_addr: int, data: bytes) -> list[str]:
    """Format an 06 RAM write gecko code. Pads data to 8-byte boundary."""
    byte_count = len(data)
    if len(data) % 8 != 0:
        data = data + b"\x00" * (8 - len(data) % 8)
    header = (0x06 << 24) | (data_addr & 0x00FFFFFF)
    lines  = [f"{header:08X} {byte_count:08X}"]
    for i in range(0, len(data), 8):
        w1, w2 = struct.unpack(">II", data[i:i+8])
        lines.append(f"{w1:08X} {w2:08X}")
    return lines

# ==============================================================================
# TOOL VERIFICATION
# ==============================================================================

def check_tools(need_gcc: bool):
    candidates = [AS, LD, OBJCOPY, OBJDUMP, READELF]
    if need_gcc:
        candidates.append(GCC)
    missing = [t for t in candidates if not os.path.isfile(t)]
    if missing:
        print("[ERROR] devkitPPC tools not found:", file=sys.stderr)
        for t in missing:
            print(f"        {t}", file=sys.stderr)
        sys.exit(1)

# ==============================================================================
# LINKER SCRIPT
# ==============================================================================

def make_linker_script(func_name: str, data_addr: int | None = None) -> str:
    """Entry function section placed first, then all other text sections.
    If data_addr is given, .rodata/.data are placed at that fixed address so
    absolute references in .text resolve correctly."""
    data_origin = f"    . = {data_addr:#010x};\n" if data_addr is not None else ""
    return (
        "SECTIONS {\n"
        "    . = 0x00000000;\n"
        "    .text : {\n"
        f"        *(.text.{func_name})\n"
        "        *(.text*)\n"
        "        *(.init*)\n"
        "    }\n"
        "    . = ALIGN(4);\n"
        + data_origin +
        "    .rodata : { *(.rodata*) }\n"
        "    . = ALIGN(4);\n"
        "    .data : { *(.data*) *(.sdata*) }\n"
        "    . = ALIGN(4);\n"
        "    .bss (NOLOAD) : { *(.bss*) *(.sbss*) }\n"
        "    /DISCARD/ : {\n"
        "        *(.comment) *(.gnu.attributes)\n"
        "        *(.eh_frame*) *(.pdr)\n"
        "    }\n"
        "}\n"
    )

# ==============================================================================
# COMPILATION & ASSEMBLY & LINKING
# ==============================================================================

def compile_c(c_path: str, obj_path: str, debug: bool):
    cmd = [GCC] + GCC_FLAGS + [c_path, "-o", obj_path]
    if debug:
        print(f"[DEBUG] Compile: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stderr.strip():
        print("[COMPILER]\n" + result.stderr)
    if result.returncode != 0:
        die("Compilation failed.")


def assemble_asm(asm_path: str, obj_path: str, debug: bool):
    cmd = [AS] + AS_FLAGS + [asm_path, "-o", obj_path]
    if debug:
        print(f"[DEBUG] Assemble: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stderr.strip():
        print("[ASSEMBLER]\n" + result.stderr)
    if result.returncode != 0:
        die("Assembly failed.")


def get_libgcc() -> str:
    """Ask GCC where its libgcc.a is for linking."""
    result = subprocess.run(
        [GCC, "-mcpu=750", "-meabi", "-mhard-float", "-print-libgcc-file-name"],
        capture_output=True, text=True
    )
    return result.stdout.strip()


def link_elf(obj_path: str, elf_path: str, ld_path: str, debug: bool):
    libgcc = get_libgcc()
    cmd    = [LD, "-T", ld_path, "--nostdlib", obj_path, libgcc, "-o", elf_path]
    if debug:
        print(f"[DEBUG] Link: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.stderr.strip():
        print("[LINKER]\n" + result.stderr)
    if result.returncode != 0:
        die("Linking failed.")

# ==============================================================================
# SECTION EXTRACTION
# ==============================================================================

def extract_section(obj_path: str, section: str) -> bytes:
    tmp = obj_path + ".sec.tmp"
    try:
        result = subprocess.run(
            [OBJCOPY, "-O", "binary", "--only-section", section, obj_path, tmp],
            capture_output=True
        )
        if result.returncode != 0 or not os.path.isfile(tmp):
            return b""
        with open(tmp, "rb") as f:
            return f.read()
    except Exception:
        return b""
    finally:
        if os.path.isfile(tmp):
            os.remove(tmp)

# ==============================================================================
# RELOCATION SAFETY CHECK
# ==============================================================================

UNSAFE_RELOCS = {
    "R_PPC_ADDR32", "R_PPC_ADDR16_LO", "R_PPC_ADDR16_HI",
    "R_PPC_ADDR16_HA", "R_PPC_ADDR14", "R_PPC_UADDR32",
}

def check_relocations(obj_path: str, debug: bool):
    try:
        result = subprocess.run(
            [READELF, "-r", obj_path],
            capture_output=True, text=True, check=True
        )
    except subprocess.CalledProcessError as e:
        warn(f"Could not inspect relocations: {e.stderr.strip()}")
        return
    if debug:
        print("[DEBUG] Relocations:\n" + result.stdout)
    bad = [l.strip() for l in result.stdout.splitlines()
           if len(l.split()) >= 3 and l.split()[2] in UNSAFE_RELOCS]
    if bad:
        warn("Unsafe absolute relocations found — will be wrong at runtime.")
        for b in bad:
            warn(f"  {b}")
        die("Refusing to emit gecko code with unsafe relocations.")

# ==============================================================================
# FPU DETECTION
# ==============================================================================

FP_OPCODES = {48, 49, 50, 51, 52, 53, 54, 55, 59, 63}

def detect_used_fprs(text: bytes, extra_fprs: set[int], debug: bool) -> set[int]:
    used = set(extra_fprs)
    for i in range(len(text) // 4):
        word   = struct.unpack_from(">I", text, i * 4)[0]
        opcode = (word >> 26) & 0x3F
        fpr    = (word >> 21) & 0x1F
        if opcode in FP_OPCODES:
            used.add(fpr)
    if used:
        names = ", ".join(f"f{n}" for n in sorted(used))
        print(f"[INFO] FPU registers: {names}")
    return used

# ==============================================================================
# BACKUP / RESTORE
# ==============================================================================

def compute_frame_size(used_fprs: set[int]) -> int:
    if not used_fprs:
        return FRAME_MIN
    raw = FPR_BASE_OFFSET + (max(used_fprs) + 1) * FPR_SLOT_SIZE
    return max((raw + 15) & ~15, FRAME_MIN)

def _stfd(f, o): return (54 << 26) | (f << 21) | (1 << 16) | (o & 0xFFFF)
def _lfd (f, o): return (50 << 26) | (f << 21) | (1 << 16) | (o & 0xFFFF)
def _stw (r, o): return (36 << 26) | (r << 21) | (1 << 16) | (o & 0xFFFF)
def _lwz (r, o): return (32 << 26) | (r << 21) | (1 << 16) | (o & 0xFFFF)
def _stwu(r, o): return (37 << 26) | (r << 21) | (1 << 16) | (o & 0xFFFF)
def _addi(d, s, i): return (14 << 26) | (d << 21) | (s << 16) | (i & 0xFFFF)
def _stmw(r, o): return (47 << 26) | (r << 21) | (1 << 16) | (o & 0xFFFF)
def _lmw (r, o): return (46 << 26) | (r << 21) | (1 << 16) | (o & 0xFFFF)
def _pack(*ii): return b"".join(struct.pack(">I", i) for i in ii)

def build_backup(frame_size: int, used_fprs: set[int]) -> bytes:
    instrs = [MFLR_R0, _stw(0, 0x4), _stwu(1, -frame_size), _stmw(3, GPR_SAVE_OFFSET)]
    for n in sorted(used_fprs):
        instrs.append(_stfd(n, FPR_BASE_OFFSET + n * FPR_SLOT_SIZE))
    return _pack(*instrs)

def build_restore(frame_size: int, used_fprs: set[int]) -> bytes:
    instrs = [_lfd(n, FPR_BASE_OFFSET + n * FPR_SLOT_SIZE) for n in sorted(used_fprs)]
    instrs += [_lmw(3, GPR_SAVE_OFFSET), _lwz(0, frame_size + 0x4),
               _addi(1, 1, frame_size), MTLR_R0]
    return _pack(*instrs)

# ==============================================================================
# PIC STUB
# ==============================================================================

PIC_STUB_INSTRS = [MFLR_R0, 0x48000005, 0x7D6802A6, MTLR_R0]

def pic_stub_bytes() -> bytes:
    return _pack(*PIC_STUB_INSTRS)

# ==============================================================================
# BLR REPLACEMENT
# ==============================================================================

def replace_blr(text: bytes, data_after: int, debug: bool) -> bytes:
    """
    Replace every blr in .text with a forward branch that jumps past
    all data sections (rodata + data) to land at RESTORE.
    delta = (end of .text - blr position) + bytes of data after .text
    """
    text_len = len(text)
    words    = list(struct.unpack(f">{text_len // 4}I", text))
    count    = 0
    for i, word in enumerate(words):
        if word == BLR_INSTR:
            instr_offset = i * 4
            delta  = (text_len - instr_offset) + data_after
            branch = B_BASE | (delta & 0x03FFFFFC)
            if debug:
                print(f"[DEBUG] blr at .text+{instr_offset:#05x} -> b +{delta} (skips {data_after} data bytes)")
            words[i] = branch
            count += 1
    if count:
        print(f"[INFO] Replaced {count} blr(s) with forward branch(es) past data to RESTORE.")
    return struct.pack(f">{len(words)}I", *words)


# ==============================================================================
# SOURCE REWRITING  (C only)
# ==============================================================================

def prepare_source(source: str, func_name: str) -> str:
    """
    Move the entry function to the top of the source (after preprocessor lines)
    so GCC places it first in its section. Add forward declarations for helpers.
    """
    pattern = make_func_pattern(func_name)
    m       = pattern.search(source)
    if not m:
        die(f"Could not find '{func_name}()' function definition in the source file.")

    brace_start = source.find("{", m.end())
    if brace_start == -1:
        die("Could not find opening brace of entry function.")
    depth, brace_end = 0, brace_start
    for i, ch in enumerate(source[brace_start:], brace_start):
        if ch == "{":   depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                brace_end = i
                break
    else:
        die("Could not find closing brace of entry function.")

    ret_type  = m.group(2).strip()
    args      = m.group(3)
    body      = source[brace_start : brace_end + 1]
    func_text = (f"{m.group(1)}__attribute__((naked)) "
                 f"{ret_type} {func_name}({args}) {body}")
    source_without = source[:m.start()] + source[brace_end + 1:]

    # Insertion point: where the entry function was in the original source.
    # Everything before it (includes, defines, static vars) stays in place.
    insert_pos = m.start()

    # Forward declare all helper functions so the entry function can call them
    fwd_decls = ""
    for fm in re.finditer(
        r'((?:unsigned\s+)?(?:int|void|float|double|void\s*\*))\s+(\w+)\s*\(([^)]*)\)\s*\{',
        source_without[insert_pos:]
    ):
        fwd_ret  = fm.group(1).strip()
        fwd_name = fm.group(2)
        fwd_args = fm.group(3)
        if fwd_name != func_name:
            fwd_decls += f"static {fwd_ret} {fwd_name}({fwd_args});\n"

    rewritten = (source_without[:insert_pos]
                 + fwd_decls
                 + "\n" + func_text + "\n\n"
                 + source_without[insert_pos:])
    return rewritten

# ==============================================================================
# PAYLOAD ASSEMBLY
# ==============================================================================

def build_payload(elf_path: str, raw_mode: bool, extra_fprs: set[int],
                  data_addr: int | None, debug: bool) -> tuple[bytes, bytes]:
    text     = extract_section(elf_path, ".text")
    rodata   = extract_section(elf_path, ".rodata")
    data_sec = extract_section(elf_path, ".data")

    if not text:
        die("No .text section in compiled output. Is the source file empty?")

    # Build the data blob: rodata padded to 4-byte alignment, then data.
    # The padding mirrors the linker's ALIGN(4) between sections.
    pad      = b"\x00" * ((-len(rodata)) % 4)
    data_blob = rodata + pad + data_sec if (rodata or data_sec) else b""

    if data_blob and data_addr is None:
        rodata_hex = " ".join(f"{b:02X}" for b in rodata) if rodata else "(empty)"
        data_hex   = " ".join(f"{b:02X}" for b in data_sec) if data_sec else "(empty)"
        die(
            ".rodata or .data section detected. Add '// Data: 0x80XXXXXX' to place "
            "it at a reserved RAM address, or use stack-based alternatives:\n"
            f"  .rodata: {len(rodata)} bytes  [{rodata_hex}]\n"
            f"  .data:   {len(data_sec)} bytes  [{data_hex}]\n"
            "  Stack alternatives:\n"
            "    - Float literal → VAR_ADDRESS(float, 0x80XXXXXX)\n"
            "    - Constant array → init each element separately\n"
            "    - String literal → char buf[]; STR4/STR2/STR1 macros\n"
            "    - static/global var → stack-local var\n"
            "  Run with -d to inspect the disassembly."
        )

    if debug:
        print(f"[DEBUG] .text   : {len(text)//4} instructions ({len(text)} bytes)")
        print(f"[DEBUG] .rodata : {len(rodata)} bytes")
        print(f"[DEBUG] .data   : {len(data_sec)} bytes")
        if data_blob:
            print(f"[DEBUG] data blob: {len(data_blob)} bytes → 06 code at {data_addr:#010x}")

    # Data lives at data_addr via the 06 code — not appended to the payload.
    text = replace_blr(text, 0, debug)

    if raw_mode:
        payload = text
    else:
        used_fprs  = detect_used_fprs(text, extra_fprs, debug)
        frame_size = compute_frame_size(used_fprs)
        backup     = build_backup(frame_size, used_fprs)
        restore    = build_restore(frame_size, used_fprs)
        if debug:
            fpu_desc = ("GPR only" if not used_fprs else
                        f"GPR+FPU {', '.join(f'f{n}' for n in sorted(used_fprs))}")
            print(f"[DEBUG] Frame  : {frame_size:#x} ({fpu_desc})")
        payload = backup + text + restore

    if len(payload) % 4 != 0:
        die(f"Payload size {len(payload)} is not 4-byte aligned.")
    return payload, data_blob


def pad_and_terminate(payload: bytes,
                       appended_instr: int | None,
                       debug: bool) -> bytes:
    """
    Append the optional overwritten instruction, then pad to C2 alignment.
    Layout: [...payload...] [instr?] [last_instr|nop] [00000000]
    The final 00000000 is overwritten at runtime by the gecko handler
    with a branch back to the instruction AFTER the injection site.
    If // Instruction is given, it is placed just before the terminator
    so it executes as the last thing before the handler branches back.
    """
    if appended_instr is not None:
        payload += struct.pack(">I", appended_instr)
    n = len(payload) // 4
    if n % 2 == 1:
        payload += struct.pack(">I", TERMINATOR)
    else:
        payload += struct.pack(">II", NOP_INSTR, TERMINATOR)
    if debug:
        print(f"[DEBUG] Final: {len(payload)//4} instructions ({len(payload)} bytes)")
    return payload

# ==============================================================================
# DISASSEMBLY
# ==============================================================================

def disassemble(obj_path: str) -> str:
    return subprocess.run(
        [OBJDUMP, "-d", "-M", "powerpc", obj_path],
        capture_output=True, text=True
    ).stdout

# ==============================================================================
# UTILITIES
# ==============================================================================

def die(msg: str) -> NoReturn:
    print(f"[ERROR] {msg}", file=sys.stderr)
    if sys.stdin.isatty():
        input("\nPress Enter to close...")
    sys.exit(1)

def warn(msg: str):
    print(f"[WARN]  {msg}", file=sys.stderr)

# ==============================================================================
# DOLPHIN INI DEPLOY
# ==============================================================================

def deploy_to_ini(ini_path: str, name: str, gecko_code: str, enable: bool = True):
    code_lines = gecko_code.strip().splitlines()
    new_header = code_lines[0]
    new_body   = code_lines[1:]

    if os.path.isfile(ini_path):
        with open(ini_path, "r", encoding="utf-8") as f:
            raw = f.read()
        if not raw.strip():
            raw = "[Gecko]\n\n[Gecko_Enabled]\n"
    else:
        raw = "[Gecko]\n\n[Gecko_Enabled]\n"

    lines = raw.splitlines()

    def ensure_section(tag: str) -> int:
        for i, l in enumerate(lines):
            if l.strip().lower() == tag.lower():
                return i
        lines.append("")
        lines.append(tag)
        return len(lines) - 1

    gecko_idx   = ensure_section("[Gecko]")
    enabled_idx = ensure_section("[Gecko_Enabled]")
    gecko_end   = enabled_idx if enabled_idx > gecko_idx else len(lines)
    gecko_body  = lines[gecko_idx + 1 : gecko_end]

    blocks: list[tuple[str, list[str]]] = []
    current: tuple[str, list[str]] | None = None
    for line in gecko_body:
        if line.startswith("$"):
            if current is not None:
                blocks.append(current)
            current = (line, [])
        elif current is not None:
            current[1].append(line)
    if current is not None:
        blocks.append(current)

    def code_name(header: str) -> str:
        h       = header.lstrip("$")
        bracket = h.find("[")
        return (h[:bracket] if bracket != -1 else h).strip()

    target_name = code_name(new_header)
    found = False
    for i, (hdr, body) in enumerate(blocks):
        if code_name(hdr) == target_name:
            blocks[i] = (new_header, new_body)
            found = True
            print(f"[INFO] Updated existing code '{target_name}' in ini.")
            break

    if not found:
        blocks.append((new_header, new_body))
        print(f"[INFO] Added new code '{target_name}' to ini.")

    new_gecko_lines = ["[Gecko]"]
    for hdr, body in blocks:
        new_gecko_lines.append("")
        new_gecko_lines.append(hdr)
        new_gecko_lines.extend(body)
    new_gecko_lines.append("")

    enabled_body  = lines[enabled_idx + 1:]
    enabled_names = [l.strip() for l in enabled_body if l.strip()]
    enabled_entry = f"${target_name}"
    if enable and not found and enabled_entry not in enabled_names:
        enabled_names.append(enabled_entry)

    new_enabled_lines = ["[Gecko_Enabled]"]
    new_enabled_lines.extend(enabled_names)

    pre_gecko   = lines[:gecko_idx]
    final_lines = pre_gecko + new_gecko_lines + new_enabled_lines

    with open(ini_path, "w", encoding="utf-8") as f:
        f.write("\n".join(final_lines) + "\n")

    print(f"[INFO] ini updated: {ini_path}")

# ==============================================================================
# DOLPHIN LAUNCH
# ==============================================================================

def launch_dolphin(dolphin_path: str, iso_path: str):
    """Launch Dolphin emulator with the given ISO."""
    if not os.path.isfile(dolphin_path):
        warn(f"Dolphin executable not found: {dolphin_path}")
        return
    if not os.path.isfile(iso_path):
        warn(f"ISO not found: {iso_path}")
        return
    print(f"[INFO] Launching Dolphin...")
    subprocess.Popen([dolphin_path, "-b", "-e", iso_path])

# ==============================================================================
# ENTRY POINT
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert a .c or .asm file into a C2/C3 Gecko code for GameCube modding."
    )
    parser.add_argument("input", nargs="?",
                        help="Input .c or .asm file. If omitted, uses build_file from config.json.")
    parser.add_argument("-d", action="store_true",
                        help="Debug mode: verbose output, save build artifacts")
    parser.add_argument("--no-enable", action="store_true",
                        help="Do not add the code to [Gecko_Enabled] in the ini")
    parser.add_argument("--no-launch", action="store_true",
                        help="Do not launch Dolphin after building, even if config says to")
    args = parser.parse_args()

    debug   = args.d
    enable  = not args.no_enable
    do_launch = get_launch() and not args.no_launch

    input_arg = args.input
    if input_arg is None:
        input_arg = get_build_file()
        if input_arg is None:
            die("No input file given and no 'build_file' set in config.json.")
        print(f"[INFO] Using build_file from config: {input_arg}")

    c_path = os.path.abspath(input_arg)
    if not os.path.isfile(c_path):
        die(f"Input file not found: {c_path}")

    ext = os.path.splitext(c_path)[1].lower()
    if ext not in (".c", ".asm", ".ini"):
        die(f"Unsupported file extension '{ext}'. Expected .c, .asm, or .ini")

    is_asm   = (ext == ".asm")
    is_ini   = (ext == ".ini")
    raw_mode = is_asm

    with open(c_path, "r") as f:
        source = f.read()

    if is_ini:
        blocks = parse_gecko_blocks(source)
        if not blocks:
            die("No gecko code blocks found in .ini file.")
        print(f"[INFO] Mode           : INI")
        print(f"[INFO] Codes found    : {len(blocks)}")
        ini_path = get_ini_path()
        if ini_path:
            for blk_name, gecko_code in blocks:
                deploy_to_ini(ini_path, blk_name, gecko_code, enable)
            print(f"[INFO] Successfully deployed {len(blocks)} code(s)")
            if do_launch:
                dolphin_path = get_dolphin_path()
                iso_path     = get_iso_path()
                if dolphin_path and iso_path:
                    launch_dolphin(dolphin_path, iso_path)
        else:
            warn("No ini_path in config.json — writing to codes.txt.")
            with open(TXT_PATH, "w", encoding="utf-8") as f:
                f.write("\n\n".join(gc for _, gc in blocks) + "\n")
            print(f"[INFO] Wrote {len(blocks)} code(s) to {TXT_PATH}")
        return

    base_name = os.path.splitext(os.path.basename(c_path))[0]
    name      = base_name                          # gecko code name (preserves spaces)
    func_name = re.sub(r'[^a-zA-Z0-9_]', '_', base_name)  # C identifier (spaces -> _)

    inject_addr = parse_address(source)
    data_addr   = parse_data_address(source)
    author      = parse_author(source)
    notes       = parse_notes(source)
    instr_text  = parse_instruction(source)
    state       = parse_state(source)

    print(f"[INFO] Mode           : {'ASM' if is_asm else 'C'}")
    print(f"[INFO] Name           : {name}")
    print(f"[INFO] Inject address : {inject_addr:#010x}")
    if data_addr:  print(f"[INFO] Data address   : {data_addr:#010x}")
    if author:     print(f"[INFO] Author         : {author}")
    if notes:      print(f"[INFO] Notes          : {len(notes)} line(s)")
    if instr_text: print(f"[INFO] Instruction    : {instr_text} (appended to payload)")
    if state:      print(f"[INFO] State          : {state[1]:#010x} (conditional wrapper {state[0]:#010x} {state[1]:#010x})")

    check_tools(need_gcc=not is_asm)

    extra_fprs: set[int] = set()

    tmpdir   = tempfile.mkdtemp(prefix="c2gecko_")
    src_path = os.path.join(tmpdir, os.path.basename(c_path))
    obj_path = os.path.join(tmpdir, "payload.o")
    elf_path = os.path.join(tmpdir, "payload.elf")
    ld_path  = os.path.join(tmpdir, "payload.ld")

    try:
        # State — derive conditional wrapper from state
        cond_value: int | None = None
        cond_addr:  int | None = None
        if state:
            cond_addr, cond_value = state

        # Instruction — assemble and store for appending to payload
        appended_instr: int | None = None
        if instr_text:
            appended_instr = assemble_instruction(instr_text, tmpdir)
            print(f"[INFO] Instruction hex : {appended_instr:#010x} (will be appended)")

        if is_asm:
            shutil.copy(c_path, src_path)
            print("[INFO] Assembling...")
            assemble_asm(src_path, obj_path, debug)
        else:
            print(f"[INFO] Entry function  : {func_name}()")
            rewritten = prepare_source(source, func_name)
            if debug:
                print("[DEBUG] Rewritten source:\n" + rewritten)
            with open(src_path, "w") as f:
                f.write(rewritten)
            print("[INFO] Compiling...")
            compile_c(src_path, obj_path, debug)

        print("[INFO] Linking...")
        with open(ld_path, "w") as f:
            f.write(make_linker_script(func_name, data_addr))
        link_elf(obj_path, elf_path, ld_path, debug)

        print("[INFO] Checking relocations...")
        check_relocations(elf_path, debug)

        disasm = disassemble(elf_path)
        if debug:
            print("[DEBUG] Disassembly:\n" + disasm)

        print("[INFO] Building payload...")
        payload, data_blob = build_payload(elf_path, raw_mode, extra_fprs, data_addr, debug)

        if is_asm and len(payload) == 4 and appended_instr is None:
            instr_word = struct.unpack(">I", payload)[0]
            code_lines = format_04(inject_addr, instr_word)
            print("[INFO] Single-instruction ASM — using 04 write code.")
        else:
            payload    = pad_and_terminate(payload, appended_instr, debug)
            code_lines = []
            if data_blob:
                assert data_addr is not None  # guaranteed: build_payload errors if blob without addr
                code_lines += format_06(data_addr, data_blob)
                print(f"[INFO] Data: {len(data_blob)} bytes at {data_addr:#010x} — prepending 06 code.")
            code_lines += format_c2(inject_addr, payload)

        gecko_code = build_gecko_output(code_lines, name, author, notes,
                                        cond_value, cond_addr)

        ini_path = get_ini_path()
        if ini_path:
            deploy_to_ini(ini_path, name, gecko_code, enable)
            print(f"[INFO] Successfully generated '{name}'")
            if do_launch:
                dolphin_path = get_dolphin_path()
                iso_path     = get_iso_path()
                if dolphin_path and iso_path:
                    launch_dolphin(dolphin_path, iso_path)
        else:
            warn("No ini_path in config.json — writing to codes.txt.")
            with open(TXT_PATH, "w", encoding="utf-8") as f:
                f.write(gecko_code + "\n")
            print(f"[INFO] Successfully generated '{name}' -> {TXT_PATH}")

        if debug:
            base = os.path.splitext(TXT_PATH)[0]
            if not is_asm:
                shutil.copy(src_path, base + ".rewritten.c")
            shutil.copy(obj_path, base + ".o")
            shutil.copy(elf_path, base + ".elf")
            shutil.copy(ld_path,  base + ".ld")
            with open(base + ".disasm.txt", "w") as f:
                f.write(disasm)
            print(f"[DEBUG] Artifacts saved: {os.path.splitext(TXT_PATH)[0]}.*")

    finally:
        if not debug:
            shutil.rmtree(tmpdir, ignore_errors=True)
        else:
            print(f"[DEBUG] Temp dir: {tmpdir}")


if __name__ == "__main__":
    main()