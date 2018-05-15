namespace eval hsa_infrastructure {
  set vlnv "esa.informatik.tu-darmstadt.de:user:HSAWrapper:1.0"
  set vlnv_package_processor "fau.de:hsa:accelerator_backend:1.0"

  proc create_hsa_infrastructure {{name "HSAInfrastructure"}} {
    variable vlnv
    variable vlnv_package_processor
    if {[tapasco::is_platform_feature_enabled "HSA"]} {
    puts "Instantiating HSAInfrastructure IP ..."
    puts "  VLNV = $vlnv"
    puts "  VLNV Package Processor = $vlnv_package_processor"
    puts "  NAME = $name"
    set dir "$::env(FAU_HOME)"
     if { $dir eq "" } {
        puts "FAU_HOME dir is not set."
        exit 1
     }
     set repos [get_property ip_repo_paths [current_project]]
     set_property  ip_repo_paths [lappend repos $dir] [current_project]
    update_ip_catalog

    create_bd_cell -type hier HSA

    create_bd_cell -type ip -vlnv $vlnv HSA/HSAWrapper_0

    set toDDR [tapasco::createInterconnect "HSA/toDDR" 3 1]
    set fromHost [tapasco::createInterconnect "HSA/fromHost" 1 2]

    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/fromHost/M00_AXI] [get_bd_intf_pins HSA/HSAWrapper_0/squeue_axi]
    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/toDDR/S00_AXI] [get_bd_intf_pins HSA/HSAWrapper_0/mddr_axi]
    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/toDDR/S01_AXI] [get_bd_intf_pins HSA/HSAWrapper_0/mdma_ddr_axi]

    set toPCIe [tapasco::createInterconnect "HSA/toPCIe" 2 1]

    connect_bd_intf_net [get_bd_intf_pins HSA/HSAWrapper_0/mdma_pcie_axi] -boundary_type upper [get_bd_intf_pins HSA/toPCIe/S00_AXI]
    connect_bd_intf_net [get_bd_intf_pins HSA/HSAWrapper_0/mpcie_axi] -boundary_type upper [get_bd_intf_pins HSA/toPCIe/S01_AXI]

    create_bd_cell -type ip -vlnv $vlnv_package_processor HSA/HSAAccelerator

    set toWrapper [tapasco::createInterconnect "HSA/toWrapper" 2 1]

    connect_bd_intf_net [get_bd_intf_pins HSA/HSAAccelerator/M_CMD_AXI] -boundary_type upper [get_bd_intf_pins HSA/toWrapper/S00_AXI]

    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/fromHost/M01_AXI] [get_bd_intf_pins HSA/toWrapper/S01_AXI]
    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/toWrapper/M00_AXI] [get_bd_intf_pins HSA/HSAWrapper_0/swrapper_axi]

    connect_bd_intf_net [get_bd_intf_pins HSA/HSAAccelerator/M_DATA_AXI] -boundary_type upper [get_bd_intf_pins HSA/toDDR/S02_AXI]

    set pcie_aclk [get_bd_pins Resets/pcie_aclk]
    set pcie_interconnect_aresetn [get_bd_pins Resets/pcie_interconnect_aresetn]
    set pcie_peripheral_aresetn [get_bd_pins Resets/pcie_peripheral_aresetn]

    set design_aclk [get_bd_pins Resets/design_aclk]
    set design_clk_interconnect_aresetn [get_bd_pins Resets/design_clk_interconnect_aresetn]
    set design_clk_peripheral_aresetn [get_bd_pins Resets/design_clk_peripheral_aresetn]

    set ddr_aclk [get_bd_pins Resets/ddr_aclk]
    set ddr_clk_interconnect_aresetn [get_bd_pins Resets/ddr_clk_interconnect_aresetn]
    set ddr_clk_peripheral_aresetn [get_bd_pins Resets/ddr_clk_peripheral_aresetn]


    # Interrupts
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/tp_halt] [get_bd_pins HSA/HSAWrapper_0/pe_halt]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_aql_irq] [get_bd_pins HSA/HSAWrapper_0/rcv_aql_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_cpl_irq] [get_bd_pins HSA/HSAWrapper_0/rcv_cpl_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_dma_irq] [get_bd_pins HSA/HSAWrapper_0/rcv_dma_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_cpl_irq_ack] [get_bd_pins HSA/HSAWrapper_0/snd_cpl_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_dma_irq_ack] [get_bd_pins HSA/HSAWrapper_0/snd_dma_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_aql_irq_ack] [get_bd_pins HSA/HSAWrapper_0/rcv_aql_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_cpl_irq_ack] [get_bd_pins HSA/HSAWrapper_0/rcv_cpl_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_dma_irq_ack] [get_bd_pins HSA/HSAWrapper_0/rcv_dma_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_cpl_irq] [get_bd_pins HSA/HSAWrapper_0/snd_cpl_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_dma_irq] [get_bd_pins HSA/HSAWrapper_0/snd_dma_irq]

     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_add_irq] [get_bd_pins HSA/HSAWrapper_0/rcv_add_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_rem_irq] [get_bd_pins HSA/HSAWrapper_0/rcv_rem_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_add_irq_ack] [get_bd_pins HSA/HSAWrapper_0/snd_add_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_rem_irq_ack] [get_bd_pins HSA/HSAWrapper_0/snd_rem_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_add_irq_ack] [get_bd_pins HSA/HSAWrapper_0/rcv_add_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rcv_rem_irq_ack] [get_bd_pins HSA/HSAWrapper_0/rcv_rem_irq_ack]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_add_irq] [get_bd_pins HSA/HSAWrapper_0/snd_add_irq]
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/snd_rem_irq] [get_bd_pins HSA/HSAWrapper_0/snd_rem_irq]

    # from Host
     connect_bd_net [get_bd_pins HSA/fromHost/ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/ARESETN] $design_clk_interconnect_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/S00_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/S00_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/M00_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/M00_ARESETN] $design_clk_peripheral_aresetn
     connect_bd_net [get_bd_pins HSA/fromHost/M01_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/fromHost/M01_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/clk] $design_aclk
     connect_bd_net [get_bd_pins HSA/HSAAccelerator/rstn] $design_clk_peripheral_aresetn

     connect_bd_net [get_bd_pins HSA/toWrapper/ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toWrapper/ARESETN] $design_clk_interconnect_aresetn
     connect_bd_net [get_bd_pins HSA/toWrapper/S00_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toWrapper/S00_ARESETN] $design_clk_peripheral_aresetn
     connect_bd_net [get_bd_pins HSA/toWrapper/M00_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toWrapper/M00_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toWrapper/S01_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toWrapper/S01_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper

     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/mdma_pcie_axi_aclk] $pcie_aclk
     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/mdma_pcie_axi_aresetn] $pcie_peripheral_aresetn

     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/mdma_ddr_axi_aclk] $ddr_aclk
     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/mdma_ddr_axi_aresetn] $ddr_clk_peripheral_aresetn

     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/s_axi_aclk] $design_aclk
     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/s_axi_aresetn] $design_clk_peripheral_aresetn

     disconnect_bd_net /InterruptControl/irq_unused_dout [get_bd_pins InterruptControl/interrupt_concat/In2]
     connect_bd_net [get_bd_pins HSA/HSAWrapper_0/hsa_signal_interrupt] [get_bd_pins InterruptControl/interrupt_concat/In2]

     connect_bd_net [get_bd_pins HSA/toPCIe/ACLK] $pcie_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/ARESETN] $pcie_interconnect_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/S00_ACLK] $pcie_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/S00_ARESETN] $pcie_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/M00_ACLK] $pcie_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/M00_ARESETN] $pcie_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/S01_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toPCIe/S01_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/ACLK] $ddr_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/ARESETN] $ddr_clk_interconnect_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S00_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S00_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/M00_ACLK] $ddr_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/M00_ARESETN] $ddr_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S01_ACLK] $ddr_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S01_ARESETN] $ddr_clk_peripheral_aresetn -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S02_ACLK] $design_aclk -boundary_type upper
     connect_bd_net [get_bd_pins HSA/toDDR/S02_ARESETN] $design_clk_peripheral_aresetn -boundary_type upper

    set mi [get_property CONFIG.NUM_MI [get_bd_cells axi_ic_from_host]]
    set mi_new [expr "$mi + 1"]
    set_property CONFIG.NUM_MI $mi_new [get_bd_cells axi_ic_from_host]
    connect_bd_intf_net -boundary_type upper [get_bd_intf_pins [format "axi_ic_from_host/M%02d_AXI" $mi]] [get_bd_intf_pins HSA/fromHost/S00_AXI]
    connect_bd_net [get_bd_pins [format "axi_ic_from_host/M%02d_ACLK" $mi]] $design_aclk -boundary_type upper
    connect_bd_net [get_bd_pins [format "axi_ic_from_host/M%02d_ARESETN" $mi]] $design_clk_peripheral_aresetn -boundary_type upper

     set si [get_property CONFIG.NUM_SI [get_bd_cells axi_ic_to_host]]
     set si_new [expr "$si + 1"]
     set_property CONFIG.NUM_SI $si_new [get_bd_cells axi_ic_to_host]
     connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/toPCIe/M00_AXI] [get_bd_intf_pins [format "axi_ic_to_host/S%02d_AXI" $si]]
     connect_bd_net [get_bd_pins [format "axi_ic_to_host/S%02d_ACLK" $si]] $pcie_aclk -boundary_type upper
     connect_bd_net [get_bd_pins [format "axi_ic_to_host/S%02d_ARESETN" $si]] $pcie_peripheral_aresetn -boundary_type upper

     set si [get_property CONFIG.NUM_SI [get_bd_cells /Memory/mig_ic]]
     set si_new [expr "$si + 1"]
     set_property CONFIG.NUM_SI $si_new [get_bd_cells /Memory/mig_ic]
     connect_bd_intf_net -boundary_type upper [get_bd_intf_pins HSA/toDDR/M00_AXI] [get_bd_intf_pins [format "/Memory/mig_ic/S%02d_AXI" $si]]
     connect_bd_net [get_bd_pins [format "/Memory/mig_ic/S%02d_ACLK" $si]] $ddr_aclk -boundary_type upper
     connect_bd_net [get_bd_pins [format "/Memory/mig_ic/S%02d_ARESETN" $si]] $ddr_clk_peripheral_aresetn -boundary_type upper
     save_bd_design
    }
    return ""
  }

  proc setup_address_map {} {
      if {[tapasco::is_platform_feature_enabled "HSA"]} {
    set ms [get_bd_addr_spaces HSA/HSAWrapper_0/mddr_axi]
    set ts [get_bd_addr_segs {Memory/mig/memmap/memaddr}]
    create_bd_addr_seg -range 4G -offset 0x0001000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]

    set ms [get_bd_addr_spaces HSA/HSAWrapper_0/mdma_ddr_axi]
    create_bd_addr_seg -range 4G -offset 0x0001000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]

    set ms [get_bd_addr_spaces HSA/HSAWrapper_0/mpcie_axi]
    set ts [get_bd_addr_segs {PCIe/axi_pcie3_0/S_AXI/BAR0}]
    create_bd_addr_seg -range 16E -offset 0x0000000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]

    set ms [get_bd_addr_spaces HSA/HSAWrapper_0/mdma_pcie_axi]
    create_bd_addr_seg -range 16E -offset 0x0000000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]

    set ms [get_bd_addr_spaces HSA/HSAAccelerator/M_DATA_AXI]
    set ts [get_bd_addr_segs {Memory/mig/memmap/memaddr}]
    create_bd_addr_seg -range 4G -offset 0x0001000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]

    set ms [get_bd_addr_spaces HSA/HSAAccelerator/M_CMD_AXI]
    set ts [get_bd_addr_segs {HSA/HSAWrapper_0/swrapper_axi/reg0}]
    create_bd_addr_seg -range 4k -offset 0x0002000000000000 $ms $ts [format "SEG_%s_%s" $ms $ts]


        set ms [get_bd_addr_spaces PCIe/axi_pcie3_0/M_AXI]
        set ts [get_bd_addr_segs {HSA/HSAWrapper_0/swrapper_axi/reg0}]
        create_bd_addr_seg -range 4k -offset 0x0002000000000000 $ms $ts [format "SEG_%s_wrapper" $ms]
        set ts [get_bd_addr_segs {HSA/HSAWrapper_0/squeue_axi/reg0}]
        create_bd_addr_seg -range 4k -offset 0x0002000000600000 $ms $ts [format "SEG_%s_queue" $ms]
    }
  }

}

tapasco::register_plugin "platform::hsa_infrastructure::create_hsa_infrastructure" "post-platform"
tapasco::register_plugin "platform::hsa_infrastructure::setup_address_map" "post-address-map"
