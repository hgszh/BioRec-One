module pre_processor (
    input               i_fpga_clk,
    input               i_rst_n,
    input signed [11:0] i_data,
    input               i_valid,

    output signed [11:0] o_bpf,
    output signed [11:0] o_derivative,
    output        [11:0] o_square,
    output        [11:0] o_average,
    output        [11:0] o_peak_data,
    output        [31:0] o_peak_ms,
    output               o_peak_valid
);

    IIR_BPF bpf_inst (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (i_data),
        .i_valid   (i_valid),
        .o_data    (o_bpf)
    );

    FIR_DERIVATIVE derivative_inst (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_valid   (i_valid),
        .i_data    (o_bpf),
        .o_data    (o_derivative)
    );

    SQUARE square_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_data    (o_derivative),
        .i_valid   (i_valid),
        .o_data    (o_square)
    );

    MOVAVG_32 average_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_data    (o_square),
        .i_valid   (i_valid),
        .o_data    (o_average)
    );

    wire [31:0] o_ms_tick;
    MS5_TIMER timer_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_valid   (i_valid),
        .o_ms_tick (o_ms_tick)
    );

    FIND_PEAK findpeak_inst (
        .i_fpga_clk (i_fpga_clk),
        .i_rst_n    (i_rst_n),
        .i_data     (o_average),
        .i_valid    (i_valid),
        .time_ms    (o_ms_tick),
        .o_valid    (o_peak_valid),
        .o_peak_data(o_peak_data),
        .o_peak_ms  (o_peak_ms)
    );
endmodule

/********************************************************************************************************/
`timescale 1ns / 1ns
module tb_pan_tompkins;
    reg               i_rst_n;
    reg               i_fpga_clk;
    reg               i_valid;
    reg signed [11:0] data_in;
    /*   wire signed [11:0] o_bpf;
    wire        [11:0] o_derivative;
    wire        [11:0] o_square;
    wire        [11:0] o_average;
    wire        [11:0] o_peak_data;
    wire        [31:0] o_peak_ms;
    wire               peak_valid;
 pre_processor uut (
        .i_rst_n     (i_rst_n),
        .i_fpga_clk  (i_fpga_clk),
        .i_data      (data_in),
        .i_valid     (i_valid),
        .o_bpf       (o_bpf),
        .o_derivative(o_derivative),
        .o_square    (o_square),
        .o_average   (o_average),
        .o_peak_valid(o_peak_valid),
        .o_peak_data (o_peak_data),
        .o_peak_ms   (o_peak_ms)
    );
*/
    wire              o_r_peak_pulse;
    wire       [11:0] o_rr_interval_ms;
    PAN_TOMPKINS uut (
        .i_rst_n         (i_rst_n),
        .i_fpga_clk      (i_fpga_clk),
        .i_data          (data_in),
        .i_valid         (i_valid),
        .o_r_peak_pulse  (o_r_peak_pulse),
        .o_rr_interval_ms(o_rr_interval_ms)
    );
    // 18MHz
    initial begin
        i_fpga_clk = 0;
        forever #(28) i_fpga_clk = ~i_fpga_clk;
    end

    reg [11:0] ecg[0:2267-1];
    integer    file_handle;
    integer    read_count;
    integer idx;
    initial begin
        file_handle = $fopen("ecg_12bit_200Hz.csv", "r");
        if (file_handle == 0) begin
            $display("Error: cannot open CSV");
        end
        idx = 0;
        //verilog_format: off
        while (!$feof(file_handle) && idx < 2267) begin
            read_count = $fscanf(file_handle, "%d\n", ecg[idx]);
            if (read_count != 1) $display("Warning: parse error at index %0d", idx);
            else idx = idx + 1;
        end
        //verilog_format: on
        $fclose(file_handle);
        $display("Read %0d samples from CSV", idx);
    end

    integer i, j;
    initial begin
        i_rst_n = 0;
        data_in = 0;
        i_valid = 0;
        #(300);
        i_rst_n = 1;
        #(300);
        if (idx == 2267) begin
            for (j = 0; j < 5; j = j + 1) begin
                for (i = 0; i < 2267; i = i + 1) begin
                    #(1e4);
                    @(posedge i_fpga_clk);
                    data_in = ecg[i];
                    i_valid = 1;
                    @(posedge i_fpga_clk);
                    i_valid = 0;
                end
            end
        end
    end

endmodule
