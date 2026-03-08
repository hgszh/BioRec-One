`ifndef FIR_LPF_H
`define FIR_LPF_H

`include "basic_fir_filter.v"

module FIR_FILTER #(
    parameter integer tapsize = 200,
    parameter integer icoeffi_width = 24,
    parameter integer idata_width = 24
) (
    input                                     i_rst_n,
    input                                     i_fpga_clk,
    input  signed     [  idata_width - 1 : 0] i_data,
    input                                     i_data_valid,
    output reg signed [           54 - 1 : 0] o_data,
    output                                    o_data_valid,
    input             [                  1:0] coeffi_sel,
    input  signed     [icoeffi_width - 1 : 0] coeffi0     [0 : tapsize - 1],
    input  signed     [icoeffi_width - 1 : 0] coeffi1     [0 : tapsize - 1],
    input  signed     [icoeffi_width - 1 : 0] coeffi2     [0 : tapsize - 1]
);
    reg [1:0] last_coeffi_sel;
    reg coeffi_reload;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            last_coeffi_sel <= 2'b0;
            coeffi_reload   <= 1'b0;
        end
        else begin
            if (last_coeffi_sel != coeffi_sel) begin
                coeffi_reload <= 1'b1;
            end
            else begin
                coeffi_reload <= 1'b0;
            end
            last_coeffi_sel <= coeffi_sel;
        end
    end

    reg i_fir_ini, wr_en_data, wr_en_coeffi;
    wire signed [               53 : 0] dout;
    reg signed  [icoeffi_width - 1 : 0] coeffi_in;
    wire                                input_rdy;
    Basic_FIR_Filter_Top basic_fir_filter_inst (
        .clk         (i_fpga_clk),
        .rst_n       (i_rst_n),
        .ini         (i_fir_ini),
        .wr_en_data  (wr_en_data),
        .wr_en_coeffi(wr_en_coeffi),
        .din_coeffi  (coeffi_in),
        .din_data    (i_data),
        .dout        (dout),
        .done        (o_data_valid),
        .input_rdy   (input_rdy)
    );

    localparam coeffi_addr_width = $clog2(tapsize);
    reg                           filter_init_delay_done;
    reg [coeffi_addr_width - 1:0] delay_cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n || coeffi_reload) begin
            delay_cnt <= 1'b0;
            filter_init_delay_done <= 1'b0;
            i_fir_ini <= 1'b0;
        end
        else if (filter_init_delay_done == 1'b0) begin
            delay_cnt <= delay_cnt + 1'b1;
            i_fir_ini <= 1'b1;
            if (delay_cnt == tapsize) begin
                delay_cnt <= 1'b0;
                filter_init_delay_done <= 1'b1;
                i_fir_ini <= 1'b0;
            end
        end
    end

    reg [coeffi_addr_width - 1 : 0] cnt_wr_coeffi;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n || coeffi_reload) begin
            cnt_wr_coeffi <= 1'b0;
            wr_en_coeffi <= 1'b0;
            coeffi_in <= 1'b0;
        end
        else if ((filter_init_delay_done) && (cnt_wr_coeffi < tapsize)) begin
            cnt_wr_coeffi <= cnt_wr_coeffi + 1'b1;
            wr_en_coeffi  <= 1'b1;
            case (coeffi_sel)
                0:       coeffi_in <= coeffi0[cnt_wr_coeffi];
                1:       coeffi_in <= coeffi1[cnt_wr_coeffi];
                2:       coeffi_in <= coeffi2[cnt_wr_coeffi];
                default: coeffi_in <= coeffi0[cnt_wr_coeffi];
            endcase
        end
        else if (cnt_wr_coeffi == tapsize) wr_en_coeffi <= 1'b0;
    end

    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n || coeffi_reload) wr_en_data <= 1'b0;
        else if (i_data_valid && input_rdy && (cnt_wr_coeffi == tapsize)) wr_en_data <= 1'b1;
        else wr_en_data <= 1'b0;
    end

    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n || coeffi_reload) o_data <= 54'b0;
        else o_data <= o_data_valid ? dout : o_data;
    end

endmodule

`endif
