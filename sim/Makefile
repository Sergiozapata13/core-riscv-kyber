# Makefile de simulación — Fase 0
#
# Objetivos:
#   make hello_verilator   -> compila y corre el testbench trivial de Verilator
#   make hello_riscv       -> compila el firmware .c minimo con el toolchain RISC-V
#   make clean             -> limpia artefactos de build

VERILATOR   := verilator
RTL_DIR     := ../rtl
TB_DIR      := ../tb
SW_DIR      := ../sw
BUILD_DIR   := obj_dir

RISCV_CC    := riscv64-unknown-elf-gcc
RISCV_OBJCOPY := riscv64-unknown-elf-objcopy
RISCV_FLAGS := -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -O2

.PHONY: all hello_verilator hello_riscv regfile alu control imm_gen branch_unit imem dmem core_top if_id_reg id_ex_reg ex_mem_reg mem_wb_reg forwarding_unit clean

all: hello_verilator hello_riscv regfile alu control imm_gen branch_unit imem dmem core_top if_id_reg id_ex_reg ex_mem_reg mem_wb_reg forwarding_unit

# ---------------------------------------------------------------------------
# Fase 2 - Forwarding unit: resuelve hazards de datos sin stall
# ---------------------------------------------------------------------------
forwarding_unit:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module forwarding_unit \
		$(RTL_DIR)/core/forwarding_unit.sv \
		$(TB_DIR)/tb_forwarding_unit.cpp \
		-o forwarding_unit_sim
	./$(BUILD_DIR)/forwarding_unit_sim

# ---------------------------------------------------------------------------
# Fase 2 - Registro de segmentacion MEM/WB
# ---------------------------------------------------------------------------
mem_wb_reg:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module mem_wb_reg \
		$(RTL_DIR)/core/mem_wb_reg.sv \
		$(TB_DIR)/tb_mem_wb_reg.cpp \
		-o mem_wb_reg_sim
	./$(BUILD_DIR)/mem_wb_reg_sim

# ---------------------------------------------------------------------------
# Fase 2 - Registro de segmentacion EX/MEM
# ---------------------------------------------------------------------------
ex_mem_reg:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module ex_mem_reg \
		$(RTL_DIR)/core/ex_mem_reg.sv \
		$(TB_DIR)/tb_ex_mem_reg.cpp \
		-o ex_mem_reg_sim
	./$(BUILD_DIR)/ex_mem_reg_sim

# ---------------------------------------------------------------------------
# Fase 2 - Registro de segmentacion ID/EX
# ---------------------------------------------------------------------------
id_ex_reg:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module id_ex_reg \
		$(RTL_DIR)/core/alu.sv \
		$(RTL_DIR)/core/id_ex_reg.sv \
		$(TB_DIR)/tb_id_ex_reg.cpp \
		-o id_ex_reg_sim
	./$(BUILD_DIR)/id_ex_reg_sim

# ---------------------------------------------------------------------------
# Fase 2 - Registro de segmentacion IF/ID
# ---------------------------------------------------------------------------
if_id_reg:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module if_id_reg \
		$(RTL_DIR)/core/if_id_reg.sv \
		$(TB_DIR)/tb_if_id_reg.cpp \
		-o if_id_reg_sim
	./$(BUILD_DIR)/if_id_reg_sim

# ---------------------------------------------------------------------------
# Fase 1 - Datapath monociclo completo: corre fib.s de punta a punta
# ---------------------------------------------------------------------------
core_top:
	$(VERILATOR) --cc --exe --build --trace \
		-Wall -Wno-DECLFILENAME \
		--top-module core_top \
		-GADDR_WIDTH=16 -GINIT_FILE='"../sw/tests/fib.hex"' \
		$(RTL_DIR)/core/alu.sv \
		$(RTL_DIR)/core/control.sv \
		$(RTL_DIR)/core/regfile.sv \
		$(RTL_DIR)/core/imm_gen.sv \
		$(RTL_DIR)/core/branch_unit.sv \
		$(RTL_DIR)/core/imem.sv \
		$(RTL_DIR)/core/dmem.sv \
		$(RTL_DIR)/core/core_top.sv \
		$(TB_DIR)/tb_core_top.cpp \
		-o core_top_sim
	./$(BUILD_DIR)/core_top_sim

# ---------------------------------------------------------------------------
# Fase 1 - Memoria de instrucciones: lectura async, cargable desde .hex
# ---------------------------------------------------------------------------
imem:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module imem \
		-GADDR_WIDTH=16 -GINIT_FILE='"../tb/hex/imem_test.hex"' \
		$(RTL_DIR)/core/imem.sv \
		$(TB_DIR)/tb_imem.cpp \
		-o imem_sim
	./$(BUILD_DIR)/imem_sim

# ---------------------------------------------------------------------------
# Fase 1 - Memoria de datos: byte/halfword/word, signed/unsigned, sync write
# ---------------------------------------------------------------------------
dmem:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module dmem \
		$(RTL_DIR)/core/dmem.sv \
		$(TB_DIR)/tb_dmem.cpp \
		-o dmem_sim
	./$(BUILD_DIR)/dmem_sim

# ---------------------------------------------------------------------------
# Fase 1 - Unidad de branch: decide branch_taken a partir de funct3 + ALU
# ---------------------------------------------------------------------------
branch_unit:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module branch_unit \
		$(RTL_DIR)/core/branch_unit.sv \
		$(TB_DIR)/tb_branch_unit.cpp \
		-o branch_unit_sim
	./$(BUILD_DIR)/branch_unit_sim

# ---------------------------------------------------------------------------
# Fase 1 - Generador de inmediato: ensambla y sign-extiende I/S/B/U/J
# ---------------------------------------------------------------------------
imm_gen:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module imm_gen \
		$(RTL_DIR)/core/alu.sv \
		$(RTL_DIR)/core/control.sv \
		$(RTL_DIR)/core/imm_gen.sv \
		$(TB_DIR)/tb_imm_gen.cpp \
		-o imm_gen_sim
	./$(BUILD_DIR)/imm_gen_sim

# ---------------------------------------------------------------------------
# Fase 1 - Unidad de control: decodifica opcode/funct3/funct7
# ---------------------------------------------------------------------------
control:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module control \
		$(RTL_DIR)/core/alu.sv \
		$(RTL_DIR)/core/control.sv \
		$(TB_DIR)/tb_control.cpp \
		-o control_sim
	./$(BUILD_DIR)/control_sim

# ---------------------------------------------------------------------------
# Fase 1 - ALU: ADD/SUB/lógicas/shifts/comparaciones RV32I
# ---------------------------------------------------------------------------
alu:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module alu \
		$(RTL_DIR)/core/alu.sv \
		$(TB_DIR)/tb_alu.cpp \
		-o alu_sim
	./$(BUILD_DIR)/alu_sim

# ---------------------------------------------------------------------------
# Fase 1 - Register file: 32x32b, 2R/1W, x0 hardwireado, write-then-read
# ---------------------------------------------------------------------------
regfile:
	$(VERILATOR) --cc --exe --build \
		-Wall -Wno-DECLFILENAME \
		--top-module regfile \
		$(RTL_DIR)/core/regfile.sv \
		$(TB_DIR)/tb_regfile.cpp \
		-o regfile_sim
	./$(BUILD_DIR)/regfile_sim

# ---------------------------------------------------------------------------
# Fase 0 - Verilator: contador trivial, produce waveform .vcd
# ---------------------------------------------------------------------------
hello_verilator:
	$(VERILATOR) --cc --exe --build --trace \
		-Wall -Wno-DECLFILENAME \
		--top-module hello_counter \
		$(RTL_DIR)/core/hello_counter.sv \
		$(TB_DIR)/tb_hello_counter.cpp \
		-o hello_counter_sim
	./$(BUILD_DIR)/hello_counter_sim

# ---------------------------------------------------------------------------
# Fase 0 - Toolchain RISC-V: compila un .c minimo a .elf y extrae .hex/.bin
# ---------------------------------------------------------------------------
hello_riscv:
	$(RISCV_CC) $(RISCV_FLAGS) -T $(SW_DIR)/crt/link.ld \
		$(SW_DIR)/crt/start.s $(SW_DIR)/tests/hello.c \
		-o hello_riscv.elf
	$(RISCV_OBJCOPY) -O verilog hello_riscv.elf hello_riscv.hex
	$(RISCV_OBJCOPY) -O binary hello_riscv.elf hello_riscv.bin
	@echo ""
	@echo "=== hello_riscv.elf generado correctamente ==="
	@riscv64-unknown-elf-size hello_riscv.elf

clean:
	rm -rf $(BUILD_DIR) *.vcd *.elf *.hex *.bin
