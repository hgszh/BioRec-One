`ifndef PT_UTILS_H
`define PT_UTILS_H

/*************************************************************************************/
module PT_IIR_LPF (
    input                      i_rst_n,
    input                      i_fpga_clk,
    input                      i_valid,
    input  signed     [11 : 0] i_data,
    output reg signed [11 : 0] o_data
);
    wire signed [11:0] yin;
    reg signed  [11:0] xin_reg[11:0];
    reg signed  [11:0] yin_reg[11:0];
    integer            j;
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if (!i_rst_n) begin
            for (j = 0; j < 12; j = j + 1) xin_reg[j] <= 12'd0;
            for (j = 0; j < 4; j = j + 1) yin_reg[j] <= 12'd0;
        end
        else if (i_valid) begin
            for (j = 11; j > 0; j = j - 1) xin_reg[j] <= xin_reg[j-1];
            xin_reg[0] <= i_data;
            for (j = 11; j > 0; j = j - 1) yin_reg[j] <= yin_reg[j-1];
            yin_reg[0] <= yin;
        end
    wire signed [13:0] x_out = i_data - (xin_reg[5] <<< 1) + (xin_reg[11]);  //x(n)-2*x(n-6)+x(n-12)
    wire signed [13:0] y_out = (yin_reg[0] <<< 1) - yin_reg[1];  //2*y(n-1)-y(n-2)
    wire signed [15:0] y_sum = x_out + y_out;
    assign yin = y_sum[13:2];
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if (!i_rst_n) o_data <= 12'b0;
        else if (i_valid) o_data <= yin;
endmodule

/*************************************************************************************/
module PT_IIR_HPF (
    input                    i_rst_n,
    input                    i_fpga_clk,
    input                    i_valid,
    input  signed     [11:0] i_data,
    output reg signed [11:0] o_data
);
    reg signed [11:0] xin_reg[0:31];  // x[n]~x[n-31]
    reg signed [11:0] yin_reg;  // 仅保留y[n-1]
    wire signed [15:0] x_div_32 = i_data >>> 5;  // x[n]/32
    wire signed [15:0] x_16 = xin_reg[15];  // x[n-16]
    wire signed [15:0] x_17 = xin_reg[16];  // x[n-17]
    wire signed [15:0] x_32_div = xin_reg[31] >>> 5;  // x[n-32]/32
    wire signed [15:0] y_sum = yin_reg - x_div_32 + x_16 - x_17 + x_32_div;  // 累加器结果
    wire signed [11:0] yin = y_sum[11:0];
    integer j;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            for (j = 0; j < 32; j = j + 1) xin_reg[j] <= 12'sd0;
            yin_reg <= 12'sd0;
            o_data  <= 12'sd0;
        end
        else if (i_valid) begin
            for (j = 31; j > 0; j = j - 1) xin_reg[j] <= xin_reg[j-1];
            xin_reg[0] <= i_data;
            yin_reg <= yin;
            o_data <= yin;
        end
    end
endmodule

/*************************************************************************************/
// 5-point derivative filter
module FIR_DERIVATIVE (
    input                      i_rst_n,
    input                      i_fpga_clk,
    input                      i_valid,
    input  signed     [11 : 0] i_data,
    output reg signed [11 : 0] o_data
);
    reg signed [11:0] samp[3:0];
    integer           k;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            for (k = 0; k < 4; k = k + 1) samp[k] <= 12'd0;
        end
        else if (i_valid) begin
            samp[3] <= samp[2];
            samp[2] <= samp[1];
            samp[1] <= samp[0];
            samp[0] <= i_data;
        end
    end
    wire signed [12:0] diff1 = {i_data[11], i_data} - {samp[3][11], samp[3]};  // x(n)-x(n-4)
    wire signed [12:0] diff2 = {samp[0][11], samp[0]} - {samp[2][11], samp[2]};  // x(n-1)-x(n-3)
    wire signed [13:0] sum = (diff1 <<< 1) + diff2;
    wire signed [13:0] sum_div = sum >>> 3;
    wire signed [11:0] y = sum_div[11:0];
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_data <= 12'd0;
        else if (i_valid) o_data <= y;
    end
endmodule

/*************************************************************************************/
// 12-bit square, take bits 16:5
module SQUARE (
    input                i_fpga_clk,
    input                i_rst_n,
    input  signed [11:0] i_data,
    input                i_valid,
    output reg    [11:0] o_data
);
    wire [11:0] abs_in = i_data[11] ? -i_data : i_data;
    // Stage0: 分三段并寄存
    reg [23:0] part0, part1, part2;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            part0 <= 0;
            part1 <= 0;
            part2 <= 0;
        end
        else begin
            part0 <= (abs_in[3:0] == 4'b0000) ? 0 : (abs_in[0] ? abs_in << 0 : 0) + (abs_in[1] ? abs_in << 1 : 0) + (abs_in[2] ? abs_in << 2 : 0) + (abs_in[3] ? abs_in << 3 : 0);
            part1 <= (abs_in[7:4] == 4'b0000) ? 0 : (abs_in[4] ? abs_in << 4 : 0) + (abs_in[5] ? abs_in << 5 : 0) + (abs_in[6] ? abs_in << 6 : 0) + (abs_in[7] ? abs_in << 7 : 0);
            part2 <= (abs_in[11:8] == 4'b0000) ? 0 : (abs_in[8] ? abs_in << 8 : 0) + (abs_in[9] ? abs_in << 9 : 0) + (abs_in[10] ? abs_in << 10 : 0) + (abs_in[11] ? abs_in << 11 : 0);
        end
    end
    // Stage1: 合并两段并寄存
    reg [23:0] sum01;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) sum01 <= 0;
        else sum01 <= part0 + part1;
    end
    // Stage2: 最终合并并输出
    reg [23:0] total;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) total <= 0;
        else total <= sum01 + part2;
    end
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_data <= 0;
        else if (i_valid) o_data <= total[16:5];
    end
endmodule

/*************************************************************************************/
// 32-point moving average
module MOVAVG_32 (
    input             i_fpga_clk,
    input             i_rst_n,
    input      [11:0] i_data,
    input             i_valid,
    output reg [11:0] o_data
);
    reg  [11:0] data_buffer[0:31];
    reg  [17:0] sum_reg;
    reg  [ 4:0] ptr;
    wire [17:0] next_sum;
    assign next_sum = sum_reg - data_buffer[ptr] + i_data;
    integer i;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            sum_reg <= 18'd0;
            ptr     <= 5'd0;
            o_data  <= 12'd0;
            for (i = 0; i < 32; i = i + 1) begin
                data_buffer[i] <= 12'd0;
            end
        end
        else if (i_valid) begin
            sum_reg          <= next_sum;
            data_buffer[ptr] <= i_data;
            ptr              <= ptr + 5'd1;
            o_data           <= next_sum[15:4];
        end
    end
endmodule

/*************************************************************************************/
// Preprocess
module PRE_PROCESSOR (
    input                i_fpga_clk,
    input                i_rst_n,
    input  signed [11:0] i_data,
    input                i_valid,
    output        [11:0] o_data
);
    // 1) Bandpass filter (5–11Hz)
    wire signed [11:0] o_lpf;
    PT_IIR_LPF lpf_inst (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (i_data),
        .i_valid   (i_valid),
        .o_data    (o_lpf)
    );
    wire signed [11:0] o_hpf;
    PT_IIR_HPF hpf_inst (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (o_lpf),
        .i_valid   (i_valid),
        .o_data    (o_hpf)
    );
    // 2) 5-point derivative (enhance QRS slope)
    wire signed [11:0] o_derivative;
    FIR_DERIVATIVE derivative_inst (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_valid   (i_valid),
        .i_data    (o_hpf),
        .o_data    (o_derivative)
    );
    // 3) Squaring (amplify peak differences)
    wire [11:0] o_square;
    SQUARE square_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_data    (o_derivative),
        .i_valid   (i_valid),
        .o_data    (o_square)
    );
    // 4) 32-point moving average (smooth envelope)
    MOVAVG_32 average_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_data    (o_square),
        .i_valid   (i_valid),
        .o_data    (o_data)
    );
endmodule

/*************************************************************************************/
// 9-point median filter
// Reference:  DOI 10.1007/s10710-016-9275-7
module MEDIAN_9 #(
    parameter WIDTH = 12
) (
    input                  i_fpga_clk,
    input                  i_rst_n,
    input      [WIDTH-1:0] i_data,
    input                  i_valid,
    output reg [WIDTH-1:0] o_data,
    output reg             o_valid
);
    reg [WIDTH-1:0] shift_reg[0:8];
    reg [3:0] count;
    localparam IDLE = 2'b00, PROCESSING = 2'b01, OUTPUT = 2'b10;
    reg [      1:0] state;
    reg [WIDTH-1:0] temp_array    [0:8];
    reg [      4:0] cycle_count;
    reg             pending_valid;
    reg [3:0] index_a, index_b;
    integer i;
    always @(*) begin
        case (cycle_count)  /* verilog_format: off */
        0:  begin index_a = 1; index_b = 2; end 1:  begin index_a = 4; index_b = 5; end 2:  begin index_a = 7; index_b = 8; end
        3:  begin index_a = 0; index_b = 1; end 4:  begin index_a = 3; index_b = 4; end 5:  begin index_a = 6; index_b = 7; end
        6:  begin index_a = 0; index_b = 3; end 7:  begin index_a = 1; index_b = 2; end 8:  begin index_a = 4; index_b = 5; end 
        9:  begin index_a = 7; index_b = 8; end 10: begin index_a = 3; index_b = 6; end 11: begin index_a = 4; index_b = 7; end
        12: begin index_a = 5; index_b = 8; end 13: begin index_a = 1; index_b = 4; end 14: begin index_a = 2; index_b = 5; end 
        15: begin index_a = 4; index_b = 7; end 16: begin index_a = 4; index_b = 2; end 17: begin index_a = 6; index_b = 4; end
        18: begin index_a = 4; index_b = 2; end default: begin index_a = 0; index_b = 0; end
    endcase  /* verilog_format: on */
    end
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            for (i = 0; i < 9; i = i + 1) begin
                shift_reg[i]  <= 0;
                temp_array[i] <= 0;
            end
            count <= 0;
            state <= IDLE;
            cycle_count <= 0;
            pending_valid <= 0;
            o_data <= 0;
            o_valid <= 0;
        end
        else begin
            o_valid <= 0;
            if (i_valid) begin
                for (i = 0; i < 8; i = i + 1) shift_reg[i] <= shift_reg[i+1];
                shift_reg[8] <= i_data;
                if (count < 9) count <= count + 1'd1;
                if (state != IDLE) pending_valid <= 1;
            end
            case (state)
                IDLE: begin
                    if (i_valid && count >= 9) begin
                        for (i = 0; i < 9; i = i + 1) temp_array[i] <= shift_reg[i];
                        state <= PROCESSING;
                        cycle_count <= 0;
                    end
                end
                PROCESSING: begin
                    if (temp_array[index_a] > temp_array[index_b]) begin
                        temp_array[index_a] <= temp_array[index_b];
                        temp_array[index_b] <= temp_array[index_a];
                    end
                    if (cycle_count < 18) cycle_count <= cycle_count + 1'd1;
                    else state <= OUTPUT;
                end
                OUTPUT: begin
                    o_data  <= temp_array[4];
                    o_valid <= 1;
                    if (pending_valid) begin
                        for (i = 0; i < 9; i = i + 1) temp_array[i] <= shift_reg[i];
                        state <= PROCESSING;
                        cycle_count <= 0;
                        pending_valid <= 0;
                    end
                    else state <= IDLE;
                end
            endcase
        end
    end
endmodule

/*************************************************************************************/
// 5ms timestamp accumulator
module MS5_TIMER (
    input             i_fpga_clk,
    input             i_rst_n,
    input             i_valid,
    output reg [31:0] o_ms_tick
);
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_ms_tick <= 32'd0;
        else if (i_valid) o_ms_tick <= o_ms_tick + 32'd5;
    end
endmodule

/*************************************************************************************/
// Peak detection: update on rising edge, confirm on 7/16 peak or timeout
module FIND_PEAK (
    input             i_fpga_clk,
    input             i_rst_n,
    input      [11:0] i_data,
    input             i_valid,
    input      [31:0] time_ms,
    output reg        o_valid,
    output reg [11:0] o_peak_data,
    output reg [31:0] o_peak_ms
);
    reg [11:0] peak_candidate;
    reg [31:0] peak_candidate_ms;
    reg confirm_flag;
    wire [11:0] half_peak = peak_candidate >> 1;
    wire [11:0] seven_sixteenths_peak = half_peak - (peak_candidate >> 4);
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            peak_candidate    <= 12'd0;
            peak_candidate_ms <= 32'd0;
            o_peak_data       <= 12'd0;
            o_peak_ms         <= 32'd0;
            o_valid           <= 1'd0;
            confirm_flag      <= 1'b0;
        end
        else if (i_valid) begin
            o_valid <= 1'd0;
            if (i_data > peak_candidate) begin  // rising edge
                peak_candidate <= i_data;
                peak_candidate_ms <= time_ms;
                confirm_flag <= 1'b0;
            end
            else if ((i_data < seven_sixteenths_peak) && (confirm_flag == 0)) begin  // 7/16 peak confirm
                o_peak_data <= peak_candidate;
                o_peak_ms <= peak_candidate_ms;
                peak_candidate <= half_peak;  // reset candidate
                o_valid <= 1'd1;
                confirm_flag <= 1'b1;  // prevent repeated o_valid assertions
            end
            else if ((time_ms - peak_candidate_ms >= 32'd175) && (confirm_flag == 0)) begin  // timeout confirm
                o_peak_data    <= peak_candidate;
                o_peak_ms <= peak_candidate_ms;
                peak_candidate <=half_peak;// reset candidate
                o_valid <=1'd1;
                confirm_flag <= 1'b1;  // prevent repeated o_valid assertions
            end
        end
        else o_valid <= 1'd0;
    end
endmodule

/*************************************************************************************/
// Refractory gate: masks signal for 200ms after R-wave detection
// Blocks signal by outputting zero within refractory period
module REFR_GATE #(
    // A short refractory period (20ms) is used to allow heart rates up to 300 bpm.
    // With a 200 ms refractory period, the maximum detectable rate would be 250 bpm.
    parameter integer REFRACTORY_TIME_MS = 140
) (
    input             i_fpga_clk,
    input             i_rst_n,
    input      [31:0] time_ms,
    input             i_valid,
    input             i_refractory_trigger,
    output reg        o_valid_masked
);
    reg  [31:0] refractory_end_time;
    wire        refractory_active;
    assign refractory_active = (time_ms < refractory_end_time);
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) refractory_end_time <= 32'd0;
        else if (i_refractory_trigger) refractory_end_time <= time_ms + REFRACTORY_TIME_MS;
    end
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) o_valid_masked <= 1'd0;
        else if (refractory_active) o_valid_masked <= 1'd0;
        else o_valid_masked <= i_valid;
    end
endmodule

`endif
