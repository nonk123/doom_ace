RSC = gsi ribbit/src/rsc.scm

SCM_FILES = lib.scm engine_main.scm
SCM_FILES := $(SCM_FILES:%=../scm/%)
SCM_OUTPUT = ../scm/_output_.scm

RIBBIT_HOST_DIR = ribbit/src/host/c
RIBBIT_OUTPUT = ribbit.c
RIBBIT_OBJ = ribbit.o

program = code

OBJ = asm.o main.o utils.o bugfix.o vesa.o wadfile.o config.o controls.o ldr_texture.o ldr_flat.o ldr_sprite.o textpars.o filebuf.o think.o generic.o sound.o mobj.o player.o decorate.o action.o dehacked.o animate.o inventory.o weapon.o map.o hitscan.o special.o sight.o draw_c.o draw_a.o polyobj.o render.o terrain.o font.o extra3d.o stbar.o menu.o saveload.o cheat.o rng.o wipe.o demo.o mover.o net.o doom_vars.o $(RIBBIT_OBJ)

CC = gcc -m32 -march=i386
OPT = -O2 -g0
BOTH = -fno-pie

CFLAGS = ${BOTH} ${OPT} -mgeneral-regs-only -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-stack-protector -fno-reorder-blocks-and-partition -fcf-protection=none -Wno-address-of-packed-member -U_FORTIFY_SOURCE -nostdlib -nodefaultlibs

default: build

$(RIBBIT_OUTPUT): $(SCM_FILES) ribbit_hack.c
	git -C ribbit restore src/
	cp $(RIBBIT_HOST_DIR)/rvm.c ribbit_tmp.c
	$(CC) -E -CC -P ribbit_hack.c -o $(RIBBIT_HOST_DIR)/rvm.c
	cat $(SCM_FILES) > $(SCM_OUTPUT)
	$(RSC) -t c -o $@ $(SCM_OUTPUT)

ribbit_hack.c:

build: ${program}.lmp

clean:
	rm -f $(RIBBIT_OUTPUT).tmp $(RIBBIT_OUTPUT) *.o ${program} ${program}.rel ${program}.bin ${program}.lmp

${program}.lmp:  $(RIBBIT_OUTPUT) ${OBJ}
	${CC} ${CFLAGS} -o ${program} -nostdlib -Wl,--gc-sections -Wl,--no-ld-generated-unwind-info -Wl,--no-dynamic-linker -Wl,--omagic -Wl,-T linker.ld -Wl,-Map=code.map $(OBJ)
	objcopy -O binary --only-section=.text --only-section=.hooks --only-section=.rodata --only-section=.data ${program} ${program}.bin
	objcopy -O binary --only-section=.rel.dyn ${program} ${program}.rel
	./lumpgen.py ${program}.bin ${program}.rel ${program}.lmp
