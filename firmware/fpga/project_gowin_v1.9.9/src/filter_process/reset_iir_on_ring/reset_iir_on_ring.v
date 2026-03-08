module RESET_IIR_ON_RING (
    input                  i_rst_n,
    input                  i_fpga_clk,
    input  signed [11 : 0] i_data,
    input                  i_valid,
    output reg             o_alarm
);

    /*****************************************10Hz HPF Filter************************************************************/
    reg signed  [11:0] xin_reg[1:0];
    wire signed [11:0] yin;
    reg signed  [11:0] yin_reg[1:0];
    always @(posedge i_fpga_clk or negedge i_rst_n)
        if ((!i_rst_n) || (!o_alarm)) begin
            xin_reg[0] <= 12'd0;
            xin_reg[1] <= 12'd0;
            yin_reg[0] <= 12'd0;
            yin_reg[1] <= 12'd0;
        end
        else if (i_valid) begin
            xin_reg[1] <= xin_reg[0];
            xin_reg[0] <= i_data;
            yin_reg[1] <= yin_reg[0];
            yin_reg[0] <= yin;
        end

    // x(n) + x(n-2)
    wire signed [12:0] x_add_reg = {i_data[11], i_data} + {xin_reg[1][11], xin_reg[1]};
    // 1810*[x(n)+x(n-2)] = 1024 + 512 + 256 + 16 + 2
    wire signed [23:0] x1810_part1 = (x_add_reg <<< 10) + (x_add_reg <<< 9);  // 1024+512
    wire signed [23:0] x1810_part2 = (x_add_reg <<< 8) + (x_add_reg <<< 4);  // 256+16
    wire signed [23:0] x1810_part3 = (x_add_reg <<< 1);  // 2
    wire signed [23:0] x_mult_reg1 = x1810_part1 + x1810_part2 + x1810_part3;
    // 3620*x(n-1) = 2048 + 1024 + 512 + 32 + 4
    wire signed [23:0] x3620_part1 = (xin_reg[0] <<< 11) + (xin_reg[0] <<< 10);  // 2048+1024
    wire signed [23:0] x3620_part2 = (xin_reg[0] <<< 9) + (xin_reg[0] <<< 5);  // 512+32
    wire signed [23:0] x3620_part3 = (xin_reg[0] <<< 2);  // 4
    wire signed [23:0] x_mult_reg2 = x3620_part1 + x3620_part2 + x3620_part3;
    // 1810*[x(n)+x(n-2)] - 3620*x(n-1)
    wire signed [23:0] x_out = x_mult_reg1 - x_mult_reg2;

    // 3593*y(n-1) = 2048 + 1024 + 512 + 8 + 1
    wire signed [23:0] y3593_part1 = (yin_reg[0] <<< 11) + (yin_reg[0] <<< 10);  // 2048+1024
    wire signed [23:0] y3593_part2 = (yin_reg[0] <<< 9);  // 512
    wire signed [23:0] y3593_part3 = (yin_reg[0] <<< 3) + yin_reg[0];  // 8 + 1
    wire signed [23:0] y_mult_reg1 = y3593_part1 + y3593_part2 + y3593_part3;
    // 1600*y(n-2) = 2048 - 448 (448=256+128+64)
    wire signed [23:0] y1600_sub = (yin_reg[1] <<< 8) + (yin_reg[1] <<< 7) + (yin_reg[1] <<< 6);  // 256+128+64
    wire signed [23:0] y_mult_reg2 = (yin_reg[1] <<< 11) - y1600_sub;  // 2048 - (256+128+64)
    // 3593*y(n-1) - 1600*y(n-2)
    wire signed [23:0] y_out = y_mult_reg1 - y_mult_reg2;

    wire signed [24:0] y_sum = x_out + y_out;
    wire signed [24:0] y_div = y_sum >>> 11;
    assign yin = (i_rst_n ? y_div[11:0] : 12'd0);

    /*****************************************Peak counter************************************************************/
    reg signed [11:0] hpf_out;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) hpf_out <= 12'd0;
        else begin
            if (yin[11]) hpf_out <= ~yin + 12'd1;  //取绝对值
            else hpf_out <= yin;
        end
    end

    localparam CLK_FREQ = 3_600, SEC_CNT_WIDTH = $clog2(CLK_FREQ);
    localparam HIGH_THRESH = 12'd200, LOW_THRESH = 12'd140, PEAK_LIMIT = 7'd5;
    localparam WAIT_HIGH = 1'b0, WAIT_LOW = 1'b1;
    reg state, next_state;
    always @(*) begin
        next_state = state;
        case (state)
            WAIT_HIGH: if (hpf_out > HIGH_THRESH) next_state = WAIT_LOW;
            WAIT_LOW:  if (hpf_out < LOW_THRESH) next_state = WAIT_HIGH;
        endcase
    end

    reg                     pause_once;  //屏蔽紧邻的alarm，避免连续触发造成振荡
    reg                     prev_hpf_above;
    reg [SEC_CNT_WIDTH-1:0] sec_cnt;
    reg [             11:0] peak_cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            state    <= WAIT_HIGH;
            sec_cnt  <= {SEC_CNT_WIDTH{1'b0}};
            peak_cnt <= 1'd0;
            o_alarm    <= 1'b1;
            prev_hpf_above <= 1'b0;
            pause_once <= 1'b0;
        end
        else if (i_valid) begin
            state <= next_state;
            sec_cnt <= sec_cnt + 1'b1;
            prev_hpf_above <= (hpf_out > LOW_THRESH);
            if (sec_cnt > CLK_FREQ / 2) begin
                sec_cnt <= {SEC_CNT_WIDTH{1'b0}};
                o_alarm <= ((peak_cnt < PEAK_LIMIT) || pause_once);
                pause_once <= 1'b0;
                peak_cnt <= 1'd0;
            end
            else if (state == WAIT_LOW && prev_hpf_above && hpf_out < LOW_THRESH) peak_cnt <= peak_cnt + 1'b1;
            if (!o_alarm) begin
                o_alarm <= 1'b1;
                pause_once <= 1'b1;
            end
        end
    end

endmodule
