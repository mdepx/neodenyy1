APP =		neodenyy1
MACHINE =	arm

CC =		${CROSS_COMPILE}gcc
LD =		${CROSS_COMPILE}ld
OBJCOPY =	${CROSS_COMPILE}objcopy

OBJDIR =	obj
OSDIR =		mdepx

EMITTER =	python3 -B ${OSDIR}/tools/emitter.py

all:
	@${EMITTER} -j mdepx.conf
	@${OBJCOPY} -O binary obj/${APP}.elf obj/${APP}.bin

clean:
	@rm -rf obj/*

include ${OSDIR}/mk/user.mk
