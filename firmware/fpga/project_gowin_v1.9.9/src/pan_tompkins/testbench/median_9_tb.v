`timescale 1ns / 1ns

module tb_median_9;

    reg         i_rst_n;
    reg         i_fpga_clk;
    reg         i_valid;
    reg  [11:0] data_in;
    wire [11:0] data_out;
    wire        o_valid;

    MEDIAN_9 #(
        .WIDTH(12)
    ) uut (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (data_in),
        .i_valid   (i_valid),
        .o_data    (data_out),
        .o_valid   (o_valid)
    );

    initial begin
        i_fpga_clk = 0;
        forever #(28) i_fpga_clk = ~i_fpga_clk;
    end

    reg [11:0] data[0:8];
    integer i, j;
    integer rnd;
    initial begin
        i_rst_n = 0;
        i_valid = 0;
        data_in = 0;
        #(100);
        i_rst_n = 1;
        for (i = 0; i < 20; i = i + 1) begin
            #(1000000);
            data_in = i + 1;
            i_valid = 1;
            @(posedge i_fpga_clk);
            i_valid = 0;
            @(posedge i_fpga_clk);
        end
        for (i = 0; i < 1000; i = i + 1) begin
            for (j = 0; j < 9; j = j + 1) begin
                #(1000000);
                @(posedge i_fpga_clk);
                rnd = $random;
                if (rnd < 0) rnd = -rnd;
                data_in = rnd%4000;
                i_valid = 1;
                @(posedge i_fpga_clk);
                i_valid = 0;
            end
        end
    end

endmodule
