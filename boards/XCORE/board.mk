# List of all the board related files.
BOARDSRC = ./boards/XCORE/board.c

# Required include directories
BOARDINC = ./boards/XCORE

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
