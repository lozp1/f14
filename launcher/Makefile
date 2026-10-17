#---------------------------------------------------------------------------------
# FIFA 14's launcher NRO (64-bit, devkitA64 + libnx). It carries
# ../fifa14_nx.nsp (the 32-bit game program built next door by ../build.sh) in
# its romfs and installs it as the ExeFS override of its forwarder.
#
# APP_TITLE is what sphaira lists; TARGET is the NRO (= PORT_NAME);
# PORT_PAYLOAD is the 32-bit program (= the wrapper's TARGET). The payload's
# name is a contract with the game program's self-update: never rename it.
#---------------------------------------------------------------------------------
APP_TITLE    := FIFA 14
APP_AUTHOR   := aks796, EA Mobile
APP_VERSION  := 0.0.1
TARGET       := fifa14_nx
PORT_PAYLOAD := fifa14_nx

TOPDIR ?= $(CURDIR)
A32    ?= $(TOPDIR)/../runtime

include $(A32)/launcher/launcher.mk
