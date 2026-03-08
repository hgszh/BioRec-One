module DAC8830 (
    input         i_rst_n,
    input         i_fpga_clk,
    input  [15:0] i_dac_data,
    input         i_dac_valid,
    output        o_dac_sclk,
    output        o_dac_mosi,
    output        o_dac_cs
);

    wire       spi_tx_ready;
    reg        spi_tx_dv;
    reg  [7:0] spi_tx_byte;
    reg  [1:0] sample_cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            sample_cnt  <= 2'd0;
            spi_tx_dv   <= 1'b0;
            spi_tx_byte <= 8'd0;
        end
        else begin
            spi_tx_dv <= 1'b0;
            if ((sample_cnt != 2'd0) && (spi_tx_ready)) begin
                case (sample_cnt)
                    2'd2: spi_tx_byte <= i_dac_data[15:8];
                    2'd1: spi_tx_byte <= i_dac_data[7:0];
                    default: spi_tx_byte <= 8'd0;
                endcase
                spi_tx_dv  <= 1'b1;
                sample_cnt <= sample_cnt - 2'd1;
            end
            else if (i_dac_valid) sample_cnt <= 2'd2;
        end
    end

    SPI_Master_With_Single_CS #(
        .SPI_MODE         (0),
        .CLKS_PER_HALF_BIT(8),
        .MAX_BYTES_PER_CS (2),
        .CS_INACTIVE_CLKS (1)
    ) dac_spi (

        .i_Rst_L(i_rst_n),
        .i_Clk  (i_fpga_clk),

        .i_TX_Count(2'd2),
        .i_TX_Byte (spi_tx_byte),
        .i_TX_DV   (spi_tx_dv),
        .o_TX_Ready(spi_tx_ready),

        .o_RX_Count(),
        .o_RX_DV   (),
        .o_RX_Byte (),

        .o_SPI_Clk (o_dac_sclk),
        .i_SPI_MISO(1'b0),
        .o_SPI_MOSI(o_dac_mosi),
        .o_SPI_CS_n(o_dac_cs)
    );

endmodule


