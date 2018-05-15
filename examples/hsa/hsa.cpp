#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <vector>
#include <sys/utsname.h>
#include <tapasco.hpp>
#include <platform.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <atomic>
#include <csignal>

#include "hsa_types.h"
#include "hsa_aql_queue.h"
#include "hsa_dma.h"

using namespace tapasco;

hsa_kernel_dispatch_packet_t generatePkg(uint32_t width, uint32_t height, uint64_t offset_kernargs, uint64_t offset_image, uint64_t completion_signal, void *buffer_virt, uint64_t buffer_device) {
    std::string colormodelstring = "UINT16_GRAY_SCALE";
    std::string opstring = "GAUSS3x3";

    int colormodel = 0;
    int op = 0;
    op = get_optype_int_representation(opstring);
    colormodel = get_colormodel_int_representation(colormodelstring);

    hsa_kernel_dispatch_packet_t packet;

    hsa_signal_t signal = {completion_signal};

    // Adding kernags
    uint64_t *kernargs = (uint64_t*)((uint64_t)buffer_virt + offset_kernargs);
    kernargs[0] = buffer_device + offset_image;
    kernargs[1] = buffer_device + offset_image;
    kernargs[2] = (uint64_t)colormodel;

    packet.header = header(HSA_PACKET_TYPE_KERNEL_DISPATCH);
    packet.setup = setup(2);
    packet.kernel_object = (uint64_t)op;
    packet.kernarg_address = (void *)(buffer_device + offset_kernargs);
    packet.grid_size_x = width;
    packet.grid_size_y = height;
    packet.completion_signal = signal;

    std::cout << std::hex << "Header: " << packet.header << std::endl;
    std::cout << std::hex << "Setup: " << packet.setup << std::endl;
    std::cout << std::hex << "Kernel Object: " << packet.kernel_object << std::endl;
    std::cout << std::hex << "Size X: " << packet.grid_size_x << std::endl;
    std::cout << std::hex << "Size Y: " << packet.grid_size_y << std::endl;
    std::cout << std::hex << "Signal: " << packet.completion_signal.handle << std::endl;
    std::cout << std::hex << "Kernarg Address: " << packet.kernarg_address << std::endl;
    std::cout << std::hex << "Kernarg 0: " << std::hex << kernargs[0] << std::dec << std::endl;
    std::cout << std::hex << "Kernarg 1: " << std::hex << kernargs[1] << std::dec << std::endl;
    std::cout << std::hex << "Kernarg 2: " << std::hex << kernargs[2] << std::dec << std::endl;

    return packet;
}

uint64_t calculate_package_index(uint64_t ptr) {
    return ptr % HSA_QUEUE_LENGTH;
}

std::atomic<bool> quit(false);    // signal flag

void got_signal(int)
{
    quit.store(true);
}

int main(int argc, const char *argv[]) {
    std::cout << "HSA hardware test program." << std::endl;

    Tapasco tapasco;

    signal(SIGINT, got_signal);

    std::cout << "Opening AQL Queue driver." << std::endl;
    aql_queue queue;

    // Aquiring signal to be used as completion signal
    // Allocating signal, we'll receive:
    //  - Device Physical Address to put in AQL packages
    //  - Index of the signal that has been allocated to access the signal in the mapped dma_mem
    std::cout << "Allocating completion signal." << std::endl;
    hsa_ioctl_params completion_signal;
    queue.allocate_signal(&completion_signal);
    uint64_t *completion = queue.get_signal_userspace(completion_signal);
    std::cout << "Allocating doorbell signal." << std::endl;
    hsa_ioctl_params doorbell_signal;
    queue.allocate_signal(&doorbell_signal);
    uint64_t *doorbell = queue.get_signal_userspace(doorbell_signal);

    // Fetch write pointer from the hardware -> Make sure we enqueue our packages at the correct location
    uint64_t current_write_ptr;
    platform::platform_read_ctl(0x600000 + 208, sizeof(current_write_ptr), &current_write_ptr, platform::PLATFORM_CTL_FLAGS_RAW);
    std::cout << "Fetched old write pointer from hardware as " << current_write_ptr << std::endl;

    *doorbell = current_write_ptr;
    // Announce doorbell signal to the hardware
    std::cout << "Set doorbell signal." << std::endl;
    queue.set_doorbell(doorbell_signal);

    std::cout << "Opening dummy DMA memory." << std::endl;
    hsa_dma mem;

    hsa_kernel_dispatch_packet_t *pkgQueue = (hsa_kernel_dispatch_packet_t *)queue.get_package_queue();
    pkgQueue[calculate_package_index(*doorbell)] = generatePkg(40, 20, 0x0, 0x1000,
            queue.get_signal_device(completion_signal), mem.getVirtualAddr(), mem.getDevAddr());

    // Initialize value of completion signal
    // Should be done atomic for run-time
    *completion = 1;
    *doorbell += 1;

    while (*completion != 0) {
        std::cout << "Waiting for completion" << std::endl;

        uint64_t cmd;

        // Print some status registers for debugging
        // Counter for signal sent and acked, should be equal
        platform::platform_read_ctl(0x600000 + 88, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Idle cycles " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 80, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Fetch iterations " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 176, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Packages Fetched is " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 184, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Packages Invalidated is " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 192, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Read_index is " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 200, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Read_index_old is " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 208, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Write_index is " << cmd << std::endl;

        platform::platform_read_ctl(0x600000 + 216, sizeof(cmd), &cmd, platform::PLATFORM_CTL_FLAGS_RAW);
        std::cout << "Write_index_old is " << cmd << std::endl;

        if( quit.load() ) break;    // exit normally after SIGINT
        sleep(1);
    }

    queue.unset_doorbell(doorbell_signal);
    queue.deallocate_signal(doorbell_signal);
    queue.deallocate_signal(completion_signal);
    return 0;
}
