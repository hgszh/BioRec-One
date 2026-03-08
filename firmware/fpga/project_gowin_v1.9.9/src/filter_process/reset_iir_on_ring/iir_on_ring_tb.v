`timescale 1ns / 1ps

module tb_reset_iir_on_ring;

    parameter integer clk_freq_hz = 18_000_000;  // 18 MHz
    parameter integer sample_freq_hz = 3_600;  // 3.6 kHz
    parameter integer sample_period = clk_freq_hz / sample_freq_hz;  // 7500 clk
    parameter integer sim_seconds = 1.5;  // 仿真总时长 
    parameter integer num_samples = sample_freq_hz * sim_seconds;

    reg  clk = 0;
    reg  rst_n = 0;
    real half_period_ns = 1_000_000_000.0 / (2 * clk_freq_hz);
    // clk 
    initial begin
        forever #half_period_ns clk = ~clk;
    end
    // reset 
    initial begin
        rst_n = 0;
        #100;
        rst_n = 1;
    end

    reg signed  [23:0] i_data = 0;
    reg                i_valid = 0;
    wire               alarm;
    wire signed [23:0] hpf_out;
    wire        [23:0] peak_cnt;
    wire        [24:0] sec_cnt;
    RESET_IIR_ON_RING dut (
        .i_rst_n   (rst_n),
        .i_fpga_clk(clk),
        .i_data    (i_data),
        .i_valid   (i_valid),
        .alarm     (alarm),
        .peak_cnt  (peak_cnt),
        .hpf_out   (hpf_out),
        .sec_cnt   (sec_cnt)
    );

    integer sample_idx, i;
    real phase = 0.0;
    real phase_step = 2.0 * 3.1415926535 * 20.0 / sample_freq_hz;
    real sine_val;
    initial begin
        // 等待 reset 释放
        @(posedge rst_n);
        // 每个采样点循环
        for (sample_idx = 0; sample_idx < num_samples; sample_idx = sample_idx + 1) begin
            // 更新正弦相位
            phase = phase + phase_step;
            if (phase >= 2.0 * 3.1415926535) phase = phase - 2.0 * 3.1415926535;
            sine_val = 0.8 * $sin(phase);
            // 前半段插尖刺：每隔10个采样点一次
            if (sample_idx < num_samples / 2 && (sample_idx % 10) == 0) begin
                i_data = 24'sd6000000;
            end
            else begin
                i_data = $rtoi(sine_val * (1 << 23));
            end
            // 产生 i_valid 一个时钟周期
            i_valid = 1;
            @(posedge clk);
            i_valid = 0;
            // 等待剩余采样周期
            for (i = 1; i < sample_period; i = i + 1) @(posedge clk);
        end
        // 给足够时间观察 alarm 复位
        #100_000;
    end

endmodule
