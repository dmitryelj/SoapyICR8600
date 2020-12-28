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

#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Types.h>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "WinUSBDevice.h"

typedef enum SDRRXFormat
{
	RX_FORMAT_FLOAT32, RX_FORMAT_INT16
} sdrRXFormat;

#define DEFAULT_BUFFER_LENGTH (4 * 1024)
#define BYTES_PER_SAMPLE 2

class SoapyICR8600 : public SoapySDR::Device
{
public:
	SoapyICR8600(const SoapySDR::Kwargs &args);

	~SoapyICR8600(void);

	/*******************************************************************
	 * Identification API
	 ******************************************************************/

	std::string getDriverKey(void) const;

	std::string getHardwareKey(void) const;

	SoapySDR::Kwargs getHardwareInfo(void) const;

	/*******************************************************************
	 * Channels API
	 ******************************************************************/

	size_t getNumChannels(const int) const;

	/*******************************************************************
	 * Stream API
	 ******************************************************************/

	std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

	std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;

	SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

	SoapySDR::Stream *setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels =
		std::vector<size_t>(), const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

	void closeStream(SoapySDR::Stream *stream);

	size_t getStreamMTU(SoapySDR::Stream *stream) const;

	int activateStream(
		SoapySDR::Stream *stream,
		const int flags = 0,
		const long long timeNs = 0,
		const size_t numElems = 0);

	int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0);

	int readStream(
		SoapySDR::Stream *stream,
		void * const *buffs,
		const size_t numElems,
		int &flags,
		long long &timeNs,
		const long timeoutUs = 100000);

	/*******************************************************************
	 * Direct buffer access API
	 ******************************************************************/

	size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

	int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

	int acquireReadBuffer(SoapySDR::Stream *stream, size_t &handle, const void **buffs, int &flags, long long &timeNs, const long timeoutUs = 100000);

	void releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle);

	/*******************************************************************
	 * Antenna API
	 ******************************************************************/

	std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

	void setAntenna(const int direction, const size_t channel, const std::string &name);

	std::string getAntenna(const int direction, const size_t channel) const;

	/*******************************************************************
	 * Frontend corrections API
	 ******************************************************************/

	bool hasDCOffsetMode(const int direction, const size_t channel) const;

	bool hasFrequencyCorrection(const int direction, const size_t channel) const;

	void setFrequencyCorrection(const int direction, const size_t channel, const double value);

	double getFrequencyCorrection(const int direction, const size_t channel) const;

	/*******************************************************************
	 * Gain API
	 ******************************************************************/

	std::vector<std::string> listGains(const int direction, const size_t channel) const;

	bool hasGainMode(const int direction, const size_t channel) const;

	void setGainMode(const int direction, const size_t channel, const bool automatic);

	bool getGainMode(const int direction, const size_t channel) const;

	void setGain(const int direction, const size_t channel, const double value);

	void setGain(const int direction, const size_t channel, const std::string &name, const double value);

	double getGain(const int direction, const size_t channel, const std::string &name) const;

	double getGain(const int direction, const size_t channel) const;

	SoapySDR::Range getGainRange(const int direction, const size_t channel) const;
	
	SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

	/*******************************************************************
	 * Frequency API
	 ******************************************************************/

	void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

	double getFrequency(const int direction, const size_t channel, const std::string &name) const;

	std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

	SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

	SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

	/*******************************************************************
	 * Sample Rate API
	 ******************************************************************/

	void setSampleRate(const int direction, const size_t channel, const double rate);

	double getSampleRate(const int direction, const size_t channel) const;

	std::vector<double> listSampleRates(const int direction, const size_t channel) const;

	void setBandwidth(const int direction, const size_t channel, const double bw);

	double getBandwidth(const int direction, const size_t channel) const;

	std::vector<double> listBandwidths(const int direction, const size_t channel) const;


	/*******************************************************************
	 * Settings API
	 ******************************************************************/

	SoapySDR::ArgInfoList getSettingInfo(void) const;

	void writeSetting(const std::string &key, const std::string &value);

	std::string readSetting(const std::string &key) const;

private:
	// WinUSB access
	DEVICE_DATA deviceData;
	USB_DEVICE_DESCRIPTOR deviceDesc;

	//cached settings
	sdrRXFormat rxFormat;
	ULONG sampleRate;
	ULONG centerFrequency;
	int antennaIndex;
	size_t bufferLength;
	unsigned char *bufferData;
};
