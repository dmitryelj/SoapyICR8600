/*
  * Icom ICR8600 SoapySDR Library
 *
 * Made in 2018 by D.Eliuseev dmitryelj@gmail.com
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "WinUSBDevice.h"

#ifdef _WIN32

#pragma comment(lib, "winusb.lib") 
#pragma comment(lib, "Cfgmgr32.lib")

#else

void Sleep(int milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

#endif

#define decToBcd(val) (UCHAR)((((val) / 10 * 16) + ((val) % 10)))
#define bcdToDec(val) (ULONG)((val>>4)*10 + (val & 0x0f))

HRESULT RetrieveDevicePath(_Out_bytecap_(BufLen) LPTSTR DevicePath, _In_ ULONG  BufLen, _Out_opt_ PBOOL  FailureDeviceNotFound);

BOOL FindICR8600Device()
{
#ifdef _WIN32
	DEVICE_DATA deviceData;
	BOOL notFound = false;
	HRESULT hr = RetrieveDevicePath(deviceData.DevicePath, sizeof(deviceData.DevicePath), &notFound);
	if (FAILED(hr) || notFound) {
		return FALSE;
	}
	return TRUE;
#else
	int r = libusb_init(NULL);
	if (r < 0)
	    return FALSE;

	BOOL res = FALSE;

    libusb_device **devs;
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
    for (int i = 0; i < cnt; i++) {
        libusb_device *device = devs[i];

        struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(device, &desc);
        if (r < 0) continue;

        printf("Device %04x:%04x, Manufacturer: %d\n", desc.idVendor, desc.idProduct, desc.iManufacturer);

//        libusb_device_handle *handle;
//        int err = libusb_open(device, &handle);
//        if (err) {
//            printf("  Error: cannot open device\n");
//            continue;
//        }
//        // Get description
//        const int STRING_LENGTH = 255;
//        unsigned char stringDescription[STRING_LENGTH];
//        if (desc.iManufacturer > 0) {
//            r = libusb_get_string_descriptor_ascii(handle, desc.iManufacturer, stringDescription, STRING_LENGTH);
//            if (r >= 0)
//                printf("Manufacturer = %s\n",  stringDescription);
//        }
//        if (desc.iProduct > 0) {
//            r = libusb_get_string_descriptor_ascii(handle, desc.iProduct, stringDescription, STRING_LENGTH);
//            if ( r >= 0 )
//                printf("Product = %s\n", stringDescription);
//        }
//        if (desc.iSerialNumber > 0) {
//            r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, stringDescription, STRING_LENGTH);
//            if (r >= 0)
//                printf("SerialNumber = %s\n", stringDescription);
//        }
//        printf("\n");
//        libusb_close(handle);
    }

	libusb_free_device_list(devs, 1);
	libusb_exit(NULL);
    return res;
#endif
}

HRESULT OpenDevice(_Out_ PDEVICE_DATA DeviceData, _Out_opt_ PBOOL FailureDeviceNotFound)
{
#ifdef _WIN32
    HRESULT hr = S_OK;
    BOOL    bResult;

    DeviceData->HandlesOpen = FALSE;

    hr = RetrieveDevicePath(DeviceData->DevicePath, sizeof(DeviceData->DevicePath), FailureDeviceNotFound);
    if (FAILED(hr)) {
        return hr;
    }

    DeviceData->DeviceHandle = CreateFile(DeviceData->DevicePath,
                                          GENERIC_WRITE | GENERIC_READ,
                                          FILE_SHARE_WRITE | FILE_SHARE_READ,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                                          NULL);

    if (INVALID_HANDLE_VALUE == DeviceData->DeviceHandle) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    bResult = WinUsb_Initialize(DeviceData->DeviceHandle, &DeviceData->WinusbHandle);
    if (FALSE == bResult) {

        hr = HRESULT_FROM_WIN32(GetLastError());
        CloseHandle(DeviceData->DeviceHandle);
        return hr;
    }

    DeviceData->HandlesOpen = TRUE;
    return hr;
#else
    return 0;
#endif
}

BOOL GetDeviceDescriptor(WINUSB_INTERFACE_HANDLE hDeviceHandle, _Out_ USB_DEVICE_DESCRIPTOR *pDeviceDesc)
{
#ifdef _WIN32
    ULONG lengthReceived;
	BOOL bResult = WinUsb_GetDescriptor(hDeviceHandle, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0, (PBYTE)pDeviceDesc, sizeof(USB_DEVICE_DESCRIPTOR), &lengthReceived);
	if (FALSE == bResult || lengthReceived != sizeof(USB_DEVICE_DESCRIPTOR)) {
		printf("Error among LastError %d or lengthReceived %d\n", FALSE == bResult ? GetLastError() : 0, lengthReceived);
		return FALSE;
	}
	return TRUE;
#else
    return FALSE;
#endif
}

VOID CloseDevice(_Inout_ PDEVICE_DATA DeviceData)
{
#ifdef _WIN32
    if (FALSE == DeviceData->HandlesOpen) {
        //
        // Called on an uninitialized DeviceData
        //
        return;
    }

    WinUsb_Free(DeviceData->WinusbHandle);
    CloseHandle(DeviceData->DeviceHandle);
    DeviceData->HandlesOpen = FALSE;
	DeviceData->DeviceHandle = INVALID_HANDLE_VALUE;
#else
    return;
#endif
}

HRESULT RetrieveDevicePath(_Out_bytecap_(BufLen) LPTSTR DevicePath, _In_ ULONG  BufLen, _Out_opt_ PBOOL  FailureDeviceNotFound)
{
#ifdef _WIN32
    CONFIGRET cr = CR_SUCCESS;
    HRESULT   hr = S_OK;
    PTSTR     DeviceInterfaceList = NULL;
    ULONG     DeviceInterfaceListLength = 0;

    if (NULL != FailureDeviceNotFound) {
        *FailureDeviceNotFound = FALSE;
    }

    //
    // Enumerate all devices exposing the interface. Do this in a loop
    // in case a new interface is discovered while this code is executing,
    // causing CM_Get_Device_Interface_List to return CR_BUFFER_SMALL.
    //
    do {
        cr = CM_Get_Device_Interface_List_Size(&DeviceInterfaceListLength,
                                               (LPGUID)&GUID_DEVINTERFACE_WinUSB1,
                                               NULL,
                                               CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (cr != CR_SUCCESS) {
            hr = HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA));
            break;
        }

        DeviceInterfaceList = (PTSTR)HeapAlloc(GetProcessHeap(),
                                               HEAP_ZERO_MEMORY,
                                               DeviceInterfaceListLength * sizeof(TCHAR));

        if (DeviceInterfaceList == NULL) {
            hr = E_OUTOFMEMORY;
            break;
        }

        cr = CM_Get_Device_Interface_List((LPGUID)&GUID_DEVINTERFACE_WinUSB1,
                                          NULL,
                                          DeviceInterfaceList,
                                          DeviceInterfaceListLength,
                                          CM_GET_DEVICE_INTERFACE_LIST_PRESENT);

        if (cr != CR_SUCCESS) {
            HeapFree(GetProcessHeap(), 0, DeviceInterfaceList);

            if (cr != CR_BUFFER_SMALL) {
                hr = HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA));
            }
        }
    } while (cr == CR_BUFFER_SMALL);

    if (FAILED(hr)) {
        return hr;
    }

    //
    // If the interface list is empty, no devices were found.
    //
    if (*DeviceInterfaceList == TEXT('\0')) {
        if (NULL != FailureDeviceNotFound) {
            *FailureDeviceNotFound = TRUE;
        }

        hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        HeapFree(GetProcessHeap(), 0, DeviceInterfaceList);
        return hr;
    }

    //
    // Give path of the first found device interface instance to the caller. CM_Get_Device_Interface_List ensured
    // the instance is NULL-terminated.
    //
    hr = StringCbCopy(DevicePath,
                      BufLen,
                      DeviceInterfaceList);

    HeapFree(GetProcessHeap(), 0, DeviceInterfaceList);

    return hr;
#else
    return 0;
#endif
}

BOOL WriteToBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID, ULONG* pcbWritten, PUCHAR send, ULONG cbSize)
{
#ifdef _WIN32
	if (hDeviceHandle == INVALID_HANDLE_VALUE || !pcbWritten) {
		return FALSE;
	}

	BOOL bResult = TRUE;

	ULONG cbSent = 0;
	bResult = WinUsb_WritePipe(hDeviceHandle, ID, send, cbSize, &cbSent, 0);
	if (bResult) {
		printf("WriteToBulkEndpoint 0x%x: %d bytes, actual data transferred: %d.\n", ID, cbSize, cbSent);
		*pcbWritten = cbSent;
	}

	return bResult;
#else
    return FALSE;
#endif
}

BOOL ReadFromBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID, ULONG cbSize)
{
#ifdef _WIN32
	if (hDeviceHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	BOOL bResult = TRUE;
	UCHAR* szBuffer = (UCHAR*)LocalAlloc(LPTR, sizeof(UCHAR)*cbSize);
	ULONG cbRead = 0;
	bResult = WinUsb_ReadPipe(hDeviceHandle, ID, szBuffer, cbSize, &cbRead, 0);
	if (bResult) {
		if (cbRead == 6) {
			// FE FE E0 96 FB FD - OK
			if (szBuffer[3] == 0x96 && szBuffer[4] == 0xFB)
				printf("ReadFromBulkEndpoint: OK\n");
			// FE FE E0 96 FA FD - Fail
			else if (szBuffer[3] == 0x96 && szBuffer[4] == 0xFA)
				printf("ReadFromBulkEndpoint: Fail\n");
			else
				printf("ReadFromBulkEndpoint output: %Xh %Xh %Xh %Xh %Xh %Xh\n", szBuffer[0], szBuffer[1], szBuffer[2], szBuffer[3], szBuffer[4], szBuffer[5]);
		}
		else {
			// printf("ReadFromBulkEndpoint: pipe 0x%x: size %d, actual data read: %d.\n", ID, cbSize, cbRead);
			printf("ReadFromBulkEndpoint output (%d): %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh\n", cbRead, szBuffer[0], szBuffer[1], szBuffer[2], szBuffer[3], szBuffer[4], szBuffer[5], szBuffer[6], szBuffer[7],
				szBuffer[8], szBuffer[9], szBuffer[10], szBuffer[11], szBuffer[12], szBuffer[13], szBuffer[14], szBuffer[15]);
		}
	}

	LocalFree(szBuffer);
	return bResult;
#else
    return FALSE;
#endif
}


ULONG ReadBufferFromBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID, PUCHAR szBuffer, ULONG cbSize)
{
#ifdef _WIN32
	if (hDeviceHandle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	BOOL bResult = TRUE;
	ULONG cbRead = 0;
	bResult = WinUsb_ReadPipe(hDeviceHandle, ID, szBuffer, cbSize, &cbRead, 0);
	if (bResult)
		return cbRead;
	else
		return 0;
#else
    return 0;
#endif
}


BOOL GetAck(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID)
{
#ifdef _WIN32

	if (hDeviceHandle == INVALID_HANDLE_VALUE) {
		SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: INVALID_HANDLE_VALUE");
		return FALSE;
	}

	ULONG cbSize = 64;
	BOOL bResult = TRUE;
	UCHAR* szBuffer = (UCHAR*)LocalAlloc(LPTR, sizeof(UCHAR)*cbSize);
	ULONG cbRead = 0;
	bResult = WinUsb_ReadPipe(hDeviceHandle, ID, szBuffer, cbSize, &cbRead, 0);
	if (bResult) {
		if (cbRead == 6) {
			// FE FE E0 96 FB FD - OK
			if (szBuffer[3] == 0x96 && szBuffer[4] == 0xFB)
				SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: Valid Command");
			// FE FE E0 96 FA FD - Fail
			else if (szBuffer[3] == 0x96 && szBuffer[4] == 0xFA)
			{
				SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: Invalid Command");
				bResult = false;
			}
			else
			{
				SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: Unexpected Response %Xh %Xh %Xh %Xh %Xh %Xh\n", szBuffer[0], szBuffer[1], szBuffer[2], szBuffer[3], szBuffer[4], szBuffer[5]);
				bResult = false;
			}
		}
		else {
			SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: Unexpected Response (%d) %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh  %Xh %Xh %Xh %Xh\n", cbRead, szBuffer[0], szBuffer[1], szBuffer[2], szBuffer[3], szBuffer[4], szBuffer[5], szBuffer[6], szBuffer[7],
				szBuffer[8], szBuffer[9], szBuffer[10], szBuffer[11], szBuffer[12], szBuffer[13], szBuffer[14], szBuffer[15]);
				bResult = false;
		}
	}
	else {
		SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: WinUsb_ReadPipe Failed");
	}

	LocalFree(szBuffer);
	return bResult;
#else
	SoapySDR_logf(SOAPY_SDR_DEBUG, "GetAck: Not _WIN32");
    return FALSE;
#endif
}


BOOL ICR8600SetRemoteOn(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetRemoteOn");
	UCHAR remote_on_cmd[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x00, 0x01,  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, remote_on_cmd, sizeof(remote_on_cmd));
	Sleep(100);
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetRemoteOff(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetRemoteOff");
	ULONG sent = 0;
	UCHAR remote_off_cmd[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x00, 0x00,  0xFD, 0xFF };
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, remote_off_cmd, sizeof(remote_off_cmd));
	Sleep(100);
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetSampleRate(WINUSB_INTERFACE_HANDLE hDeviceHandle, ULONG sampleRate)
{
	UCHAR iq_5120_16bit[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x01,  0xFD, 0xFF };
	UCHAR iq_3840_16bit[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x02,  0xFD, 0xFF };
	UCHAR iq_1920_16bit[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x03,  0xFD, 0xFF };
	UCHAR iq_960_16bit[]  = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x04,  0xFD, 0xFF };
	UCHAR iq_480_16bit[]  = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x05,  0xFD, 0xFF };
	UCHAR iq_240_16bit[]  = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x01, 0x00, 0x06,  0xFD, 0xFF };
	ULONG sent = 0;
	if (sampleRate == 240000) {
		printf("240000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_240_16bit, sizeof(iq_240_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	if (sampleRate == 480000) {
		printf("480000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_480_16bit, sizeof(iq_480_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	if (sampleRate == 960000) {
		printf("960000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_960_16bit, sizeof(iq_960_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	if (sampleRate == 1920000) {
		printf("1920000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_1920_16bit, sizeof(iq_1920_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	if (sampleRate == 3840000) {
		printf("3840000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_3840_16bit, sizeof(iq_3840_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	if (sampleRate == 5120000) {
		printf("5120000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_5120_16bit, sizeof(iq_5120_16bit));
		Sleep(100);
		return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
	}
	return FALSE;
}

BOOL ICR8600SetFrequency(WINUSB_INTERFACE_HANDLE hDeviceHandle, ULONG frequency)
{
	ULONG f1 = frequency % 100, f2 = (frequency / 100) % 100, f3 = (frequency / 10000) % 100, f4 = (frequency / 1000000) % 100, f5 = (frequency / 100000000) % 100;
	UCHAR set_freq[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x05,  decToBcd(f1),decToBcd(f2),decToBcd(f3),decToBcd(f4),decToBcd(f5),  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_freq, sizeof(set_freq));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetAntenna(WINUSB_INTERFACE_HANDLE hDeviceHandle, int antennaIndex)
{
	UCHAR set_ant[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x12, (UCHAR)antennaIndex,  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_ant, sizeof(set_ant));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

ULONG ICR8600ReadPipe(WINUSB_INTERFACE_HANDLE hDeviceHandle, PUCHAR Buffer, ULONG BufferLength) 
{
	ULONG cbRead = 0;
#ifdef _WIN32
	BOOL bResult = WinUsb_ReadPipe(hDeviceHandle, PIPE_IQ_ID, Buffer, BufferLength, &cbRead, 0);
	if (bResult) {
	}
#endif
	return cbRead;
}

BOOL ICR8600SetPreAmpOn(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetPreAmpOn");
	UCHAR set_preamp[] = { 0xFE, 0xFE, 0x96, 0xE0, 0x16, 0x02, 0x01, 0xFD };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_preamp, sizeof(set_preamp));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetPreAmpOff(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetPreAmpOff");
	UCHAR set_preamp[] = { 0xFE, 0xFE, 0x96, 0xE0, 0x16, 0x02, 0x00, 0xFD };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_preamp, sizeof(set_preamp));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetGainRF(WINUSB_INTERFACE_HANDLE hDeviceHandle, ULONG gain)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetGainRF");
	ULONG g1 = gain % 100, g2 = (gain / 100) % 100;
	UCHAR set_gain[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x14, 0x02, decToBcd(g2), decToBcd(g1),  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_gain, sizeof(set_gain));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600SetAttenuator(WINUSB_INTERFACE_HANDLE hDeviceHandle, ULONG atten)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600SetAttenuator");
	UCHAR set_atten[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x11, decToBcd(atten), 0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_atten, sizeof(set_atten));
	return GetAck(hDeviceHandle, PIPE_RESPONSE_ID);
}

BOOL ICR8600GetGainRF(WINUSB_INTERFACE_HANDLE hDeviceHandle, PULONG gain)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetGainRF");
	UCHAR set_gain[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x14, 0x02, 0xFD, 0xFF };
	UCHAR response[64];
	ULONG sent = 0;
	ULONG recv = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_gain, sizeof(set_gain));
	recv = ReadBufferFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, response, sizeof(response));
	if (recv == 10) {
		ULONG g1 = bcdToDec(response[6]);
		ULONG g2 = bcdToDec(response[7]);
		*gain = (g1 * 100) +  g2;
		SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetGainRF RF Gain = %d", *gain);
		return true;
	}
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetGainRF Invalid Command");
	return false;
}

BOOL ICR8600GetPreAmpState(WINUSB_INTERFACE_HANDLE hDeviceHandle, PBOOL on)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetPreAmpState");
	UCHAR set_preamp[] = { 0xFE, 0xFE, 0x96, 0xE0, 0x16, 0x02, 0xFD, 0xFF };
	UCHAR response[64];
	ULONG sent = 0;
	ULONG recv = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_preamp, sizeof(set_preamp));
	recv = ReadBufferFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, response, sizeof(response));
	if (recv == 8) {
		if (response[6] == 0x00) {
			*on = false;
			SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetPreAmpState OFF");
			return true;
		}
		else if (response[6] == 0x01) {
			*on = true;
			SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetPreAmpState ON");
			return true;
		}
		SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetPreAmpState Unexpected Response %Xh", response[6]);
		return false;
	}
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetPreAmpState Invalid Command %d", recv);
	return false;
}

BOOL ICR8600GetAttenuator(WINUSB_INTERFACE_HANDLE hDeviceHandle, PULONG gain)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetAttenuator");
	UCHAR set_gain[] = { 0xFE, 0xFE, 0x96, 0xE0, 0x11, 0xFD };
	UCHAR response[64];
	ULONG sent = 0;
	ULONG recv = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_gain, sizeof(set_gain));
	recv = ReadBufferFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, response, sizeof(response));
	if (recv == 8) {
		*gain = bcdToDec(response[5]);
		SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetAttenuator Gain = %d", *gain);
		return true;
	}
	SoapySDR_logf(SOAPY_SDR_DEBUG, "ICR8600GetAttenuator Invalid Command");
	return false;
}
