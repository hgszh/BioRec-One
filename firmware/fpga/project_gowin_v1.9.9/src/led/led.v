module LED_CONTROL (
    input         i_fpga_clk,
    input         i_rst_n,
    input         i_fast_recovery,
    input         i_iir_ring_alarm,
    input         i_ecg_r_pulse,
    input  [11:0] i_rr_interval,
    input  [ 1:0] i_lpf_coeffi_sel,
    output [ 5:0] o_led
);
    reg        iir_ring_alarm_led;
    reg [23:0] iir_ring_alarm_delay_cnt;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            iir_ring_alarm_led <= 0;
            iir_ring_alarm_delay_cnt <= 0;
        end
        else begin
            if (!i_iir_ring_alarm) iir_ring_alarm_delay_cnt <= 24'd16777_000;
            else if (iir_ring_alarm_delay_cnt > 0) iir_ring_alarm_delay_cnt <= iir_ring_alarm_delay_cnt - 1'b1;
            iir_ring_alarm_led <= (iir_ring_alarm_delay_cnt == 0);
        end
    end

    reg        ecg_peak_led;
    reg [24:0] ecg_peak_delay_cnt;
    reg [24:0] rr_interval_mult_27000;
    localparam [24:0] phase_offset = 7_000_000;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            ecg_peak_led <= 0;
            ecg_peak_delay_cnt <= 0;
            rr_interval_mult_27000 <= 0;
        end
        else begin
            rr_interval_mult_27000 <= (i_rr_interval << 14) + (i_rr_interval << 13) - phase_offset;  //为了和R波对齐
            if (i_ecg_r_pulse) begin
                ecg_peak_delay_cnt <= rr_interval_mult_27000;
            end
            else if (ecg_peak_delay_cnt > 0) ecg_peak_delay_cnt <= ecg_peak_delay_cnt - 1'b1;
            ecg_peak_led <= (ecg_peak_delay_cnt == 0);
        end
    end

    wire [2:0] lower_bits = (i_lpf_coeffi_sel == 2) ? 3'b011 : (i_lpf_coeffi_sel == 1) ? 3'b101 : 3'b110;
    assign o_led = {ecg_peak_led, iir_ring_alarm_led, i_fast_recovery, lower_bits};

endmodule
