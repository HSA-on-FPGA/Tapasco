#ifndef HSA_DMA_H
#define HSA_DMA_H

#include "hsa_dma_ioctl_calls.h"

class hsa_dma {
public:
    hsa_dma() : fd(0), dma_mem(0), dma_mem_device(0), mem_size(0) {
        fd = open("/dev/HSA_DMA_BUFFER_0", O_RDWR, 0);
        if (fd < 0) {
            std::cout << "Could not open HSA_DMA_BUFFER_0" << std::endl;
            throw 1;
        }

        hsa_dma_ioctl_params dma_buffer_addr_device;
        if (ioctl(fd, IOCTL_CMD_HSA_DMA_ADDR, &dma_buffer_addr_device)) {
            std::cout << "Couldn't fetch DMA Buffer address." << std::endl;
            throw 1;
        }
        dma_mem_device = dma_buffer_addr_device.data;

        hsa_dma_ioctl_params dma_buffer_size;
        if (ioctl(fd, IOCTL_CMD_HSA_DMA_SIZE, &dma_buffer_size)) {
            std::cout << "Couldn't fetch DMA Buffer size." << std::endl;
            throw 1;
        }
        mem_size = dma_buffer_size.data;

        dma_mem = mmap(0, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (dma_mem == MAP_FAILED) {
            std::cout << "Couldn't get mapping for DMA Buffer" << std::endl;
            throw 1;
        }
    }

    virtual ~hsa_dma() {
        if (dma_mem && mem_size)
            munmap((void*) dma_mem, mem_size);
        if (fd)
            close(fd);
    }

    void *getVirtualAddr() {
        return dma_mem;
    }

    uint64_t getDevAddr() {
        return dma_mem_device;
    }

    size_t getSize() {
        return mem_size;
    }

private:
    int fd;
    void *dma_mem;
    uint64_t dma_mem_device;
    size_t mem_size;
};

#endif
