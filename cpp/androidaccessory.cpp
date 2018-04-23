#include "androidaccessory.h"

#include <thread>
#include <chrono>

namespace
{

inline void delay(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void throwError(const char * message)
{
    throw AccessoryException(0, message);
}

const uint8_t ACCESSORY_STRING_MANUFACTURER   = 0;
const uint8_t ACCESSORY_STRING_MODEL          = 1;
const uint8_t ACCESSORY_STRING_DESCRIPTION    = 2;
const uint8_t ACCESSORY_STRING_VERSION        = 3;
const uint8_t ACCESSORY_STRING_URI            = 4;
const uint8_t ACCESSORY_STRING_SERIAL         = 5;

const uint8_t ACCESSORY_GET_PROTOCOL          = 51;
const uint8_t ACCESSORY_SEND_STRING           = 52;
const uint8_t ACCESSORY_START                 = 53;

const uint16_t GOOGLE_VENDOR_ID = 0x18D1;
const uint16_t GOOGLE_PRODUCT_ID[] = { 0x2D00, 0x2D01, 0x2D04, 0x2D05 };

}

void throwUSBError(int code)
{
	if(code < 0)
	  throw AccessoryException(code, libusb_error_name(code));
}

AndroidAccessory::AndroidAccessory(libusb_context * usbContext, uint32_t vid, uint32_t pid, const std::string & manufacturer, const std::string & model, const std::string & description, const std::string & version, const std::string & uri, const std::string & serial)
    : usbContext_(usbContext)
    , handle_(0)
    , inAddr_(0)
    , outAddr_(0)
    , vid_(vid)
    , pid_(pid)
    , manufacturer_(manufacturer)
    , model_(model)
    , description_(description)
    , version_(version)
    , uri_(uri)
    , serial_(serial)
{
}

AndroidAccessory::~AndroidAccessory()
{
    closeDevice();
}

void AndroidAccessory::open()
{
    try
    {
        openAccessoryDevice();
    }
    catch(AccessoryException& e)
    {
        openDevice();
        switchDevice();
        delay(2000);
        openAccessoryDevice();
    }

    delay(100);
}

void AndroidAccessory::close()
{
    closeDevice();
}

int AndroidAccessory::read(uint8_t * buf, int size, unsigned int timeout)
{
    throwErrorIfNotOpen();
    int ret = 0;
    int bytes = 0;
    ret = libusb_bulk_transfer(handle_, inAddr_, buf, size, &bytes, timeout);
    if(ret < 0)
        throwUSBError(ret);
    return bytes;
}

int AndroidAccessory::write(uint8_t * data, int size, unsigned int timeout)
{
    throwErrorIfNotOpen();
    int ret = 0;
    int sent = 0;

    ret = libusb_bulk_transfer(handle_, outAddr_, data, size, &sent, timeout);
    if(ret < 0)
        throwUSBError(ret);
    return sent;
}

void AndroidAccessory::openAccessoryDevice()
{
    closeDevice();

    for(auto & pid : GOOGLE_PRODUCT_ID)
    {
        if((handle_ = libusb_open_device_with_vid_pid(usbContext_, GOOGLE_VENDOR_ID, pid)) != 0)
            break;
    }

    if(handle_ == 0)
    {
        throwError("No Device Found");
    }

    int ret = libusb_claim_interface(handle_, 0);
    if(ret < 0)
    {
        closeDevice();
        throwUSBError(ret);
    }

    findEndpoints();
}

void AndroidAccessory::closeDevice()
{
    if(handle_ == 0)
        return;
    libusb_release_interface(handle_, 0);
    libusb_close(handle_);
    handle_ = 0;
}

uint16_t AndroidAccessory::getProtocol()
{
    uint16_t protocol;
    int ret = libusb_control_transfer(handle_, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, ACCESSORY_GET_PROTOCOL, 0, 0, (unsigned char *)&protocol, 2, 0);
    if(ret < 0)
    {
        closeDevice();
        throwUSBError(ret);
    }
    return protocol;
}

void AndroidAccessory::sendString(int index, const std::string & str)
{
    int ret = libusb_control_transfer(handle_, LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, ACCESSORY_SEND_STRING, 0, index, (unsigned char *)str.data(), str.size()+1, 0);
    if(ret < 0)
    {
        closeDevice();
        throwUSBError(ret);
    }
}

void AndroidAccessory::switchDevice()
{
    uint16_t protocol = getProtocol();
    if (protocol < 1)
    {
        throwError("Get Protocol Version");
    }

    sendString(ACCESSORY_STRING_MANUFACTURER, manufacturer_);
    sendString(ACCESSORY_STRING_MODEL, model_);
    sendString(ACCESSORY_STRING_DESCRIPTION, description_);
    sendString(ACCESSORY_STRING_VERSION, version_);
    sendString(ACCESSORY_STRING_URI, uri_);
    sendString(ACCESSORY_STRING_SERIAL, serial_);

    int ret = libusb_control_transfer(handle_,  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, ACCESSORY_START, 0, 0, 0, 0, 0);
    if(ret < 0)
    {
        throwUSBError(ret);
    }

    closeDevice();
}

void AndroidAccessory::openDevice()
{
    closeDevice();

    if((handle_ = libusb_open_device_with_vid_pid(0, vid_, pid_)) == 0)
    {
        throwError("No Device Found");
    }

#ifdef __linux__
    libusb_detach_kernel_driver(handle_, 0);
#endif

    int ret  = libusb_claim_interface(handle_, 0);
    if(ret != 0)
    {
        closeDevice();
        throwUSBError(ret);
    }
}

void AndroidAccessory::findEndpoints()
{
    throwErrorIfNotOpen();

    inAddr_ = 0;
    outAddr_ = 0;

    libusb_device * dev = libusb_get_device(handle_);
    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(dev, &desc);
    if(ret < 0)
    {
        closeDevice();
        throwUSBError(ret);
    }

    libusb_config_descriptor * config = 0;
    ret = libusb_get_active_config_descriptor(dev, &config);
    if(ret < 0)
    {
        closeDevice();
        throwUSBError(ret);
    }

    for(int i = 0; i < config->bNumInterfaces; ++i)
    {
        for(int j = 0; j < config->interface[i].num_altsetting; ++j)
        {
            for(int k = 0; k < config->interface[i].altsetting[j].bNumEndpoints; ++k)
            {
                uint8_t addr = config->interface[i].altsetting[j].endpoint[k].bEndpointAddress;
                if((inAddr_ == 0) && (addr & LIBUSB_ENDPOINT_DIR_MASK))
                {
                    inAddr_ = addr;
                }
                else if(outAddr_ == 0)
                {
                    outAddr_ = addr;
                }

                if((inAddr_ != 0) && (outAddr_ != 0))
                {
                    break;
                }
            }
        }
    }

    libusb_free_config_descriptor(config);

    if(inAddr_ == 0)
    {
        closeDevice();
        throwError("In Endpoints Not Found");
    }

    if(outAddr_ == 0)
    {
        closeDevice();
        throwError("Out Endpoints Not Found");
    }
}

void AndroidAccessory::throwErrorIfNotOpen()
{
    if(handle_ == 0)
        throwError("Device Not Open");
}
