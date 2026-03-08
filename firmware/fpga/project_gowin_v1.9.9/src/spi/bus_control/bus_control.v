/*******************************************************************************************/
module Bus_Control (
    input i_fpga_clk,
    input i_rst_n,

    output o_spi_miso,
    input  i_spi_mosi,
    input  i_spi_sclk,
    input  i_spi_cs,

    output [4:0] reg0_high,
    input  [2:0] reg0_low,
    input  [7:0] reg1,
    input  [7:0] reg2,

    output reg           ecg_fifo_read_en,
    input  signed [23:0] ecg_fifo_out,
    input                ecg_fifo_empty,
    input                ecg_fifo_full,

    output reg        rr_fifo_read_en,
    input      [15:0] rr_fifo_out,
    input             rr_fifo_empty,
    input             rr_fifo_full,

    output reg sw_reset
);
    wire [7:0] data_recv;
    wire rx_valid;
    reg [7:0] data_send;
    reg tx_valid;
    SPI_Slave #(
        .DATA_WIDTH(8),
        .SPI_MODE  (0)
    ) spi_slave (
        .i_Rst_L  (i_rst_n),
        .i_Clk_Sys(i_fpga_clk),

        .o_RX_DV  (rx_valid),
        .o_RX_Byte(data_recv),
        .i_TX_DV  (tx_valid),
        .i_TX_Byte(data_send),

        .o_SPI_MISO(o_spi_miso),
        .i_SPI_MOSI(i_spi_mosi),
        .i_SPI_Clk (i_spi_sclk),
        .i_SPI_CS_n(i_spi_cs)
    );

    reg [7:0] crc_data_in, crc_in;
    wire [7:0] crc_out  /* synthesis syn_keep=1 */;
    CRC crc (
        .i_crc (crc_in),
        .i_data(crc_data_in),
        .o_crc (crc_out)
    );

    reg [7:0] reg_map[0:2];
    assign reg0_high = reg_map[0][7:3];

    integer i;
    reg [7:0] fsm_state  /* synthesis syn_encoding = "onehot" */;
    localparam [7:0] S_IDLE = 8'h00, S_WAIT_CMD = 8'h01, S_WAIT_PARAM = 8'h02, S_SEND_CMD_CRC = 8'h04, S_WAIT_CMD_CRC = 8'h08;
    localparam [7:0] S_PAYLOAD = 8'h10, S_SEND_PAYLOAD_CRC = 8'h20, S_WAIT_RRSQ_SEND_BYTE = 8'h40, S_WAIT_ECGS_SEND_BYTE = 8'h80;
    reg [7:0] cmd, param;
    reg fifo_read_req;
    localparam CMD_WREG = 8'd1, CMD_RREG = 8'd2, CMD_RRSQ = 8'd3, CMD_ECGS = 8'd4, CMD_RSET = 8'd5;
    localparam [7:0] FRAME_FLAG = 8'h7E;
    reg crc_send_done;
    reg [5:0] payload_cnt;
    reg [1:0] payload_byte_cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            ecg_fifo_read_en <= 1'd0;
            rr_fifo_read_en <= 1'd0;
            sw_reset <= 1'd1;
            fsm_state <= S_IDLE;
            cmd <= 8'd0;
            param <= 8'd0;
            payload_cnt <= 6'd0;
            payload_byte_cnt <= 2'd2;
            crc_data_in <= 8'd0;
            crc_in <= 8'd0;
            data_send <= 8'd0;
            tx_valid <= 1'd0;
            crc_send_done <= 1'd0;
            fifo_read_req <= 1'b0;
            reg_map[0] <= {5'h8, 3'd0};
            for (i = 1; i < 3; i = i + 1) reg_map[i] <= 8'd0;
        end
        else begin
            tx_valid <= 1'd0;
            ecg_fifo_read_en <= 1'd0;
            rr_fifo_read_en <= 1'd0;
            sw_reset <= 1'd1;
            reg_map[0] <= {reg_map[0][7:3], reg0_low};
            reg_map[1] <= reg1;
            reg_map[2] <= reg2;
            if (rx_valid && (data_recv == FRAME_FLAG)) begin
                fsm_state <= S_WAIT_CMD;
                crc_in <= 8'd0;
                crc_data_in <= 8'd0;
                fifo_read_req <= 1'b0;
            end
            else begin
                case (fsm_state)
                    S_IDLE:  fsm_state <= S_IDLE;
                    S_WAIT_CMD: begin
                        if (rx_valid) begin  //收到命令
                            cmd <= data_recv;
                            crc_in <= 8'd0;
                            crc_data_in <= data_recv;
                            fsm_state <= S_WAIT_PARAM;
                        end
                    end
                    S_WAIT_PARAM: begin
                        if (rx_valid) begin  //收到参数
                            param <= data_recv;
                            if ((cmd == CMD_RRSQ) || (cmd == CMD_ECGS)) payload_cnt <= data_recv[5:0];
                            if (cmd == CMD_RRSQ) payload_byte_cnt <= 2'd1;
                            else payload_byte_cnt <= 2'd2;
                            crc_in <= crc_out;
                            crc_data_in <= data_recv;
                            fsm_state <= S_SEND_CMD_CRC;
                        end
                        if (cmd == CMD_RSET) sw_reset <= 1'd0;
                    end
                    S_SEND_CMD_CRC: begin
                        data_send <= crc_out;  //发送命令阶段CRC值
                        tx_valid  <= 1'd1;
                        fsm_state <= S_WAIT_CMD_CRC;
                    end
                    S_WAIT_CMD_CRC:
                    if (rx_valid) begin
                        fsm_state <= S_PAYLOAD;
                        crc_send_done <= 1'd1;  //命令阶段CRC值发送完毕，接下来根据命令处理payload
                    end
                    S_PAYLOAD: begin
                        case (cmd)
                            CMD_WREG: begin
                                if (rx_valid) begin  //收到将要写入的payload
                                    if (param == 0) begin  //仅允许写reg0[7:3]
                                        reg_map[0]  <= {data_recv[7:3], reg0_low};
                                        crc_data_in <= data_recv;
                                    end
                                    crc_in <= 8'd0;
                                    fsm_state <= S_SEND_PAYLOAD_CRC;
                                end
                            end
                            CMD_RREG: begin
                                if (crc_send_done) begin  //读出寄存器的值并发送
                                    if (param < 3) begin
                                        data_send <= reg_map[param];
                                        tx_valid  <= 1'b1;
                                    end
                                    else begin
                                        data_send <= 8'hFF;
                                        tx_valid  <= 1'b1;
                                    end
                                    crc_send_done <= 1'd0;
                                end
                                if (rx_valid) begin  //寄存器值已发送，接着发送寄存器值的CRC
                                    crc_in <= 8'd0;
                                    crc_data_in <= data_send;
                                    fsm_state <= S_SEND_PAYLOAD_CRC;
                                end
                            end
                            CMD_RRSQ: begin
                                if (fifo_read_req) begin  // 第2拍：标志位为1，说明数据已准备好
                                    fsm_state <= S_WAIT_RRSQ_SEND_BYTE;
                                    payload_cnt <= 
                                     - 6'd1;
                                    payload_byte_cnt <= 2'd1;
                                    fifo_read_req <= 1'b0;  // 清除标志位
                                end
                                else if (payload_cnt && (~rr_fifo_empty)) begin  // 第1拍：发出读请求
                                    rr_fifo_read_en <= 1'd1;  // 使能FIFO读
                                    fifo_read_req   <= 1'd1;  // 设置标志位
                                    // fsm_state 保持不变，等待下一拍
                                end
                                else begin
                                    fsm_state <= S_IDLE;  // 传输完成或FIFO为空，返回IDLE
                                end
                            end
                            CMD_ECGS: begin
                                if (fifo_read_req) begin  // 第2拍：标志位为1，说明数据已准备好
                                    fsm_state <= S_WAIT_ECGS_SEND_BYTE;
                                    payload_cnt <= payload_cnt - 6'd1;
                                    payload_byte_cnt <= 2'd2;
                                    fifo_read_req <= 1'b0;  // 清除标志位
                                end
                                else if (payload_cnt && (~ecg_fifo_empty)) begin  // 第1拍：发出读请求
                                    ecg_fifo_read_en <= 1'd1;  // 使能FIFO读
                                    fifo_read_req <= 1'd1;  // 设置标志位
                                    // fsm_state 保持不变，等待下一拍
                                end
                                else begin
                                    fsm_state <= S_IDLE;  // 传输完成或FIFO为空，返回IDLE
                                end
                            end
                            default: fsm_state <= S_IDLE;
                        endcase
                    end
                    S_SEND_PAYLOAD_CRC: begin  //发送payload阶段CRC值
                        data_send <= crc_out;
                        tx_valid  <= 1'd1;
                        fsm_state <= S_IDLE;
                    end
                    S_WAIT_RRSQ_SEND_BYTE: begin
                        if (crc_send_done) crc_send_done <= 1'd0;
                        if (rx_valid || crc_send_done) begin
                            case (payload_byte_cnt)
                                2'd1:    data_send <= rr_fifo_out[15:8];
                                2'd0:    data_send <= rr_fifo_out[7:0];
                                default: data_send <= data_send;
                            endcase
                            tx_valid <= 1'b1;
                            if (payload_byte_cnt > 2'd0) payload_byte_cnt <= payload_byte_cnt - 2'd1;
                            else fsm_state <= S_PAYLOAD;
                        end
                    end
                    S_WAIT_ECGS_SEND_BYTE: begin
                        if (crc_send_done) crc_send_done <= 1'd0;
                        if (rx_valid || crc_send_done) begin
                            case (payload_byte_cnt)
                                2'd2:    data_send <= ecg_fifo_out[23:16];
                                2'd1:    data_send <= ecg_fifo_out[15:8];
                                2'd0:    data_send <= ecg_fifo_out[7:0];
                                default: data_send <= data_send;
                            endcase
                            tx_valid <= 1'b1;
                            if (payload_byte_cnt > 2'd0) payload_byte_cnt <= payload_byte_cnt - 2'd1;
                            else fsm_state <= S_PAYLOAD;
                        end
                    end
                    default: fsm_state <= S_IDLE;
                endcase
            end
        end
    end

endmodule

