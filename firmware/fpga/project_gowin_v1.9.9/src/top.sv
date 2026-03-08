module top (
    input rst_key,
    input clk_xtal_27,

    output ads1259_clk,
    output ads1259_din,
    input  ads1259_dout,
    input  ads1259_drdy,

    output dac8830_sclk,
    output dac8830_mosi,
    output dac8830_cs,

    output spi_miso,
    input  spi_mosi,
    input  spi_sclk,
    input  spi_cs,

    output       afe_hpf_sel_o,
    output [5:0] led,
    output       debug_ecg_r_pulse
);
    wire               sw_reset;
    wire               reset_n = rst_key && sw_reset;
    wire               adc_data_valid;
    wire signed [23:0] adc_data;
    ADS1259 ads1259 (
        .i_rst_n        (reset_n),
        .i_fpga_clk     (clk_xtal_27),
        .o_ads1259_clk  (ads1259_clk),
        .o_ads1259_mosi (ads1259_din),
        .i_ads1259_miso (ads1259_dout),
        .i_ads1259_drdy (ads1259_drdy),
        .o_drdy_negedge (adc_data_valid),
        .o_sample_signed(adc_data)
    );
    OVLD_FAST_RECOVERY afe_fast_recovery (
        .i_fpga_clk      (clk_xtal_27),
        .i_rst_n         (reset_n),
        .i_adc_data      (adc_data),
        .i_adc_data_valid(adc_data_valid),
        .o_fast_recovery (fast_recovery)
    );
    wire afe_hpf_sel;
    assign afe_hpf_sel_o = fast_recovery & afe_hpf_sel;
    /*****************************************************************/
    wire               filter_data_valid;
    wire signed [23:0] filter_data;
    wire        [ 1:0] lpf_coeffi_sel;
    wire               dsp_hpf_bypass;
    wire               notch_bypass;
    wire               iir_ring_alarm;
    FILTER_PROCESS filter_process (
        .i_rst_n   (reset_n),
        .i_fpga_clk(clk_xtal_27),

        .i_data      (adc_data),
        .i_data_valid(adc_data_valid),

        .i_lpf_coeffi_sel(lpf_coeffi_sel),
        .i_hpf_bypass    (dsp_hpf_bypass),
        .i_notch_bypass  (notch_bypass),

        .o_data          (filter_data),
        .o_data_valid    (filter_data_valid),
        .o_iir_ring_alarm(iir_ring_alarm)
    );
    /*****************************************************************/
    wire [15:0] dac_data;
    assign dac_data = {!filter_data[23], filter_data[22:8]};
    DAC8830 dac8830 (
        .i_rst_n    (reset_n),
        .i_fpga_clk (clk_xtal_27),
        .i_dac_data (dac_data),
        .i_dac_valid(filter_data_valid),
        .o_dac_sclk (dac8830_sclk),
        .o_dac_mosi (dac8830_mosi),
        .o_dac_cs   (dac8830_cs)
    );
    /*****************************************************************/
    wire pan_tompkins_data_valid;
    wire signed [11:0] pan_tompkins_data_in;
    DECIMATOR decimator_200hz (
        .i_rst_n   (reset_n),
        .i_fpga_clk(clk_xtal_27),
        .i_valid   (filter_data_valid),
        .i_data    (filter_data),
        .o_valid   (pan_tompkins_data_valid),
        .o_data    (pan_tompkins_data_in)
    );
    wire ecg_r_pulse;
    wire [11:0] ecg_rr_interval;
    reg pt_watchdog_rst;  //Reset if R wave is not detected in 10s
    PAN_TOMPKINS pan_tompkins (
        .i_fpga_clk      (clk_xtal_27),
        .i_rst_n         (reset_n && pt_watchdog_rst),
        .i_data          (pan_tompkins_data_in),
        .i_valid         (pan_tompkins_data_valid),
        .o_r_peak_pulse  (ecg_r_pulse),
        .o_rr_interval_ms(ecg_rr_interval)
    );
    reg [10:0] pt_watchdog_cnt;
    reg fast_recovery_reg;
    wire fast_recovery_negedge = (fast_recovery_reg) && (!fast_recovery);
    always @(posedge clk_xtal_27 or negedge reset_n) begin
        if (!reset_n) begin
            pt_watchdog_cnt   <= 11'd0;
            pt_watchdog_rst   <= 1'd1;
            fast_recovery_reg <= 1'd0;
        end
        else begin
            fast_recovery_reg <= fast_recovery;
            pt_watchdog_rst   <= 1'd1;
            if (ecg_r_pulse) pt_watchdog_cnt <= 11'd0;
            if (pan_tompkins_data_valid) pt_watchdog_cnt <= pt_watchdog_cnt + 11'd1;
            if ((pt_watchdog_cnt > 11'd2000) || (fast_recovery_negedge) || (!iir_ring_alarm)) begin
                pt_watchdog_rst <= 1'd0;
                pt_watchdog_cnt <= 11'd0;
            end
        end
    end
    /*****************************************************************/
    LED_CONTROL led_control (
        .i_rst_n         (reset_n),
        .i_fpga_clk      (clk_xtal_27),
        .i_fast_recovery (fast_recovery),
        .i_iir_ring_alarm(iir_ring_alarm),
        .i_ecg_r_pulse   (ecg_r_pulse),
        .i_rr_interval   (ecg_rr_interval),
        .i_lpf_coeffi_sel(lpf_coeffi_sel),
        .o_led           (led)
    );
    assign debug_ecg_r_pulse = led[5];
    /*****************************************************************/
    wire ecg_valid_600hz;
    wire signed [23:0] ecg_data_600hz;
    DECIMATOR #(
        .DECIM_FACTOR(6),
        .SHIFT(0)
    ) decimator_600hz (
        .i_rst_n   (reset_n),
        .i_fpga_clk(clk_xtal_27),
        .i_valid   (filter_data_valid),
        .i_data    (filter_data),
        .o_valid   (ecg_valid_600hz),
        .o_data    (ecg_data_600hz)
    );
    wire ecg_fifo_read_en;
    wire [6:0] ecg_fifo_cnt;
    wire signed [23:0] ecg_fifo_out;
    wire ecg_fifo_empty, ecg_fifo_full;
    fifo_24 ecg_sample_fifo (
        .Reset(!reset_n),
        .Clk  (clk_xtal_27),
        .Data (ecg_data_600hz),
        .WrEn (ecg_valid_600hz),
        .Q    (ecg_fifo_out),
        .RdEn (ecg_fifo_read_en),
        .Wnum (ecg_fifo_cnt),
        .Empty(ecg_fifo_empty),
        .Full (ecg_fifo_full)
    );

    wire rr_fifo_read_en;
    wire [3:0] rr_fifo_cnt;
    wire [15:0] rr_fifo_out;
    wire [15:0] rr_fifo_in = {4'd0, ecg_rr_interval};
    wire rr_fifo_empty, rr_fifo_full;
    fifo_16 rr_sequence_fifo (
        .Reset(!reset_n),
        .Clk  (clk_xtal_27),
        .Data (rr_fifo_in),
        .WrEn (ecg_r_pulse),
        .Q    (rr_fifo_out),
        .RdEn (rr_fifo_read_en),
        .Wnum (rr_fifo_cnt),
        .Empty(rr_fifo_empty),
        .Full (rr_fifo_full)
    );
    wire [7:0] reg_map[2:0];
    assign {lpf_coeffi_sel[1], lpf_coeffi_sel[0], notch_bypass, dsp_hpf_bypass, afe_hpf_sel} = reg_map[0][7:3];
    assign reg_map[0][2:0] = {1'b0, ~led[3], ~led[4], ~led[5]};
    assign reg_map[1] = {1'd0, ecg_fifo_cnt};
    assign reg_map[2] = {2'd0, rr_fifo_cnt};
    Bus_Control spi_bus (
        .i_fpga_clk(clk_xtal_27),
        .i_rst_n   (reset_n),

        .o_spi_miso(spi_miso),
        .i_spi_mosi(spi_mosi),
        .i_spi_sclk(spi_sclk),
        .i_spi_cs  (spi_cs),

        .reg0_high(reg_map[0][7:3]),
        .reg0_low (reg_map[0][2:0]),
        .reg1     (reg_map[1]),
        .reg2     (reg_map[2]),

        .ecg_fifo_read_en(ecg_fifo_read_en),
        .ecg_fifo_out    (ecg_fifo_out),
        .ecg_fifo_empty  (ecg_fifo_empty),
        .ecg_fifo_full   (ecg_fifo_full),

        .rr_fifo_read_en(rr_fifo_read_en),
        .rr_fifo_out    (rr_fifo_out),
        .rr_fifo_empty  (rr_fifo_empty),
        .rr_fifo_full   (rr_fifo_full),

        .sw_reset(sw_reset)
    );

endmodule
