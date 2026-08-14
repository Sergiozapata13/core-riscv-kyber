// forwarding_unit.sv
//
// Unidad de forwarding — Fase 2 (pipeline de 5 etapas).
//
// Resuelve hazards de datos SIN perder ciclos: si la instruccion en EX
// necesita un operando que una instruccion posterior (una o dos
// posiciones adelante en el pipeline) todavia no escribio al regfile,
// esta unidad detecta la dependencia y selecciona la fuente correcta
// para el mux de entrada de la ALU, en vez de forzar un stall.
//
// Dos rutas de forwarding:
//   EX/MEM -> EX : la instruccion inmediatamente anterior (una posicion
//                  adelante) ya calculo su resultado en su propia EX, y
//                  esta sentado en ex_mem_reg.
//   MEM/WB -> EX : la instruccion de dos posiciones adelante ya paso por
//                  MEM y esta en mem_wb_reg, a punto de escribirse en WB.
//
// Prioridad: si AMBAS fuentes escriben al mismo registro que se
// necesita, EX/MEM gana — es el resultado mas reciente (caso clasico de
// doble dependencia: add x1,.. / add x1,.. / usa x1).
//
// x0 nunca se forwardea: si rd_ex_mem=0 o rd_mem_wb=0, esa fuente se
// descarta aunque reg_write este activo — el regfile ya garantiza que
// x0 siempre vale 0, forwardear "el valor que se iba a escribir a x0"
// no tiene sentido y ademas x0 puede aparecer como rd de una instruccion
// que legitimamente no quiere escribir nada (ej. sub x0, x5, x5 usado
// como no-op con side effect nulo).
//
// alu_op no se incluyen aca: esta unidad solo decide DE DONDE viene el
// dato, no que hace la ALU con el — esa decision ya la tomo control.sv
// en ID y viaja en id_ex_reg.

package forwarding_pkg;
    typedef enum logic [1:0] {
        FWD_NONE   = 2'b00,  // usar rs_data tal cual viene de id_ex_reg (del regfile)
        FWD_EX_MEM = 2'b01,  // forwardear desde ex_mem_reg.alu_result
        FWD_MEM_WB = 2'b10   // forwardear desde mem_wb_reg (mux de WB)
    } fwd_sel_t;
endpackage

module forwarding_unit
    import forwarding_pkg::*;
(
    input  logic [4:0]  id_ex_rs1,
    input  logic [4:0]  id_ex_rs2,

    input  logic [4:0]  ex_mem_rd,
    input  logic         ex_mem_reg_write,

    input  logic [4:0]  mem_wb_rd,
    input  logic         mem_wb_reg_write,

    output fwd_sel_t     fwd_a,   // selector para el operando rs1 (A) de la ALU
    output fwd_sel_t     fwd_b    // selector para el operando rs2 (B) de la ALU
);

    always_comb begin
        // ---- Operando A (rs1) ----
        if (ex_mem_reg_write && (ex_mem_rd != 5'd0) && (ex_mem_rd == id_ex_rs1)) begin
            fwd_a = FWD_EX_MEM;
        end else if (mem_wb_reg_write && (mem_wb_rd != 5'd0) && (mem_wb_rd == id_ex_rs1)) begin
            fwd_a = FWD_MEM_WB;
        end else begin
            fwd_a = FWD_NONE;
        end

        // ---- Operando B (rs2) ----
        if (ex_mem_reg_write && (ex_mem_rd != 5'd0) && (ex_mem_rd == id_ex_rs2)) begin
            fwd_b = FWD_EX_MEM;
        end else if (mem_wb_reg_write && (mem_wb_rd != 5'd0) && (mem_wb_rd == id_ex_rs2)) begin
            fwd_b = FWD_MEM_WB;
        end else begin
            fwd_b = FWD_NONE;
        end
    end

endmodule
