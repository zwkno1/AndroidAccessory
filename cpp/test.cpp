#include <iostream>
#include <vector>

#include "androidaccessory.h"

struct USBContext
{
	USBContext()
		: context_(0)
	{
		int ret = libusb_init(&context_);
		throwUSBError(ret);
		// debug
		//libusb_set_option(context_, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_DEBUG);
	}

	~USBContext()
	{
		if(context_)
		  libusb_exit(context_);
	}

	libusb_context * context_;
};

static void print_devs(libusb_context * ctx)
{
    libusb_device **devs;
    ssize_t cnt;
    cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0)
        return;

    libusb_device *dev;
    int i = 0, j = 0;
    uint8_t path[8];

    while ((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            fprintf(stderr, "failed to get device descriptor");
            return;
        }

        printf("%04x:%04x (bus %d, device %d)",
            desc.idVendor, desc.idProduct,
            libusb_get_bus_number(dev), libusb_get_device_address(dev));

        r = libusb_get_port_numbers(dev, path, sizeof(path));
        if (r > 0) {
            printf(" path: %d", path[0]);
            for (j = 1; j < r; j++)
                printf(".%d", path[j]);
        }
        printf("\n");
    }

    libusb_free_device_list(devs, 1);
}

int main(int argc, char *argv[])
{
	USBContext context;
    print_devs(context.context_);

    AndroidAccessory accessory(context.context_, 0x2717, 0xff40, "AccessoryTest", "TestDemo", "test", "1.0", "http://zwkno1.github.com", "");
    try
    {
        accessory.open();
    }
    catch(const AccessoryException & e)
    {
        std::cout << "Open Error: " << e.code() << ", " << e.what() << std::endl;
    }

    std::vector<uint8_t> recvBuf(1024);
	int bytes = 0;
    while(true)
    {
        try
        {
            int ret = accessory.read(&recvBuf[0], recvBuf.size() - 1, 200);
			recvBuf.resize(ret + 1);
            recvBuf[ret] = '\0';
            std::cout << "read " << ret << "bytes: " << &recvBuf[0] << std::endl;
        }
        catch(const AccessoryException & e)
        {
			if((e.code() != LIBUSB_ERROR_TIMEOUT) && (e.code() != LIBUSB_ERROR_OVERFLOW))
			{
				std::cout << "Read Error: " << e.code() << ", " << e.what() << std::endl;
				break;
			}

			if(e.code() == LIBUSB_ERROR_OVERFLOW)
            {
				recvBuf.resize(recvBuf.size()*2);
            }
        }

		if(bytes <= 0)
		  continue;

        try
        {
            bytes = accessory.write(&recvBuf[0], bytes);
            std::cout << "send " << bytes << " bytes" << std::endl;
        }
        catch(const AccessoryException & e)
        {
            std::cout << "Write Error: " << e.code() << ", " << e.what() << std::endl;
            break;
        }
		bytes = 0;
    }

    return 0;
}
