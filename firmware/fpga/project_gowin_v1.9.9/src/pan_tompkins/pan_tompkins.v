`include "pan_tompkins_utils.v"

module PAN_TOMPKINS (
    input                i_fpga_clk,
    input                i_rst_n,
    input  signed [11:0] i_data,           // Sampled ECG data
    input                i_valid,          // 200Hz sampling clock
    output               o_r_peak_pulse,
    output        [11:0] o_rr_interval_ms
);
    // 5ms tick counter
    wire [31:0] ms_tick;
    MS5_TIMER timer_inst (
        .i_fpga_clk(i_fpga_clk),
        .i_rst_n   (i_rst_n),
        .i_valid   (i_valid),
        .o_ms_tick (ms_tick)
    );
    // Refractory gate
    wire ecg_valid_masked;
    wire r_peak_detected;
    REFR_GATE refractory_gate (
        .i_fpga_clk          (i_fpga_clk),
        .i_rst_n             (i_rst_n),
        .time_ms             (ms_tick),
        .i_valid             (i_valid),
        .i_refractory_trigger(r_peak_detected),
        .o_valid_masked      (ecg_valid_masked)
    );
    // Preprocessing
    wire [11:0] ecg_filtered;
    PRE_PROCESSOR ecg_preprocessor (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (i_data),
        .i_valid   (i_valid),
        .o_data    (ecg_filtered)
    );
    // Peak detection
    wire peak_detected;
    wire [11:0] peak_data;
    wire [31:0] peak_ms;
    FIND_PEAK findpeak (
        .i_fpga_clk (i_fpga_clk),
        .i_rst_n    (i_rst_n),
        .i_data     (ecg_filtered),
        .i_valid    (ecg_valid_masked),
        .time_ms    (ms_tick),
        .o_valid    (peak_detected),
        .o_peak_data(peak_data),
        .o_peak_ms  (peak_ms)
    );
    // R peak, noise peak, and RR interval estimators
    reg [11:0] r_peak_estimator_i_data;
    reg r_peak_estimator_i_valid;
    wire [11:0] r_peak_estimator_o_data;
    wire r_peak_estimator_o_valid;
    MEDIAN_9 r_peak_estimator (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (r_peak_estimator_i_data),
        .i_valid   (r_peak_estimator_i_valid),
        .o_data    (r_peak_estimator_o_data),
        .o_valid   (r_peak_estimator_o_valid)
    );
    reg [11:0] ns_peak_estimator_i_data;
    reg ns_peak_estimator_i_valid;
    wire [11:0] ns_peak_estimator_o_data;
    wire ns_peak_estimator_o_valid;
    MEDIAN_9 ns_peak_estimator (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (ns_peak_estimator_i_data),
        .i_valid   (ns_peak_estimator_i_valid),
        .o_data    (ns_peak_estimator_o_data),
        .o_valid   (ns_peak_estimator_o_valid)
    );
    reg [11:0] rr_interval_estimator_i_data;
    reg rr_interval_estimator_i_valid;
    wire [11:0] rr_interval_estimator_o_data;
    wire rr_interval_estimator_o_valid;
    MEDIAN_9 rr_interval_estimator (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (rr_interval_estimator_i_data),
        .i_valid   (rr_interval_estimator_i_valid),
        .o_data    (rr_interval_estimator_o_data),
        .o_valid   (rr_interval_estimator_o_valid)
    );
    // Update dynamic threshold: TH = NPK + 0.1875 * (SPK - NPK)
    wire peak_estimator_valid = ns_peak_estimator_o_valid | r_peak_estimator_o_valid;
    wire [11:0] r_ns_peak_diff = (r_peak_estimator_o_data > ns_peak_estimator_o_data) ? (r_peak_estimator_o_data - ns_peak_estimator_o_data) : 12'd0;
    wire [11:0] threshold_offset = (r_ns_peak_diff >> 3) + (r_ns_peak_diff >> 4);
    reg [11:0] detection_threshold;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) detection_threshold <= 12'd0;
        else if (peak_estimator_valid) detection_threshold <= ns_peak_estimator_o_data + threshold_offset;
    end
    // R-peak classification , update noise peak tracking since last confirmed R-peak
    reg [31:0] time_of_max_noise;
    reg [11:0] noise_peak_max;
    assign r_peak_detected = (!i_rst_n) ? (1'b0) : (peak_detected & (peak_data >= detection_threshold));
    wire ns_peak_detected = (!i_rst_n) ? (1'b0) : (peak_detected & (peak_data < detection_threshold));
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            r_peak_estimator_i_valid  <= 1'b0;
            ns_peak_estimator_i_valid <= 1'b0;
            r_peak_estimator_i_data   <= 12'd0;
            ns_peak_estimator_i_data  <= 12'd0;
            time_of_max_noise         <= 32'b0;
            noise_peak_max            <= 12'b0;
        end
        else begin
            r_peak_estimator_i_valid  <= 1'd0;
            ns_peak_estimator_i_valid <= 1'd0;
            if (r_peak_detected) begin
                r_peak_estimator_i_data <= peak_data;
                r_peak_estimator_i_valid <= 1'd1;
                time_of_max_noise <= 32'd0;  // Reset noise peak tracking after confirmed R-peak
                noise_peak_max <= 12'd0;
            end
            else if (ns_peak_detected) begin
                ns_peak_estimator_i_data  <= peak_data;
                ns_peak_estimator_i_valid <= 1'b1;
                if (peak_data > noise_peak_max) begin
                    noise_peak_max <= peak_data;
                    time_of_max_noise <= peak_ms;
                end
            end
        end
    end
    // Update RR estimator input and apply searchback recovery using the max noise peak
    reg [31:0] r_peak_ms_prev;
    reg [11:0] rr_searchback_threshold;
    reg [11:0] rr_interval_ms;
    always @(posedge i_fpga_clk or negedge i_rst_n) begin
        if (!i_rst_n) begin
            rr_interval_estimator_i_valid <= 1'b0;
            rr_interval_estimator_i_data  <= 12'd0;
            rr_interval_ms                <= 12'd0;
            r_peak_ms_prev                <= 32'd0;
            rr_searchback_threshold       <= 12'd4095;
        end
        else begin
            rr_interval_estimator_i_valid <= 1'b0;
            // rr_searchback_threshold = 1.5 * estimated RR interval
            if (rr_interval_estimator_o_valid) rr_searchback_threshold <= rr_interval_estimator_o_data + (rr_interval_estimator_o_data >> 1);
            if (r_peak_detected) begin
                rr_interval_ms <= ((peak_ms - r_peak_ms_prev) & 12'hFFF);
                if (rr_interval_ms < rr_searchback_threshold) begin
                    rr_interval_estimator_i_data  <= rr_interval_ms;
                    rr_interval_estimator_i_valid <= 1'b1;
                end
                // Attempts to recover missed R-peaks when abnormal RR intervals are detected
                else if (noise_peak_max > (detection_threshold >> 1)) begin
                    rr_interval_estimator_i_data  <= (time_of_max_noise - r_peak_ms_prev) & 12'hFFF;
                    rr_interval_estimator_i_valid <= 1'b1;
                end
                r_peak_ms_prev <= peak_ms;
            end
        end
    end

    // Output R-peak pulse and RR interval (final output from RR estimator)
    assign o_r_peak_pulse   = rr_interval_estimator_o_valid;
    assign o_rr_interval_ms = rr_interval_estimator_o_data[11:0];

endmodule
