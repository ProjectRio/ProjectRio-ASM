###########################################################
# Clear Hit Result
###########################################################
# Author: PeacockSlayer



###########################################################
###########################################################

# Requires: "206BBF88 99090037"
# Inject: 0x806BBF88
START:
  stb r8, 55(r9)                # Replace overwritten instruction
  lis r21, 0x8089               # Load hit result addr
  ori r21, r21, 0x3BAA
  stb r8, 0(r21)                # Set to 0
  li r21, 0x0                   # Reset register

END:

