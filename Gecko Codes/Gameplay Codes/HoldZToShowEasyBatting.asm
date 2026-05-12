###########################################################
# Hold Z to show Easy Batting (No changes to gameplay)
###########################################################
# Author: Roeming



###########################################################
###########################################################

# Inject: 0x806a82b0
lis r5, -0x7f77 ; load address top bytes

addi r4, r5, 0x2990 
lwz r0, 0x0(r4) ; load batting player
mulli r0,r0, 0x4

addi r4, r5, 0x2a78 
add r4,r4,r0 
lwz r0, 0(r4) ; load batting port
mulli r0,r0,0x10

addi r4, r5, 0x392c
add r4, r4, r0
lhz r0, 0(r4) ; load batting controls
andi. r0, r0, 0x10 ; Remove all other inputs to see if Z is held

cmplwi r0,0x0 ; if not held, return. (return statement in game code)
