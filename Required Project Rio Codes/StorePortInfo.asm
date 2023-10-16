###########################################################
# Store Port Info
###########################################################
# Author: LittleCoaks

# 0x802EBF91 --> initialized port P1
# 0x802EBF92 --> initialized port Px
# 0x802EBF94 --> fielding port
# 0x802EBF95 --> batting port



###########################################################
###########################################################
# Grab Active Ports

# Inject: 0x80042CD8
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



###########################################################
###########################################################
# Store Active Ports to Batter/Fielder Port
# Requires: "206706B8 3C608089"

# Inject: 0x806706B8
START:
  lis r31, 0x8089               # Check if P1 is controlling pitcher
  ori r31, r31, 0x3928
  cmpw r4, r31                  # If r4 == r31, P1 controlls the pitcher
  lis r31, 0x802E               # Load init port info addr
  ori r31, r31, 0xBF91
  beq- P1_FIELDING

P1_BATTING:
  lbz r3, 0(r31)                # Move P1 info to Batter Port addr
  stb r3, 4(r31)
  lbz r3, 1(r31)                # Move Px info to Fielder Port addr
  stb r3, 3(r31)
  b END

P1_FIELDING:
  lbz r3, 1(r31)                # Move Px info to Batter Port addr
  stb r3, 4(r31)
  lbz r3, 0(r31)                # Move P1 info to Fielder Port addr
  stb r3, 3(r31)

END:
  lis r3, 0x8089                # Replace overwritten instruction



###########################################################
###########################################################
# Clear port info after game ends
# Requires: "2063F14C 38600000"

# Inject: 0x8063F14C
START:
  li r3, 0x0                    # Clear r3
  lis r5, 0x802E                # Load port info addr
  ori r5, r5, 0xBF90
  stb r3, 1(r5)                 # Clear addrs   
  sth r3, 2(r5)
  sth r3, 4(r5)

END:

