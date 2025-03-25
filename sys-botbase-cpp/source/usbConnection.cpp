#include "defines.h"
#include "usbConnection.h"

namespace UsbConnection {
	bool UsbConnection::initialize() {
		return R_FAILED(usbCommsInitialize());
	}

	void UsbConnection::connect() {

	}

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    int UsbConnection::sendData(void* data, u64 length, int sockfd) {
		USBResponse response{};
		response.data = &data;
		response.size = length;

		usbCommsWrite((void*)&response, 4);

		int ret;
		while (response.size > 0) {
			ret = usbCommsWrite(response.data, response.size);
			if (ret < 0)
				return ret;
		}

        return 0;
    }
}
