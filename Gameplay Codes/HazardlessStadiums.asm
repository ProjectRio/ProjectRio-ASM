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

Check_Star_Skills:              # Big Balla now uses hazardless
    lis r19, 0x803C
    ori r19, r19, 0x5F41
    lbz r19, 0(r19)
    cmpwi r19, 0
    beq Check_Stadiums

Check_Superstar_Characters:     # if star skills on, check if star characters
    lis r19, 0x8035             # r19 stores starred icon addr
    ori r19, r19, 0x323B
    li r21, 0                   # r21 is for loop index

Start_Loop:
    lbzx r20, r19, r21          # r20 is value at ba + for loop i
    cmpwi r20, 1
    beq End
    addi r21, r21, 0x1
    cmpwi r21, 0x12
    bne Start_Loop

Check_Stadiums:
    cmpwi r5, 0             # mario's
    beq End
    cmpwi r5, 6             # toy field
    bge End
    cmpwi r5, 1             # bowser's
    beq Bowser
    cmpwi r5, 2             # wario's
    beq Wario
    cmpwi r5, 3             # yoshi's
    beq Yoshi
    cmpwi r5, 4             # peach's
    beq Peach
    cmpwi r5, 5             # dk's
    beq DK
    b End
    
Bowser:
    lis r19, 0x8070         # disable fireballs
    ori r19, r19, 0x56c8
    lis r20, 0x3860
    stw r20, 0(r19)
    lis r20, 0x3800         # lock thwomps
    stw r20, 0x1638(r19)
    b End

Wario:
    lis r19, 0x8070         # disable tornadoes
    ori r19, r19, 0xfc30
    lis r20, 0x6000
    stw r20, 0(r19)
    lis r20, 0x3800         # freeze chain chomps
    stw r20, 0x376C(r19)
    b End

Yoshi:
    lis r19, 0x8069         # disable plants
    ori r19, r19, 0x9e54
    lis r20, 0x3800
    stw r20, 0(r19)
    b End

Peach:
    lis r19, 0x807c         # remove blocks
    ori r19, r19, 0xd098
    li r20, 0x0
    li r21, 0

Peach_For_Loop:
    stb r20, 0x11(r19)
    addi r21, r21, 1
    addi r19, r19, 0x14
    cmpwi r21, 0x10
    blt Peach_For_Loop
    b End

DK:
    lis r19, 0x8072
    ori r19, r19, 0xf950
    lis r20, 0x6000
    stw r20, 0(r19)         # disable klaptraps
    stw r20, 0x4A54(r19)    # disable barrels
    stw r20, 0x4A60(r19)

End:
    lmw r14, 0x8 (r1)
    addi r1, r1, 0x50       # restore stack


# Remove Klaptraps

# Requires: 2072FDC8 C0010044
# Inject: 0x8072FDC8
Start:
    lis r19, 0x803C             # check for star skills enabled
    ori r19, r19, 0x5F41
    lbz r19, 0(r19)
    cmpwi r19, 0
    beq Disable_Klaptraps

Check_Superstar_Characters:
    lis r19, 0x8035             # r19 stores starred icon addr
    ori r19, r19, 0x323B
    li r21, 0                   # r21 is for loop index

Start_Loop:
    lbzx r20, r19, r21          # r20 is value at ba + for loop i
    cmpwi r20, 1
    beq End
    addi r21, r21, 0x1
    cmpwi r21, 0x12
    bne Start_Loop

Disable_Klaptraps:              # this just makes them spawn in offscreen
    lis r17, 0x4348
    stw r17, 0x44(r1)

End:
    lfs f0, 0x44(r1)

