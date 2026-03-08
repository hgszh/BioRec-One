`ifndef IIR_HPF_H
`define IIR_HPF_H

//0.67Hz highpass filter
module IIR_HPF (
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
    // 523854*[x(n)+x(n-2)]
    wire signed [43:0] x_mult_reg1 = ((x_add_reg <<< 19) - (x_add_reg <<< 9)) + ((x_add_reg <<< 6) + (x_add_reg <<< 3)) + ((x_add_reg <<< 2) + (x_add_reg <<< 1));
    // 1047708*x(n-1)
    wire signed [43:0] x_mult_reg2 = ((xin_reg[0] <<< 20) - (xin_reg[0] <<< 10)) + ((xin_reg[0] <<< 7) + (xin_reg[0] <<< 4)) + ((xin_reg[0] <<< 3) + (xin_reg[0] <<< 2));
    // 523854*[x(n)+x(n-2)]-1047708*x(n-1)
    wire signed [44:0] x_out = x_mult_reg1 - x_mult_reg2;

    // 1047708*y(n-1)
    wire signed [43:0] y_mult_reg1 = ((yin_reg[0] <<< 20) - (yin_reg[0] <<< 10)) + ((yin_reg[0] <<< 7) + (yin_reg[0] <<< 4)) + ((yin_reg[0] <<< 3) + (yin_reg[0] <<< 2));
    // 523421*y(n-2)
    wire signed [43:0] y_mult_reg2 = ((yin_reg[1] <<< 19) - (yin_reg[1] <<< 10)) + ((yin_reg[1] <<< 7) + (yin_reg[1] <<< 4)) + ((yin_reg[1] <<< 3) + (yin_reg[1] <<< 2) + yin_reg[1]);
    // 1047708*y(n-1)-523421*y(n-2)
    wire signed [44:0] y_out = y_mult_reg1 - y_mult_reg2;

    wire signed [45:0] y_sum = x_out + y_out;
    wire signed [18:0] bias = y_sum[45] ? -19'sd262144 : 19'sd262144;
    wire signed [26:0] y_div = (y_sum + bias) >>> 19;
    assign yin = (i_rst_n ? y_div[23:0] : 24'd0);

    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_data <= 24'b0;
        else o_data <= yin;
    end

endmodule

`endif
