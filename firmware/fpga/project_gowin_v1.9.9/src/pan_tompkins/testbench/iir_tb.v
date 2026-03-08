`timescale 1ns / 1ps
module tb_iir_bpf;

    // 参数定义
    parameter integer fs = 200;  // 采样频率Hz
    parameter integer ts_ns = 13.889;  // 采样周期ns 
    parameter integer N = 4096;  // 每个频点采样点数
    parameter real A = 2.0 ** 10;  // 正弦幅度

    // 扫描参数
    parameter integer f_start = 1;
    parameter integer f_stop = 100;  // 奈奎斯特
    parameter integer df = 1;  // 步长Hz

    // 时钟&复位
    reg                i_rst_n;
    reg                i_fpga_clk;
    reg signed  [11:0] data_in;
    wire signed [11:0] data_out;

    // 实例化待测滤波器
    IIR_BPF uut (
        .i_rst_n   (i_rst_n),
        .i_fpga_clk(i_fpga_clk),
        .i_data    (data_in),
        .i_valid   (1'b1),
        .o_data    (data_out)
    );

    // 产生时钟
    initial begin
        i_fpga_clk = 0;
        forever #(ts_ns / 2) i_fpga_clk = ~i_fpga_clk;
    end

    // 主控制：复位→频率扫描→结束
    integer freq, n;
    real phase, sample;
    real    max_out;
    integer fd;
    real    gain_db;
    real    out_val;

    initial begin
        // 打开输出文件
        fd = $fopen("freq_response.csv", "w");
        $fwrite(fd, "freq_hz,gain_db\n");

        // 复位
        i_rst_n = 0;
        data_in = 0;
        #(10 * ts_ns);
        i_rst_n = 1;
        #(10 * ts_ns);

        // 频率扫描
        for (freq = f_start; freq <= f_stop; freq = freq + df) begin
            max_out = 0.0;
            phase   = 0.0;

            // 采样 N 点
            for (n = 0; n < N; n = n + 1) begin
                // 正弦信号，2π·f·n/fs
                phase   = 2.0 * 3.141592653589793 * freq * n / fs;
                sample  = A * $sin(phase);
                data_in = $signed($rtoi(sample));  // 确保有符号转换
                @(posedge i_fpga_clk);

                // 添加1个周期延迟以确保输出稳定
                @(negedge i_fpga_clk);

                // 正确计算绝对值
                if (data_out[11]) begin
                    out_val = -$signed(data_out);
                end
                else begin
                    out_val = data_out;
                end

                // 更新峰值
                if (out_val > max_out) max_out = out_val;
            end

            // 计算增益 (dB)：20·log10(max_out/A)
            if (max_out > 0) begin
                gain_db = 20.0 * $log10(max_out / A);
            end
            else begin
                gain_db = -200;  // 极小值表示错误
            end

            $fwrite(fd, "%0d,%.6f\n", freq, gain_db);
            $display("freq=%0d Hz, gain=%.3f dB", freq, gain_db);
        end

        $fclose(fd);
    end

endmodule
