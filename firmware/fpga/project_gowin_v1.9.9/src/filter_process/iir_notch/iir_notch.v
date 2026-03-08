`ifndef IIR_NOTCH_H
`define IIR_NOTCH_H

module IIR_NOTCH (
    input                      i_rst_n,
    input                      i_fpga_clk,
    input                      i_valid,
    input  signed     [23 : 0] i_data,
    output reg signed [23 : 0] o_data
);
    reg signed [23:0] xin_reg[1:0];
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if (!i_rst_n) begin
            xin_reg[0] <= 24'd0;
            xin_reg[1] <= 24'd0;
        end
        else if (i_valid) begin
            xin_reg[1] <= xin_reg[0];
            xin_reg[0] <= i_data;
        end

    wire signed [23:0] yin;
    reg signed  [23:0] yin_reg[1:0];
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if (!i_rst_n) begin
            yin_reg[0] <= 24'd0;
            yin_reg[1] <= 24'd0;
        end
        else if (i_valid) begin
            yin_reg[1] <= yin_reg[0];
            yin_reg[0] <= yin;
        end

    // x(n)+x(n-2)
    wire signed [24:0] x_add_reg = {i_data[23], i_data} + {xin_reg[1][23], xin_reg[1]};
    // 16355*[x(n)+x(n-2)]
    wire signed [38:0] x_mult_reg1 = ((x_add_reg <<< 14) - (x_add_reg <<< 5)) + ((x_add_reg <<< 1) + x_add_reg);
    // 32585*x(n-1)
    wire signed [38:0] x_mult_reg2 = ((xin_reg[0] <<< 15) - (xin_reg[0] <<< 8)) + ((xin_reg[0] <<< 6) + (xin_reg[0] <<< 3) + xin_reg[0]);
    // 16355*[x(n)+x(n-2)]-32585*x(n-1)
    wire signed [39:0] x_out = x_mult_reg1 - x_mult_reg2;

    // 32585*y(n-1)
    wire signed [38:0] y_mult_reg1 = ((yin_reg[0] <<< 15) - (yin_reg[0] <<< 8)) + ((yin_reg[0] <<< 6) + (yin_reg[0] <<< 3) + yin_reg[0]);
    // 16326*y(n-2)
    wire signed [38:0] y_mult_reg2 = ((yin_reg[1] <<< 14) - (yin_reg[1] <<< 6)) + ((yin_reg[1] <<< 2) + (yin_reg[1] <<< 1));
    //32585*y(n-1)-16326*y(n-2)
    wire signed [39:0] y_out = y_mult_reg1 - y_mult_reg2;

    wire signed [40:0] y_sum = x_out + y_out;
    wire signed [14:0] bias = y_sum[40] ? -15'sd8192 : 15'sd8192;
    wire signed [26:0] y_div = (y_sum + bias) >>> 14;
    assign yin = (i_rst_n ? y_div[23:0] : 24'd0);

    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_data <= 24'b0;
        else o_data <= yin;
    end

endmodule

`endif
