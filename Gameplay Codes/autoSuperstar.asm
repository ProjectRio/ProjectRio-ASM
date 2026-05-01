###########################################################
# Auto Superstar
###########################################################
# Author: Nuche17



###########################################################
###########################################################
# Requires: 
# 1. To determine which characters to superstar, indicators need to be set in the first unused byte for each character in the roster struct.
# The first indicator byte is at 0x80353be5. This is for the first character on the P1 team. Offset for each character is 0xa0.
# For the common situation of starring everyone, use this code:
   # 08353be5 00000001
   # 001100a0 00000000
# 2. Two free bytes of memory to store index values to track the progress of the superstarring. Currently using 0x802EBF99 and 0x802EBF9A.

# Inject: 8005a4f4
cmpwi r27, 0x2 #compare register that is supposed to have the team number
bgt end #if greater than or equal to 2, then skip.

lis r3, 0x802F #load index address into r3
subi r3, r3, 0x4067
add r3, r3, r27 # add team number to address, so add 1 if P2 team.

lbz r24, 0x0(r3) #load index value into r24

#if index = 0, just increment index to 1 to give superstar gecko codes a chance to load.
cmpwi r24, 0x0
beq increment

#load cursor address into r30
lis r30, 0x8033
addi r30, r30, 0x6726
add r30, r30, r27 #add p2 offset

#if index = 0xa or 0xb, superstarring done. 0xa set cursor to 0 and increment index. 0xb do nothing.
cmpwi r24, 0xa
blt superstar #go to main superstar code if less than 0xa.

beq cursorTo0 #if index is 0xa, set cursor to 0 and increment.
b end #if index is above 0xa, do nothing.

cursorTo0: #if index is 0xa, set cursor to 0 and increment.
li r26, 0x0 #safe to use r26 at this point as scratch register
stb r26, 0x0(r30)
b increment

#main superstarring loop
superstar:
stb r24, 0x0(r30) #set cursor to index number

#get superstar bool address
lis r3, 0x8035 #load base address into r3
addi r3, r3, 0x3BE5
mulli r30, r27, 0x9 # create offset in r30. Start by skipping to character number 9 if P2.
add r30, r30, r24 # add index to get to character number + 1.
subi r30, r30, 0x1 # subtract 1 since index value is 1-indexed, but we need 0-indexed.
mulli r30, r30, 0xA0 # multiply by character offset.
add r3, r3, r30 # add offset to base address

#load SuperstarringInProgress address into r30
lis r30, 0x8033 
addi r30, r30, 0x677e
add r30, r30, r27 # add p2 offset

lbz r28, 0x0(r3)  #load superstarring bool value into r28.
stb r28, 0x0(r30) #store superstarring bool value into SuperstarringInProgress, which will trigger the game to superstar the character.

#increment index by 1
increment:
lis r3, 0x802F # re-load index address into r3
subi r3, r3, 0x4067
add r3, r3, r27 # add team number to address, so add 1 if P2 team.

addi r24, r24, 1 # add 1 to current value

stb r24, 0x0(r3) # store

b end

end:
lis r3, 0x8033 #replace original instruction