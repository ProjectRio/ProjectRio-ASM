###########################################################
# Fix Random Captain
###########################################################
# Author: LittleCoaks

# Address: 0x8063F7C4
# State: Menu
# *Properly seeds the rng seed which determines the random captain

###########################################################
###########################################################

START:
  stw r0, 0(r4)
  stw r0, 0x330(r4)

END:


