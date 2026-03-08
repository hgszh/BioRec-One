`include "fir_lpf/fir_lpf.sv"
`include "iir_hpf/iir_hpf.v"
`include "iir_notch/iir_notch.v"
`include "reset_iir_on_ring/reset_iir_on_ring.v"

module FILTER_PROCESS (
    input i_rst_n,
    input i_fpga_clk,

    input signed [23 : 0] i_data,
    input                 i_data_valid,

    input [1:0] i_lpf_coeffi_sel,
    input       i_hpf_bypass,
    input       i_notch_bypass,

    output reg signed [23:0] o_data,
    output                   o_data_valid,
    output                   o_iir_ring_alarm
);
    reg [1:0] i_lpf_coeffi_sel_reg;
    reg i_hpf_bypass_reg, i_notch_bypass_reg;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            i_lpf_coeffi_sel_reg <= 2'd0;
            i_hpf_bypass_reg <= 1'd0;
            i_notch_bypass_reg <= 1'd0;
        end
        else begin
            i_lpf_coeffi_sel_reg <= i_lpf_coeffi_sel;
            i_hpf_bypass_reg <= i_hpf_bypass;
            i_notch_bypass_reg <= i_notch_bypass;
        end
    end

    function automatic signed [23:0] sat_slice;
        input logic signed [53:0] data_in;
        input logic [5:0] shift_amt;
        logic signed [53:0] tmp;
        begin
            tmp = data_in >>> shift_amt;
            if (tmp > 24'sh7F_FFFF) sat_slice = 24'sh7F_FFFF;  // 正饱和
            else if (tmp < 24'sh80_0000) sat_slice = 24'sh80_0000;  // 负饱和
            else sat_slice = tmp[23:0];  // 正常截断
        end
    endfunction

    reg signed [23 : 0] lpf_40_coeffi [0 : 199]  /* synthesis syn_romstyle = "block_rom" */;
    reg signed [23 : 0] lpf_100_coeffi[0 : 199]  /* synthesis syn_romstyle = "block_rom" */;
    reg signed [23 : 0] lpf_200_coeffi[0 : 199]  /* synthesis syn_romstyle = "block_rom" */;
    initial begin
        $readmemb("lpf_40_coeffi.txt", lpf_40_coeffi);
        $readmemb("lpf_100_coeffi.txt", lpf_100_coeffi);
        $readmemb("lpf_200_coeffi.txt", lpf_200_coeffi);
    end
    wire signed [53:0] lpf_data;
    wire               lpf_data_valid;
    FIR_FILTER lpf_filter (
        .i_rst_n     (i_rst_n),
        .i_fpga_clk  (i_fpga_clk),
        .i_data      (i_data),
        .i_data_valid(i_data_valid),
        .o_data      (lpf_data),
        .o_data_valid(lpf_data_valid),
        .coeffi_sel  (i_lpf_coeffi_sel_reg),
        .coeffi0     (lpf_40_coeffi),
        .coeffi1     (lpf_100_coeffi),
        .coeffi2     (lpf_200_coeffi)
    );

    reg lpf_data_valid_reg;
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if (!i_rst_n) lpf_data_valid_reg <= 1'b0;
        else lpf_data_valid_reg <= lpf_data_valid;
    wire common_valid = (!lpf_data_valid_reg) && (lpf_data_valid);

    reg signed [23:0] lpf_data_24;
    localparam [5:0] LPF_SHIFT_ARR[0:2] = '{28, 27, 25};
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) lpf_data_24 <= 24'b0;
        else lpf_data_24 <= sat_slice($signed(lpf_data), LPF_SHIFT_ARR[i_lpf_coeffi_sel_reg]);
    end

    wire iir_reset = i_rst_n & o_iir_ring_alarm;

    wire signed [23:0] hpf_data;
    wire hpf_valid = common_valid && (!i_hpf_bypass_reg);
    IIR_HPF hpf_filter (
        .i_rst_n   (iir_reset),
        .i_fpga_clk(i_fpga_clk),
        .i_valid   (hpf_valid),
        .i_data    (lpf_data_24),
        .o_data    (hpf_data)
    );
    wire signed [23:0] hpf_or_bypass_data = i_hpf_bypass_reg ? lpf_data_24 : hpf_data;

    wire notch_valid = common_valid && (!i_notch_bypass_reg);
    localparam NUM_NOTCH_STAGES = 2;
    wire signed [23:0] notch_out[0:NUM_NOTCH_STAGES];
    assign notch_out[0] = hpf_or_bypass_data;
    genvar i;
    generate
        for (i = 0; i < NUM_NOTCH_STAGES; i = i + 1) begin : notch_gen
            IIR_NOTCH notch_filter (
                .i_rst_n   (iir_reset),
                .i_fpga_clk(i_fpga_clk),
                .i_valid   (notch_valid),
                .i_data    (notch_out[i]),
                .o_data    (notch_out[i+1])
            );
        end
    endgenerate
    wire signed [23:0] notch_or_bypass_data = i_notch_bypass_reg ? notch_out[0] : notch_out[NUM_NOTCH_STAGES];

    wire signed [11:0] iir_data_scaled = notch_or_bypass_data >>> 12;
    RESET_IIR_ON_RING rst_iir_on_ring (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (iir_data_scaled),
        .i_valid   (common_valid),
        .o_alarm   (o_iir_ring_alarm)
    );
    assign o_data       = notch_or_bypass_data;
    assign o_data_valid = common_valid;

endmodule
