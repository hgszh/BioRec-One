/*******************************************************************************************/
// 修改自https://github.com/nandland/spi-slave/tree/master
module SPI_Slave #(
    parameter DATA_WIDTH = 16,
    parameter SPI_MODE   = 0
) (
    input                       i_Rst_L,
    input                       i_Clk_Sys,
    output reg                  o_RX_DV,
    output reg [DATA_WIDTH-1:0] o_RX_Byte,
    input                       i_TX_DV,
    input      [DATA_WIDTH-1:0] i_TX_Byte,

    input  i_SPI_Clk,
    output o_SPI_MISO,
    input  i_SPI_MOSI,
    input  i_SPI_CS_n
);
    wire w_CPOL = (SPI_MODE == 2) | (SPI_MODE == 3);
    wire w_CPHA = (SPI_MODE == 1) | (SPI_MODE == 3);
    wire w_SPI_Clk = w_CPHA ? ~i_SPI_Clk : i_SPI_Clk;

    reg [2:0] CS_Cnt;
    always @(posedge i_Clk_Sys or negedge i_Rst_L) begin  //对输入信号去毛刺
        if (~i_Rst_L) CS_Cnt <= 3'd0;
        else begin
            if (~i_SPI_CS_n) CS_Cnt <= 3'd0;
            else if (CS_Cnt < 3'd7) CS_Cnt <= CS_Cnt + 3'd1;
        end
    end
    wire CS_Det = (CS_Cnt == 3'd7);

    localparam COUNTER_WIDTH = $clog2(DATA_WIDTH);
    reg [DATA_WIDTH-1:0] r_RX_Byte, r_Temp_RX_Byte;
    reg [COUNTER_WIDTH-1:0] r_RX_Bit_Count;
    localparam [COUNTER_WIDTH-1:0] CLEAR_POS = 2;
    reg r_RX_Done, r2_RX_Done, r3_RX_Done;
    always @(posedge w_SPI_Clk or posedge CS_Det) begin  //读取MOSI上数据
        if (CS_Det) begin
            r_RX_Bit_Count <= 1'b0;
            r_RX_Done      <= 1'b0;
            r_RX_Byte      <= {DATA_WIDTH{1'b0}};
            r_Temp_RX_Byte <= {DATA_WIDTH{1'b0}};
        end
        else begin
            r_RX_Bit_Count <= r_RX_Bit_Count + 1'b1;
            r_Temp_RX_Byte <= {r_Temp_RX_Byte[(DATA_WIDTH-1)-1:0], i_SPI_MOSI};  // 对数据左移，同时，更新LSB
            if (r_RX_Bit_Count == DATA_WIDTH - 1) begin
                r_RX_Done <= 1'b1;
                r_RX_Byte <= {r_Temp_RX_Byte[(DATA_WIDTH-1)-1:0], i_SPI_MOSI};
            end
            else if (r_RX_Bit_Count == CLEAR_POS) r_RX_Done <= 1'b0;
        end
    end
    always @(posedge i_Clk_Sys or negedge i_Rst_L) begin  // 将外部时钟的数据同步到系统时钟
        if (~i_Rst_L) begin
            r2_RX_Done <= 1'b0;
            r3_RX_Done <= 1'b0;
            o_RX_DV    <= 1'b0;
            o_RX_Byte  <=  {DATA_WIDTH{1'b0}};
        end
        else begin
            r2_RX_Done <= r_RX_Done;
            r3_RX_Done <= r2_RX_Done;
            if (r3_RX_Done == 1'b0 && r2_RX_Done == 1'b1) begin  // 上升沿
                o_RX_DV   <= 1'b1;
                o_RX_Byte <= r_RX_Byte;
            end
            else o_RX_DV <= 1'b0;
        end
    end

    reg [DATA_WIDTH-1:0] r_TX_Byte;
    reg [COUNTER_WIDTH-1:0] r_TX_Bit_Count;
    reg r_SPI_MISO_Bit, r_Preload_MISO;
    always @(posedge i_Clk_Sys or negedge i_Rst_L) begin  // 当数据有效脉冲出现，寄存待发送的数据
        if (~i_Rst_L) r_TX_Byte <= {DATA_WIDTH{1'b0}};
        else if (i_TX_DV) r_TX_Byte <= i_TX_Byte;
    end
    always @(negedge w_SPI_Clk or posedge CS_Det) begin  // 发送数据到MISO
        if (CS_Det) begin
            r_Preload_MISO <= 1'b1;
            r_TX_Bit_Count <= (DATA_WIDTH - 1) - 1;  //计数变量需减去预加载的MSB
        end
        else begin
            r_Preload_MISO <= 1'b0;
            r_SPI_MISO_Bit <= r_TX_Byte[r_TX_Bit_Count];
            r_TX_Bit_Count <= r_TX_Bit_Count - 1'b1;
        end
    end

    wire w_SPI_MISO_Mux = r_Preload_MISO ? r_TX_Byte[DATA_WIDTH-1] : r_SPI_MISO_Bit;  // 当预加载选择器为高电平时，将MISO预加载为发送数据的MSB，否则只发送正常的MISO数据
    assign o_SPI_MISO = CS_Det ? 1'bZ : w_SPI_MISO_Mux;  // 当CS为高电平时，MISO处于高阻态。允许多个从设备进行通信
endmodule

/*******************************************************************************************/
// https://bues.ch/h/crcgen
// CRC polynomial coefficients: x^8 + x^2 + x + 1
//                              0x7 (hex)
// CRC width:                   8 bits
// CRC shift direction:         left (big endian)
// Input word width:            8 bits
module CRC (
    input  [7:0] i_crc,
    input  [7:0] i_data,
    output [7:0] o_crc
);
    assign o_crc[0] = i_crc[0] ^ i_crc[6] ^ i_crc[7] ^ i_data[0] ^ i_data[6] ^ i_data[7];
    assign o_crc[1] = i_crc[0] ^ i_crc[1] ^ i_crc[6] ^ i_data[0] ^ i_data[1] ^ i_data[6];
    assign o_crc[2] = i_crc[0] ^ i_crc[1] ^ i_crc[2] ^ i_crc[6] ^ i_data[0] ^ i_data[1] ^ i_data[2] ^ i_data[6];
    assign o_crc[3] = i_crc[1] ^ i_crc[2] ^ i_crc[3] ^ i_crc[7] ^ i_data[1] ^ i_data[2] ^ i_data[3] ^ i_data[7];
    assign o_crc[4] = i_crc[2] ^ i_crc[3] ^ i_crc[4] ^ i_data[2] ^ i_data[3] ^ i_data[4];
    assign o_crc[5] = i_crc[3] ^ i_crc[4] ^ i_crc[5] ^ i_data[3] ^ i_data[4] ^ i_data[5];
    assign o_crc[6] = i_crc[4] ^ i_crc[5] ^ i_crc[6] ^ i_data[4] ^ i_data[5] ^ i_data[6];
    assign o_crc[7] = i_crc[5] ^ i_crc[6] ^ i_crc[7] ^ i_data[5] ^ i_data[6] ^ i_data[7];
endmodule
