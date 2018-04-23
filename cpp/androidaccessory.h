#pragma once

#include <string>
#include <stdexcept>

#include <stdint.h>
#include <libusb.h>

class AccessoryException : public std::exception
{
public:
  AccessoryException(int code, const char * message)
      : code_(code)
      , message_(message)
  {
  }

  int code() const
  {
      return code_;
  }

  const char * what() const noexcept
  {
      return message_;
  }

private:
  int code_;

  const char * message_;
};

void throwUSBError(int code);

class AndroidAccessory
{
public:
    AndroidAccessory(libusb_context * usbContext, uint32_t vid, uint32_t pid, const std::string & manufacturer, const std::string & model, const std::string & description, const std::string & version, const std::string & uri, const std::string & serial);

    ~AndroidAccessory();

    void open();

    void close();

    int read(uint8_t * buf, int size, unsigned int timeout = 0);

    int write(uint8_t * data, int size, unsigned int timeout = 0);

private:
    AndroidAccessory(const AccessoryException &);
    AndroidAccessory & operator = (const AndroidAccessory &);

    void openAccessoryDevice();
    void closeDevice();
    uint16_t getProtocol();
    void sendString(int index, const std::string & str);
    void switchDevice();
    void openDevice();
    void findEndpoints();
    void throwErrorIfNotOpen();

    libusb_context * usbContext_;
    libusb_device_handle * handle_;

    uint8_t inAddr_;
    uint8_t outAddr_;

    uint32_t vid_;
    uint32_t pid_;
    std::string manufacturer_;
    std::string model_;
    std::string description_;
    std::string version_;
    std::string uri_;
    std::string serial_;
};
