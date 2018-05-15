# create a dictionary of compatible VLNVs
source $::env(TAPASCO_HOME)/common/common_ip.tcl
dict set stdcomps dualdma vlnv "esa.informatik.tu-darmstadt.de:user:dual_dma:1.7"
dict set stdcomps clk_wiz vlnv "xilinx.com:ip:clk_wiz:5.3"
