###########################################################
# Store Random Batting Ints
###########################################################
# Author: Roeming



###########################################################
###########################################################

# Inject: 80651e68
stb r0,0x91(r4)    # old command

lis r5,-0x7f77 
addi r5,r5,0x2684  # load pointer to rand values 80892684

lis r6,-0x7fd1
subi r6,r6,0x3ff0  # load pointer to write-out location 802ec010

lwz r4, 0x0(r5)    # load rand1 from 80892684
sth r4, 0x0(r6)    # store rand1 at 802ec010

lwz r4, 0x4(r5)    # load rand2 from 80892688
sth r4, 0x2(r6)    # store rand2 at 802ec012

lhz r4, 0x18(r5)   # load rand3 from 8089269c
sth r4, 0x4(r6)    # store rand3 at 802ec014
