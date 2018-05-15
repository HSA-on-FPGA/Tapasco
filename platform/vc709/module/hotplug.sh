VENDOR=10EE
        DEVICE=7038
        PCIEDEVICE=`lspci -d $VENDOR:$DEVICE | sed -e "s/ .*//"`
        echo "hotplugging device: $PCIEDEVICE"
        # remove device, if it exists
        if [ -n "$PCIEDEVICE" ]; then
                sudo sh -c "echo 1 >/sys/bus/pci/devices/0000:$PCIEDEVICE/remove"
        fi

        # Scan for new hotplugable device, like the one may deleted before
        sudo sh -c "echo 1 >/sys/bus/pci/rescan"
        echo "hotplugging finished"

