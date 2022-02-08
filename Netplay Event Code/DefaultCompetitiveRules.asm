###########################################################
# Default Competitive Rules
###########################################################
# Authors: LittleCoaks

# mercy on for both
# 9-innings for stars off
# 5-innings drop spot off for stars on


###########################################################
###########################################################
# Inject: 0x80049d08
START_FUNC:
    lis r14, 0x8035             # r14 stores starred icon addr
    ori r14, r14, 0x323B
    li r16, 0                   # r16 is for loop index

START_LOOP:
    lbzx r15, r14, r16          # r15 is value at ba + for loop i
    cmpwi r15, 1
    beq STARS_ON
    addi r16, r16, 0x1
    cmpwi r16, 0x12
    bne START_LOOP

STARS_OFF:
    li r14, 4                   # 9-innings
    li r15, 1                   # drop spot on
    b END_FUNC

STARS_ON:
    li r14, 2                   # 5-innings
    li r15, 0                   # drop spot off

END_FUNC:
    stb r14, 0x3E(r3)           # store args to memory addrs
    stb r15, 0x48(r3)
    stb r15, 0x4C(r3)
    li r16, 1                   # mercy rule on
    stb r16, 0x3F(r3)
    lis	r3, 0x803C              # restore instruciton that was overwritten

