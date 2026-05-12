###########################################################
# Pause Clock
###########################################################
# Author: LittleCoaks
# 
# 806eed90 -- write pause bool instr
# 806eed18 -- lhz	r0, 0x0006 (r4) # fielder pause check
# 806eed5c -- lhz	r0, 0x0006 (r4) # batter pause check
# 	skip these instruction if port went over pause clock


###########################################################
###########################################################
# Requires: "206EED18 A0040006"
# Requires: "206EED5C A0040006"

# TODO:
# new function that increments pause clock
# new function that resets pause clock each inning
# add time for tired pitcher
# close pause menu if time expires

# Inject: 0x806EED18
START:
    # get pause clock value
    # check if too high
    # branch to END if too high
    # run replaced instruction if not


END:

