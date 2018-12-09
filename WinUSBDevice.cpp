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

#pragma comment(lib, "winusb.lib") 
#pragma comment(lib, "Cfgmgr32.lib")

HRESULT RetrieveDevicePath(_Out_bytecap_(BufLen) LPTSTR DevicePath, _In_ ULONG  BufLen, _Out_opt_ PBOOL  FailureDeviceNotFound);

BOOL FindICR8600Device()
/*++
Routine description:
	Check if Icom IC-R8600 device is connected to the USB port.
--*/
{
	DEVICE_DATA deviceData;
	BOOL notFound = false;
	HRESULT hr = RetrieveDevicePath(deviceData.DevicePath, sizeof(deviceData.DevicePath), &notFound);
	if (FAILED(hr) || notFound) {
		return FALSE;
	}
	return TRUE;
}

HRESULT OpenDevice(_Out_ PDEVICE_DATA DeviceData, _Out_opt_ PBOOL FailureDeviceNotFound)
/*++
Routine description:
    Open all needed handles to interact with the device.
    If the device has multiple USB interfaces, this function grants access to
    only the first interface.
    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.

Arguments:
    DeviceData - Struct filled in by this function. The caller should use the
        WinusbHandle to interact with the device, and must pass the struct to
        CloseDevice when finished.
    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.
--*/
{
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
}

VOID CloseDevice(_Inout_ PDEVICE_DATA DeviceData)
/*++
Routine description:
    Perform required cleanup when the device is no longer needed.
    If OpenDevice failed, do nothing.

Arguments:
    DeviceData - Struct filled in by OpenDevice
--*/
{
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
}

HRESULT RetrieveDevicePath(_Out_bytecap_(BufLen) LPTSTR DevicePath, _In_ ULONG  BufLen, _Out_opt_ PBOOL  FailureDeviceNotFound)
/*++
Routine description:
    Retrieve the device path that can be used to open the WinUSB-based device.
    If multiple devices have the same device interface GUID, there is no
    guarantee of which one will be returned.
Arguments:
    DevicePath - On successful return, the path of the device (use with CreateFile).
    BufLen - The size of DevicePath's buffer, in bytes
    FailureDeviceNotFound - TRUE when failure is returned due to no devices
        found with the correct device interface (device not connected, driver
        not installed, or device is disabled in Device Manager); FALSE
        otherwise.
-*/
{
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
}

BOOL WriteToBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID, ULONG* pcbWritten, PUCHAR send, ULONG cbSize) {
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
}

BOOL ReadFromBulkEndpoint(WINUSB_INTERFACE_HANDLE hDeviceHandle, UCHAR ID, ULONG cbSize) {
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
}

void ICR8600SetRemoteOn(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	UCHAR remote_on_cmd[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x00, 0x01,  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, remote_on_cmd, sizeof(remote_on_cmd));
	ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	Sleep(100);
}

void ICR8600SetRemoteOff(WINUSB_INTERFACE_HANDLE hDeviceHandle)
{
	ULONG sent = 0;
	// UCHAR iq_stop_cmd[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x01, 0x00, 0x00, 0x06,  0xFD, 0xFF };
	// WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_stop_cmd, sizeof(iq_stop_cmd));
	// ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	// Sleep(100);

	UCHAR remote_off_cmd[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x1A, 0x13, 0x00, 0x00,  0xFD, 0xFF };
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, remote_off_cmd, sizeof(remote_off_cmd));
	ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	Sleep(100);
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
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	if (sampleRate == 480000) {
		printf("480000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_480_16bit, sizeof(iq_480_16bit));
		Sleep(100);
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	if (sampleRate == 960000) {
		printf("960000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_960_16bit, sizeof(iq_960_16bit));
		Sleep(100);
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	if (sampleRate == 1920000) {
		printf("1920000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_1920_16bit, sizeof(iq_1920_16bit));
		Sleep(100);
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	if (sampleRate == 3840000) {
		printf("3840000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_3840_16bit, sizeof(iq_3840_16bit));
		Sleep(100);
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	if (sampleRate == 5120000) {
		printf("5120000\n");
		WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, iq_5120_16bit, sizeof(iq_5120_16bit));
		Sleep(100);
		ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	}
	return TRUE;
}

BOOL ICR8600SetFrequency(WINUSB_INTERFACE_HANDLE hDeviceHandle, ULONG frequency)
{
	#define decToBcd(val) (UCHAR)((((val) / 10 * 16) + ((val) % 10)))

	ULONG f1 = frequency % 100, f2 = (frequency / 100) % 100, f3 = (frequency / 10000) % 100, f4 = (frequency / 1000000) % 100, f5 = (frequency / 100000000) % 100;
	UCHAR set_freq[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x05,  decToBcd(f1),decToBcd(f2),decToBcd(f3),decToBcd(f4),decToBcd(f5),  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_freq, sizeof(set_freq));
	ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);

	return TRUE;
}

BOOL ICR8600SetAntenna(WINUSB_INTERFACE_HANDLE hDeviceHandle, int antennaIndex)
{
	UCHAR set_ant[] = { 0xFE, 0xFE, 0x96, 0xE0,  0x12, antennaIndex,  0xFD, 0xFF };
	ULONG sent = 0;
	WriteToBulkEndpoint(hDeviceHandle, PIPE_CONTROL_ID, &sent, set_ant, sizeof(set_ant));
	ReadFromBulkEndpoint(hDeviceHandle, PIPE_RESPONSE_ID, 64);
	return TRUE;
}

ULONG ICR8600ReadPipe(WINUSB_INTERFACE_HANDLE hDeviceHandle, PUCHAR Buffer, ULONG BufferLength) 
{
	ULONG cbRead = 0;
	BOOL bResult = WinUsb_ReadPipe(hDeviceHandle, PIPE_IQ_ID, Buffer, BufferLength, &cbRead, 0);
	if (bResult) {
	}
	return cbRead;
}