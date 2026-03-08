`timescale 1ns / 1ps

module tb_ads1259;

  // Clock & Reset
  reg clk;
  reg rst_n;

  // SPI interface wires
  wire spi_clk;
  wire spi_mosi;
  reg  spi_miso;
  reg  drdy;

  // Instantiate DUT
  ADS1259 dut (
    .i_rst_n(rst_n),
    .i_fpga_clk(clk),
    .o_ads1259_clk(spi_clk),
    .o_ads1259_mosi(spi_mosi),
    .i_ads1259_miso(spi_miso),
    .i_ads1259_drdy(drdy)
  );

  // Clock generation: 72 MHz → 13.89 ns period
  always #7 clk = ~clk;

  initial begin
    // Initial state
    clk     = 0;
    rst_n   = 0;
    spi_miso = 1'b0;
    drdy     = 1'b1;

    // Hold reset for a while
    #100;
    rst_n = 1;

    // Let simulation run
    #1000000;

    $display("Simulation finished.");
    $stop;
  end

endmodule
