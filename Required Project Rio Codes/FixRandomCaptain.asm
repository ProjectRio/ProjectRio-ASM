###########################################################
# Fix Random Captain
###########################################################
# Author: LittleCoaks



###########################################################
###########################################################
# Properly seeds the rng seed which determines the random captain
# Requires: 2063F7C4 90040000

# Inject: 0x8063F7C4
START:
  stw r0, 0(r4)
  stw r0, 0x330(r4)

END:


