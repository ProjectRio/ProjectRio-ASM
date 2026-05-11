###########################################################
# Get Active Ports.asm
###########################################################
# Author: LittleCoaks

# Address: 0x80042CD8

###########################################################
###########################################################
# 0x802EBF91 --> initialized port P1
# 0x802EBF92 --> initialized port Px
# 0x802EBF94 --> fielding port
# 0x802EBF95 --> batting port

START:
  stwu r1, -0x50 (r1)            # Backup stack, make r1ace for 18 registers
  stmw r14, 0x8 (r1)

  lis r15, 0x802E                # Load init port P1 addr
  ori r15, r15, 0xBF91
  lis r16, 0x8089                # Load P1 CPU bool
  ori r16, r16, 0x2ACA
  lbz r16, 0(r16)
  cmpwi r16, 0x1                 # Is P1 CPU
  beq- SET_P1_CPU
  li r16, 0x1                    # If not CPU, set to 1 (P1)
  b SET_P2

SET_P1_CPU:
  li r16, 0x5

SET_P2:
  stb r16, 0(r15)                # Store P1 info to r5
  lis r16, 0x800E                # Get Px's port
  ori r16, r16, 0x874D
  lbz r16, 0(r16)                 
  addi r16, r16, 0x1             # Add 1 since we want P1 == 1 instead of 0
  stb r16, 1(r15)                # Store Px info

END:
  lmw r14, 0x8 (r1)
  addi r1, r1, 0x50              # restore stack
  li r5, 0x3f                    # Replace overwritten instruction

