SYSYC=		../build/compiler
CC=		clang
LD=		ld.lld
OBJCOPY=	llvm-objcopy

CFLAGS=		-target riscv32-unknown-linux-elf -march=rv32im -mabi=ilp32
LDFLAGS=	-T linker.ld

SRC_DIR=	src
SRC_SRCS=	$(shell find src -type f -name "*.sysy")
SRC_ASMS=	$(SRC_SRCS:.sysy=.S)
SRC_OBJS=	$(SRC_SRCS:.sysy=.o)

ASM_DIR=	asm
ASM_SRCS=	$(shell find asm -type f -name "*.S")
ASM_OBJS=	$(ASM_SRCS:.S=.o)

LIB_DIR=	lib
LIB_ASMS=	$(shell find lib -type f -name "*.S")
LIB_OBJS=	$(LIB_ASMS:.S=.o)

ELF=		game
BIN=		game.bin

all: $(BIN)

$(SRC_DIR)/%.S: $(SRC_DIR)/%.sysy
	$(SYSYC) -riscv $< -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.S
	$(CC) $< -c -o $@ $(CFLAGS)

$(ASM_DIR)/%.o: $(ASM_DIR)/%.S
	$(CC) $< -c -o $@ $(CFLAGS)

$(LIB_DIR)/%.o: $(LIB_DIR)/%.S
	$(CC) $< -c -o $@ $(CFLAGS)

$(ELF): $(LIB_OBJS) $(ASM_OBJS) $(SRC_OBJS)
	$(LD) $^ -o $@ $(LDFLAGS)

$(BIN): $(ELF)
	$(OBJCOPY) --strip-all $< -O binary $@

debug: $(BIN)
	qemu-system-riscv32 -s -S -M virt -display none -serial stdio \
		-bios none -kernel $<

run: $(BIN)
	qemu-system-riscv32 -M virt -display none -serial stdio \
		-bios none -kernel $<

clean:
	rm $(SRC_ASMS) $(SRC_OBJS) $(LIB_OBJS) $(ASM_OBJS) $(ELF) $(BIN)

.PHONY: debug run clean

.SECONDARY: $(SRC_ASMS)
