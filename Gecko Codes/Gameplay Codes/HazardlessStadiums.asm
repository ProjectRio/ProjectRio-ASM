###########################################################
# Hazardless Stadiums
###########################################################
# Author: LittleCoaks, PeacockSlayer, DannyBoy



###########################################################
###########################################################
# Requires: 20699508 88a40009

# Inject: 80699508
Start:
    lbz	r5, 0x0009 (r4)         # stadium id, this line is required
    stwu r1, -0x50 (r1)         # Backup stack, make r1ace for 18 registers
    stmw r14, 0x8 (r1)

Check_Stadiums:
    lis r20, 0x3800				# set r20 to the instruction we overwrite with
    ori r20, r20, 0x0007
    cmpwi r5, 2				    # wario's
    bne Bowser

# the minigame strat doesn't work for wario's cause it loads in a single chain chomp at home plate.
# for now, we do it manually
Wario:
    lis r19, 0x8070				# disable tornadoes
    ori r19, r19, 0xfc30
    lis r21, 0x6000
    stw r21, 0(r19)
    lis r21, 0x3800				# freeze chain chomps
    stw r21, 0x376C(r19)
    lis r19, 0x807c				# remove tornadoes
    ori r19, r19, 0x9964
    lis r21, 0x42c8
    stw r21, 0x0(r19)
    stw r21, 0x34(r19)
    
Bowser:
    lis r19, 0x8070				# set to minigame mode (no hazards loaded)
    ori r19, r19, 0x7cb8
    stw r20, 0(r19)

Yoshi:
    lis r19, 0x8072				# set to minigame mode (no hazards loaded)
    ori r19, r19, 0x4428
    stw r20, 0(r19)

Peach:
    lis r19, 0x8073				# set to minigame mode (no hazards loaded)
    ori r19, r19, 0x9a28
    stw r20, 0(r19)

DK:
    lis r19, 0x8073				# set to minigame mode (no hazards loaded)
    ori r19, r19, 0x6d08
    stw r20, 0(r19)
    lis r19, 0x8073
    ori r19, r19, 0x43A4
    lis r21, 0x6000
    stw r21, 0(r19)             # remove barrels, doesn't go away on minigame mode smh :/
    stw r21, 0xc(r19)

End:
    lmw r14, 0x8 (r1)
    addi r1, r1, 0x50	        # restore stack

