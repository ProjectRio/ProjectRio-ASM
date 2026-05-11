###########################################################
# Control Stick Overrides DPad
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################

# Address: 0x800A59FC
# Instruction: stw r0, 0x01C0 (r5)
Start:
  mr r14, r0
 
CheckLeft:
  rlwinm r14, r14, 24, 24, 31
  cmpwi r14, 0x52
  ble ZeroDPad

CheckRight:
  cmpwi r14, 0xae
  bge ZeroDPad

CheckUp:
  mr r14, r0
  rlwinm r14, r14, 0, 24, 31
  cmpwi r14, 0xae
  bge ZeroDPad

CheckDown:
  cmpwi r14, 0x52
  bgt End

ZeroDPad:
  rlwinm r0, r0, 0, 16, 11

End:
