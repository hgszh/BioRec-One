module ADS1259 (
    input                    i_rst_n,
    input                    i_fpga_clk,
    output                   o_ads1259_clk,
    output                   o_ads1259_mosi,
    input                    i_ads1259_miso,
    input                    i_ads1259_drdy,
    output reg signed [23:0] o_sample_signed,
    output                   o_drdy_negedge
);

    reg  [7:0] spi_tx_byte;
    reg        spi_tx_dv;
    wire       spi_tx_ready;
    wire       spi_rx_dv;
    wire [7:0] spi_rx_byte;

    SPI_Master #(
        .SPI_MODE(1),
        .CLKS_PER_HALF_BIT(8)
    ) ads1259_spi (
        .i_Rst_L(i_rst_n),
        .i_Clk  (i_fpga_clk),

        .i_TX_Byte (spi_tx_byte),
        .i_TX_DV   (spi_tx_dv),
        .o_TX_Ready(spi_tx_ready),

        .o_RX_DV  (spi_rx_dv),
        .o_RX_Byte(spi_rx_byte),

        .o_SPI_Clk (o_ads1259_clk),
        .i_SPI_MISO(i_ads1259_miso),
        .o_SPI_MOSI(o_ads1259_mosi)
    );

    //====================================
    localparam S_DELAY = 5'b00000, S_SEND_INIT_SEQ = 5'b00001;
    localparam S_SEND_START = 5'b00010, S_SEND_RDATAC = 5'b00100, S_SEND_STOP = 5'b01000, S_SAMPLING = 5'b10000;
    reg [4:0] current_state  /* synthesis syn_encoding = "onehot" */;
    reg [4:0] next_state  /* synthesis syn_encoding = "onehot" */;
    reg [7:0] init_sequence[0:5];
    reg [2:0] init_seq_idx, next_init_seq_idx;
    reg [19:0] delay_counter;
    reg        spi_tx_ready_ff;

    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            init_seq_idx <= 3'd0;
            init_sequence[0] <= 8'h06;  // RESET command
            init_sequence[1] <= 8'h40;  // WREG starting at address 0
            init_sequence[2] <= 8'h02;  // Write 3 registers
            init_sequence[3] <= 8'h05;  // CONFIG0 value
            init_sequence[4] <= 8'h10;  // CONFIG1 value
            init_sequence[5] <= 8'h06;  // CONFIG2 value
            delay_counter <= 20'b0;
            spi_tx_ready_ff <= 1'b0;
            current_state <= S_DELAY;
        end
        else begin
            current_state <= next_state;
            spi_tx_ready_ff <= spi_tx_ready;  // Store the previous state of spi_tx_ready
            init_seq_idx <= next_init_seq_idx;
            if (current_state == S_DELAY) delay_counter <= delay_counter + 1'b1;
            else delay_counter <= 20'b0;
        end
    end

    //====================================
    wire spi_tx_ready_posedge;
    assign spi_tx_ready_posedge = (spi_tx_ready == 1'b1) && (spi_tx_ready_ff == 1'b0);
    localparam [19:0] POWERUP_DELAY = 20'hFFFFF, RESET_CMD_DELAY = 20'hFFFF, POST_INIT_DELAY = 20'hFFF;

    always @(*) begin
        next_state = current_state;
        next_init_seq_idx = init_seq_idx;
        case (current_state)
            S_DELAY: begin
                if ((init_seq_idx == 3'b0) & (delay_counter == POWERUP_DELAY)) next_state = S_SEND_INIT_SEQ;
                else if ((init_seq_idx == 3'd1) & (delay_counter == RESET_CMD_DELAY)) next_state = S_SEND_INIT_SEQ;
                else if ((init_seq_idx == 3'd5) & (delay_counter == POST_INIT_DELAY)) next_state = S_SEND_START;
            end
            S_SEND_INIT_SEQ: begin
                if (spi_tx_ready_posedge && (init_seq_idx < 3'd5)) begin
                    next_init_seq_idx = init_seq_idx + 1'b1;
                    if (next_init_seq_idx == 2'b1) next_state = S_DELAY;  //RESET command delay
                end
                else if (spi_tx_ready_posedge && (init_seq_idx == 3'd5)) next_state = S_DELAY;  // Post-initialization delay
            end
            S_SEND_START: if (spi_tx_ready_posedge) next_state = S_SEND_RDATAC;
            S_SEND_RDATAC: if (spi_tx_ready_posedge) next_state = S_SAMPLING;
            default: next_state = S_SAMPLING;
        endcase
    end

    //====================================
    localparam CMD_RDATAC = 8'h10, CMD_START = 8'h08, CMD_STOP = 8'h0A;
    reg  [2:0] drdy_edge_detect;
    wire       drdy_negedge = (drdy_edge_detect[1] == 1'b0) && (drdy_edge_detect[2] == 1'b1);
    assign o_drdy_negedge = drdy_negedge;
    reg        [ 1:0] sample_cnt;
    reg        [ 7:0] sample_data   [0:2];
    reg signed [23:0] sample_signed;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            spi_tx_dv <= 1'b0;
            spi_tx_byte <= 8'h0;
            drdy_edge_detect <= 3'b0;
            sample_cnt <= 2'd0;
        end
        else begin
            spi_tx_dv <= 1'b0;  // Clear the transmit data-valid pulse by default
            drdy_edge_detect <= {drdy_edge_detect[1:0], i_ads1259_drdy};  // synchronous to i_fpga_clk
            case (next_state)
                S_DELAY: begin
                end
                S_SEND_INIT_SEQ: begin
                    if (spi_tx_ready) begin
                        spi_tx_byte <= init_sequence[init_seq_idx];
                        spi_tx_dv   <= 1'b1;
                    end
                end
                S_SEND_START: begin
                    if (spi_tx_ready) begin
                        spi_tx_byte <= CMD_START;
                        spi_tx_dv   <= 1'b1;
                    end
                end
                S_SEND_RDATAC: begin
                    if (spi_tx_ready) begin
                        spi_tx_byte <= CMD_RDATAC;
                        spi_tx_dv   <= 1'b1;
                    end
                end
                S_SAMPLING: begin
                    if (sample_cnt != 0) begin
                        if (spi_tx_ready) begin
                            spi_tx_byte <= 8'h00;  // Dummy, generate 8 clocks
                            spi_tx_dv   <= 1'b1;
                        end
                        if (spi_rx_dv) begin
                            sample_data[sample_cnt-1] <= spi_rx_byte;
                            sample_cnt <= sample_cnt - 1'b1;
                        end
                    end
                    else begin
                        if (drdy_negedge) sample_cnt <= 2'd3;  // Ready to trigger a new read
                        o_sample_signed <= {sample_data[2], sample_data[1], sample_data[0]};  // If we have read 3 bytes, convert to signed value
                    end
                end
                default: next_state <= S_SAMPLING;
            endcase
        end
    end
endmodule



/*******************************************************************************************************************************/
module OVLD_FAST_RECOVERY (
    input  wire          i_fpga_clk,
    input  wire          i_rst_n,
    input  signed [23:0] i_adc_data,
    input  wire          i_adc_data_valid,
    output reg           o_fast_recovery
);
    assign adc_overload = (i_adc_data >= 24'sd800_0000) || (i_adc_data <= -24'sd800_0000);
    localparam DETECT = 2'd0, RECOVERY = 2'd1, WAIT = 2'd2;
    reg [ 1:0] state;
    reg [ 7:0] window60_cnt;  //60周期窗口计数
    reg [ 7:0] overload_cnt;  //窗口内过载计数
    reg [15:0] recovery_cnt;  //64800周期恢复计数
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            state           <= DETECT;
            window60_cnt    <= 6'd0;
            overload_cnt    <= 6'd0;
            recovery_cnt    <= 16'd0;
            o_fast_recovery <= 1'b1;
        end
        else if (i_adc_data_valid) begin
            case (state)
                //====================================
                DETECT: begin
                    if (window60_cnt < 6'd60) begin  // 累计60周期窗口和过载次数
                        window60_cnt <= window60_cnt + 6'd1;
                        if (adc_overload) overload_cnt <= overload_cnt + 6'd1;
                    end
                    if (window60_cnt == 6'd60) begin  // 达到60周期，判断是否触发恢复
                        if (overload_cnt >= 6'd55) begin
                            state        <= RECOVERY;
                            recovery_cnt <= 16'd64800;
                        end
                        window60_cnt <= 6'd0;
                        overload_cnt <= 6'd0;
                    end
                    o_fast_recovery <= 1'b1;
                end
                //====================================
                RECOVERY: begin
                    if (recovery_cnt > 16'd0) begin  // 使能恢复，输出64800周期（18秒）低电平
                        recovery_cnt    <= recovery_cnt - 16'd1;
                        o_fast_recovery <= 1'b0;
                    end
                    else begin
                        state           <= DETECT;
                        o_fast_recovery <= 1'b1;
                    end
                end
                default: state <= DETECT;
            endcase
        end
    end
endmodule



/*******************************************************************************************************************************/
module DECIMATOR #(
    parameter DECIM_FACTOR = 18,
    parameter SHIFT        = 12
) (
    input                          i_rst_n,
    input                          i_fpga_clk,
    input                          i_valid,     // 3.6kHz
    input  signed     [      23:0] i_data,
    output reg                     o_valid,     // 200Hz
    output reg signed [23-SHIFT:0] o_data
);
    localparam CNT_WIDTH = $clog2(DECIM_FACTOR);
    reg [CNT_WIDTH-1:0] cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            cnt     <= {CNT_WIDTH{1'b0}};
            o_valid <= 1'b0;
            o_data  <= {(23 - SHIFT + 1) {1'b0}};
        end
        else if (i_valid) begin
            o_valid <= 1'b0;
            if (cnt == DECIM_FACTOR - 1) begin
                cnt     <= {CNT_WIDTH{1'b0}};
                o_valid <= 1'b1;
                o_data  <= i_data >>> SHIFT;
            end
            else cnt <= cnt + 1'b1;
        end
        else o_valid <= 1'b0;
    end
endmodule

