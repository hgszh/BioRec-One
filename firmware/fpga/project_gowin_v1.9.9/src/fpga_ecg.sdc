//Copyright (C)2014-2025 GOWIN Semiconductor Corporation.
//All rights reserved.
//File Title: Timing Constraints file
//Tool Version: V1.9.9 (64-bit) 
//Created Time: 2025-07-15 17:05:18
create_clock -name dac8830_clk -period 333.333 -waveform {0 166.667} [get_ports {dac8830_sclk}]
create_clock -name clk_xtal_27 -period 37.037 -waveform {0 18.518} [get_ports {clk_xtal_27}]
create_clock -name spi_sclk -period 50 -waveform {0 25} [get_ports {spi_sclk}]
create_clock -name ads1259_clk -period 333.333 -waveform {0 166.667} [get_ports {ads1259_clk}]
set_clock_groups -asynchronous -group [get_clocks {clk_xtal_27}] -group [get_clocks {spi_sclk}]

set_false_path -from [get_regs {filter_process/i_hpf_bypass_reg_s0}] -to [get_regs {filter_process/notch_gen[0].notch_filter/*}] 
set_false_path -from [get_regs {filter_process/lpf_data_24*}] -to [get_regs {filter_process/hpf_filter/*}] 
set_false_path -from [get_regs {filter_process/lpf_data_24*}] -to [get_regs {filter_process/notch_gen[0].notch_filter/*}]

set_multicycle_path -from [get_regs {filter_process/hpf_filter/xin_reg*}] -to [get_regs {filter_process/hpf_filter/o_data*}]  -setup -end 7500
set_multicycle_path -from [get_regs {filter_process/hpf_filter/xin_reg*}] -to [get_regs {filter_process/hpf_filter/o_data*}]  -hold -end 7499
set_multicycle_path -from [get_regs {filter_process/hpf_filter/xin_reg*}] -to [get_regs {filter_process/hpf_filter/yin_reg*}]  -setup -end 7500
set_multicycle_path -from [get_regs {filter_process/hpf_filter/xin_reg*}] -to [get_regs {filter_process/hpf_filter/yin_reg*}]  -hold -end 7499

set_multicycle_path -from [get_regs {filter_process/hpf_filter/o_data*}] -to [get_regs {filter_process/notch_gen[0].notch_filter/*}]  -setup -end 7500
set_multicycle_path -from [get_regs {filter_process/hpf_filter/o_data*}] -to [get_regs {filter_process/notch_gen[0].notch_filter/*}]  -hold -end 7499

set_multicycle_path -from [get_regs {filter_process/notch_gen[0].notch_filter/*}] -to [get_regs {filter_process/notch_gen[1].notch_filter/yin_reg*}]  -setup -end 7500
set_multicycle_path -from [get_regs {filter_process/notch_gen[0].notch_filter/*}] -to [get_regs {filter_process/notch_gen[1].notch_filter/yin_reg*}]  -hold -end 7499
