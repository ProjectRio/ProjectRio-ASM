###########################################################
# Smash C-Stick
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################

# Inject: 0x800A5A3C
START:
  lwz r0, 0x01C0 (r3)               # Get Player Raw Inputs --> 0x <ABXY> <LRZ> <LeftRight> <UpDown>
  lis r15, 0x0100                   # 0x0100 will be anded to r0 later on to force the A input


LEFT:
  lbz r14, 0x01C4(r3)               # c-stick left/right value
  cmpwi r14, 0x40                   # hold left condition
  bgt RIGHT
  lis r19, 0xffff                   # set r19 to 0xffff00ff
  ori r19, r19, 0x00ff
  and. r0, r0, r19                  # and bits to set control stick to holding left
  or r0, r0, r15                    # press A
  b DOWN                            # skip right cause can't have errors


RIGHT:
  cmpwi r14, 0xB0                   # hold right condition
  blt DOWN
  ori r0, r0, 0xff00                # or bits to set control stick to holding right
  or r0, r0, r15                    # press A


DOWN:
  lbz r14, 0x01C5(r3)               # c-stick up/down value
  cmpwi r14, 0x40                   # hold down condition
  bgt UP
  lis r19, 0xffff                   # set r19 to 0xffffff00
  ori r19, r19, 0xff00
  and. r0, r0, r19                  # and bits to set control stick to holding down
  or r0, r0, r15                    # press A
  b END


UP:
  cmpwi r14, 0xB0                   # hold up condition
  blt END
  ori r0, r0, 0xff                  # or bits to set control stick to holding up
  or r0, r0, r15                    # press A
  b END


END:
  li r14, 0                         # clear registers
  li r15, 0
  li r19, 0

