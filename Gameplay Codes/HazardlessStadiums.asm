###########################################################
# Hazardless Stadiums
###########################################################
# Author: LittleCoaks, PeacockSlayer, DannyBoy



###########################################################
###########################################################
# Requires: 20699508 88a40009

# Inject: 80699508 88a40009
START:
    lbz	r5, 0x0009 (r4)
    cmpwi r5, 0             # mario's
    beq END
    cmpwi r5, 6             # toy field
    bge END
    cmpwi r5, 1             # bowser's
    beq BOWSER
    cmpwi r5, 2             # wario's
    beq WARIO
    cmpwi r5, 3             # yoshi's
    beq YOSHI
    cmpwi r5, 4             # peach's
    beq PEACH
    cmpwi r5, 5             # dk's
    beq DK
    b END
    

BOWSER:
    lis r14, 0x8070         # disable fireballs
    ori r14, r14, 0x56c8
    lis r15, 0x3860
    stw r15, 0(r14)
    lis r15, 0x3800         # lock thwomps
    stw r15, 0x1638(r14)
    b END


WARIO:
    lis r14, 0x8070         # disable tornadoes
    ori r14, r14, 0xfc30
    lis r15, 0x6000
    stw r15, 0(r14)
    lis r15, 0x3800         # freeze chain chomps
    stw r15, 0x376C(r14)
    b END


YOSHI:
    lis r14, 0x8069         # disable plants
    ori r14, r14, 0x9e54
    lis r15, 0x3800
    stw r15, 0(r14)
    b END


PEACH:
    lis r14, 0x807c         # remove blocks
    ori r14, r14, 0xd09c
    lis r15, 0x4120
    li r16, 0


PEACH_FOR_LOOP:
    stw r15, 0(r14)
    addi r16, r16, 1
    addi r14, r14, 0x14
    cmpwi r16, 0x10
    blt PEACH_FOR_LOOP
    b END


DK:
    lis r14, 0x8072
    ori r14, r14, 0xf950
    lis r15, 0x6000
    stw r15, 0(r14)         # disable klaptraps
    stw r15, 0x4A54(r14)    # disable barrels
    stw r15, 0x4A60(r14)


END:


