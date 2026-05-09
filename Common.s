# Common.s
# GAS macros for GameCube (PowerPC) ASM injection files.
# Include at the top of any .asm file with: .include "Common.s"
#
# Stack management (backupall/restoreall) lives here rather than Common.h
# because C code managed by cgecko.py has its stack handled automatically.

.ifndef HEADER_COMMON

# ==============================================================================
# REGISTER ALIASES
# ==============================================================================

# .set <name>, <reg>
# Create a named alias for a register. From this point in the file, <name>
# can be used anywhere the register is expected. Aliases are file-scoped —
# define them near the top of each .asm file after .include "Common.s".
#
# Aliasing does not affect the original register name — both names work.
# There is no way to undefine an alias, so choose names that are meaningful
# for the entire file.
#
# Example:
#   .set pPlayer, r3    # r3 holds a pointer to the player struct
#   .set score,   r4    # r4 holds the current score value
#   .set temp,    r12   # r12 used as scratch
#
#   lwz  score, 0x44(pPlayer)
#   addi score, score, 1
#   stw  score, 0x44(pPlayer)

# ==============================================================================
# ADDRESS LOADING
# ==============================================================================

# load reg, address
# Load a 32-bit immediate address into a register.
# PPC cannot load a 32-bit value in one instruction — this expands to lis + ori.
# Use this whenever you need a full address in a register.
#
# Example:
#   load r3, 0x80123456    # r3 = 0x80123456
.macro load reg, address
    lis  \reg, \address @h
    ori  \reg, \reg, \address @l
.endm

# loadwz reg, address
# Load a 32-bit word value from a memory address into a register.
# Equivalent to: reg = *(int*)address
#
# Example:
#   loadwz r3, 0x80123456  # r3 = value at 0x80123456
.macro loadwz reg, address
    lis  \reg, \address @h
    ori  \reg, \reg, \address @l
    lwz  \reg, 0(\reg)
.endm

# loadbz reg, address
# Load a single byte value from a memory address into a register (zero-extended).
# Equivalent to: reg = *(u8*)address
#
# Example:
#   loadbz r3, 0x80123456  # r3 = byte at 0x80123456
.macro loadbz reg, address
    lis  \reg, \address @h
    ori  \reg, \reg, \address @l
    lbz  \reg, 0(\reg)
.endm

# ==============================================================================
# BRANCHING
# ==============================================================================

# branchl reg, address
# Call a function at an absolute address (branch with link).
# reg is used as scratch — its value after the call is undefined.
# Return address is stored in LR as normal.
# Use r12 as reg by convention (caller-saved, safe to clobber).
#
# Example:
#   branchl r12, 0x800F9BDC    # call OSReport
.macro branchl reg, address
    lis    \reg, \address @h
    ori    \reg, \reg, \address @l
    mtctr  \reg
    bctrl
.endm

# branch reg, address
# Jump to an absolute address (no link — does not return).
# Use for tail calls or unconditional jumps to distant addresses.
# Use r12 as reg by convention.
#
# Example:
#   branch r12, 0x80012345     # jump to address, no return
.macro branch reg, address
    lis    \reg, \address @h
    ori    \reg, \reg, \address @l
    mtctr  \reg
    bctr
.endm

# ==============================================================================
# STACK MANAGEMENT
# ==============================================================================

# backupall
# Save GPRs r3–r31 and LR to the stack, allocating a 0x100-byte frame.
# Call at the start of an injection before touching any registers.
# Must be paired with restoreall.
#
# Frame layout after backupall:
#   new SP + 0x000  back-chain (old SP)
#   new SP + 0x008  r3
#   ...
#   new SP + 0x080  r31
#   old SP + 0x004  LR  (= new SP + 0x104)
.macro backupall
    mflr  r0
    stw   r0,  0x4(r1)
    stwu  r1, -0x100(r1)
    stmw  r3,  0x8(r1)
.endm

# restoreall
# Restore GPRs r3–r31 and LR from the stack, deallocating the 0x100-byte frame.
# Must follow a matching backupall.
# Do NOT place a blr after this — execution falls through to the C2 terminator.
.macro restoreall
    lmw   r3,  0x8(r1)
    lwz   r0,  0x104(r1)
    addi  r1,  r1, 0x100
    mtlr  r0
.endm

.endif
.set HEADER_COMMON, 1
