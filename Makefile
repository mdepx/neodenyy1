APP =		neodenyy1
MACHINE =	arm

CC =		${CROSS_COMPILE}gcc
LD =		${CROSS_COMPILE}ld
OBJCOPY =	${CROSS_COMPILE}objcopy
SIZE =		${CROSS_COMPILE}size

OBJDIR =	obj
OSDIR =		mdepx

EMITTER =	python3 -B ${OSDIR}/tools/emitter.py

all:
	@${EMITTER} -j mdepx.conf
	@${OBJCOPY} -O binary obj/${APP}.elf obj/${APP}.bin
	@${SIZE} obj/${APP}.elf

clean:
	@rm -rf obj/*

include ${OSDIR}/mk/user.mk
