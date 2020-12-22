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

#include "SoapyICR8600.hpp"
#include "WinUSBDevice.h"

SoapyICR8600::SoapyICR8600(const SoapySDR::Kwargs &args)
{
	SoapySDR_logf(SOAPY_SDR_DEBUG, "SoapyICR8600::SoapyICR8600");
	if (!FindICR8600Device()) {
		throw std::runtime_error("Icom ICR8600 not found.");
	}

	rxFormat = RX_FORMAT_INT16;

	sampleRate = 1920000;
	centerFrequency = 15000000;
	antennaIndex = 0;

	bufferLength = DEFAULT_BUFFER_LENGTH;
	bufferData = NULL;

	BOOL noDevice;
	HRESULT hr = OpenDevice(&deviceData, &noDevice);
	if (FAILED(hr)) {
		if (noDevice) {
			SoapySDR_logf(SOAPY_SDR_ERROR, "Error: device not connected or driver not installed");
		} else {
			SoapySDR_logf(SOAPY_SDR_ERROR, "Error: failed looking for device");
		}
		throw std::runtime_error("Icom ICR8600 not found or cannot be opened.");
	}

	BOOL bResult = GetDeviceDescriptor(deviceData.WinusbHandle, &deviceDesc);
	if (FALSE == bResult) {
		printf("GetDeviceDescriptor: failed\n");
		throw std::runtime_error("WinUsb_GetDescriptor failed");
	}

	// Print a few parts of the device descriptor
	SoapySDR_logf(SOAPY_SDR_INFO, "Device found: VID_%04X&PID_%04X; bcdUsb %04X\n", deviceDesc.idVendor, deviceDesc.idProduct, deviceDesc.bcdUSB);

	// Need to enable I/Q Mode or other commands will not work
	ICR8600SetRemoteOn(deviceData.WinusbHandle);

}

SoapyICR8600::~SoapyICR8600(void)
{
	// Exit I/Q Mode
	ICR8600SetRemoteOff(deviceData.WinusbHandle);

	CloseDevice(&deviceData);
	SoapySDR_logf(SOAPY_SDR_DEBUG, "SoapyICR8600::~SoapyICR8600");
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyICR8600::getDriverKey(void) const
{
	return "IC-R8600";
}

std::string SoapyICR8600::getHardwareKey(void) const
{
	return "IC-R8600";
}

SoapySDR::Kwargs SoapyICR8600::getHardwareInfo(void) const
{
	// Key/value pairs for any useful information
	// This also gets printed in --probe
	SoapySDR::Kwargs args;
	args["origin"] = "https://www.icom.co.jp/world/products/receiver/desktop/ic-r8600/";
	return args;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyICR8600::getNumChannels(const int dir) const
{
	return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyICR8600::listAntennas(const int direction, const size_t channel) const
{
	std::vector<std::string> antennas;
	antennas.push_back("ANT 1");
	antennas.push_back("ANT 2");
	antennas.push_back("ANT 3");
	return antennas;
}

void SoapyICR8600::setAntenna(const int direction, const size_t channel, const std::string &name)
{
	if (direction != SOAPY_SDR_RX)
	{
		throw std::runtime_error("setAntena failed: RTL-SDR only supports RX");
	}

	if (name == "ANT 1") {
		antennaIndex = 0;
		ICR8600SetAntenna(deviceData.WinusbHandle, 0);
	}
	else if (name == "ANT 2") {
		antennaIndex = 1;
		ICR8600SetAntenna(deviceData.WinusbHandle, 1);
	}
	else if (name == "ANT 3") {
		antennaIndex = 2;
		ICR8600SetAntenna(deviceData.WinusbHandle, 2);
	}
}

std::string SoapyICR8600::getAntenna(const int direction, const size_t channel) const
{
	std::vector<std::string> antennas = listAntennas(SOAPY_SDR_RX, 0);
	return antennas[antennaIndex];
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyICR8600::hasDCOffsetMode(const int direction, const size_t channel) const
{
	return false;
}

bool SoapyICR8600::hasFrequencyCorrection(const int direction, const size_t channel) const
{
	return false;
}

void SoapyICR8600::setFrequencyCorrection(const int direction, const size_t channel, const double value)
{
	// ppm = int(value);
	// rtlsdr_set_freq_correction(dev, ppm);
}

double SoapyICR8600::getFrequencyCorrection(const int direction, const size_t channel) const
{
	return double(0);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyICR8600::listGains(const int direction, const size_t channel) const
{
	//list available gain elements,
	//the functions below have a "name" parameter
	std::vector<std::string> gains;

	gains.push_back("RF");
	gains.push_back("PRE-AMP");
	gains.push_back("ATTENUATOR");

	return gains;
}

bool SoapyICR8600::hasGainMode(const int direction, const size_t channel) const
{
	// IC-R8600 disables AGC while in I/Q mode
	return false;
}

void SoapyICR8600::setGainMode(const int direction, const size_t channel, const bool automatic)
{
	// IC-R8600 disables AGC while in I/Q mode
}

bool SoapyICR8600::getGainMode(const int direction, const size_t channel) const
{
	// IC-R8600 disables AGC while in I/Q mode
	return false;
}

// removed for now
// may add back in later...
//
//void SoapyICR8600::setGain(const int direction, const size_t channel, const double value)
//{
//	//set the overall gain by distributing it across available gain elements
//	//OR delete this function to use SoapySDR's default gain distribution algorithm...
//	SoapySDR::Device::setGain(direction, channel, value);
//}

void SoapyICR8600::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
	//    SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain for stage %d: %f", stage, IFGain[stage - 1]);

	if (name == "RF")
	{
		ICR8600SetGainRF(deviceData.WinusbHandle, ULONG(int(value)));
	}
	else if (name == "PRE-AMP")
	{
		if (value > 0)
		{
			ICR8600SetPreAmpOn(deviceData.WinusbHandle);
		}
		else
		{
			ICR8600SetPreAmpOff(deviceData.WinusbHandle);			
		}
	}
	else if (name == "ATTENUATOR")
	{
		// give the attenuator a positive attenuation value
		ULONG atten = (ULONG)(int(-1.0 * value));
		ICR8600SetAttenuator(deviceData.WinusbHandle, atten);
	}
	else
	{
		// invalid gain name was given
		// do nothing, or throw an error
		// SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain for stage %d: %f", stage, IFGain[stage - 1]);
		// throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
	}
}

double SoapyICR8600::getGain(const int direction, const size_t channel, const std::string &name) const
{
	ULONG gain;
	if (name == "RF")
	{
		if (ICR8600GetGainRF(deviceData.WinusbHandle, &gain))
		{
			return (double)gain;
		}
		else
		{
			return 0;
		}
	}
	else if (name == "PRE-AMP")
	{
		BOOL on;

		if (ICR8600GetPreAmpState(deviceData.WinusbHandle, &on))
		{
			if (on)
				return(14.0);
			else
				return(0.0);
		}
		else
		{
			return 0;
		}
	}
	else if (name == "ATTENUATOR")
	{
		if (ICR8600GetAttenuator(deviceData.WinusbHandle, &gain))
		{
			if (gain != 0)
				// convert to negative
				return -1.0*(double)gain;
			else
				return 0.0;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		// invalid gain name was given
		// do nothing, or throw an error
		// SoapySDR_logf(SOAPY_SDR_DEBUG, "Setting RTL-SDR IF Gain for stage %d: %f", stage, IFGain[stage - 1]);
		// throw std::runtime_error("Invalid IF stage, 1 or 1-6 for E4000");
	}
	return 0;
}

// removed for now
// may add back in later...
//
//SoapySDR::Range SoapyICR8600::getGainRange(const int direction, const size_t channel) const {
//	return SoapySDR::Range(0.0, 32.0);
//}

SoapySDR::Range SoapyICR8600::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
	if (name == "RF")
	{
		return SoapySDR::Range(0.0, 255.0, 1.0);
	}
	else if (name == "PRE-AMP")
	{
		return SoapySDR::Range(0.0, 14.0, 14.0);
	}
	else if (name == "ATTENUATOR")
	{
		return SoapySDR::Range(-30.0, 0.0, 10.0);
	}
	else
	{
		// invalid gain name was given
		return SoapySDR::Range(0.0, 0.0);
	}
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyICR8600::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
	if (name == "RF")
	{
		centerFrequency = (ULONG)frequency;
		SoapySDR_logf(SOAPY_SDR_INFO, "Setting center freq: %d for %s", centerFrequency, name.c_str());
		ICR8600SetFrequency(deviceData.WinusbHandle, centerFrequency);
	} else if (name == "CORR")
	{
		// ppm = (int)frequency;
		// rtlsdr_set_freq_correction(dev, ppm);
	}
}

double SoapyICR8600::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
	if (name == "RF")
	{
		return (double)centerFrequency;
	}
	else if (name == "CORR")
	{
		return (double)0;
	}

	return 0;
}

std::vector<std::string> SoapyICR8600::listFrequencies(const int direction, const size_t channel) const
{
	std::vector<std::string> names;
	names.push_back("RF");
	// names.push_back("CORR");
	return names;
}

SoapySDR::RangeList SoapyICR8600::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
	SoapySDR::RangeList results;
	if (name == "RF")
	{
		results.push_back(SoapySDR::Range(10, 3000000000));
	}
	if (name == "CORR")
	{
		results.push_back(SoapySDR::Range(-1000, 1000));
	}
	return results;
}

SoapySDR::ArgInfoList SoapyICR8600::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList freqArgs;

	// TODO: frequency arguments

	return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyICR8600::setSampleRate(const int direction, const size_t channel, const double rate)
{
	sampleRate = (ULONG)rate;
	SoapySDR_logf(SOAPY_SDR_INFO, "Setting sample rate: %d", sampleRate);
	ICR8600SetSampleRate(deviceData.WinusbHandle, sampleRate);
}

double SoapyICR8600::getSampleRate(const int direction, const size_t channel) const
{
	return sampleRate;
}

std::vector<double> SoapyICR8600::listSampleRates(const int direction, const size_t channel) const
{
	std::vector<double> results;
	results.push_back(240000);
	results.push_back(480000);
	results.push_back(960000);
	results.push_back(1920000);
	results.push_back(3840000);
	results.push_back(5120000);
	return results;
}

void SoapyICR8600::setBandwidth(const int direction, const size_t channel, const double bw)
{
	SoapySDR::Device::setBandwidth(direction, channel, bw);
}

double SoapyICR8600::getBandwidth(const int direction, const size_t channel) const
{
	return SoapySDR::Device::getBandwidth(direction, channel);
}

std::vector<double> SoapyICR8600::listBandwidths(const int direction, const size_t channel) const
{
	std::vector<double> results;

	return results;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyICR8600::getSettingInfo(void) const
{
	SoapySDR::ArgInfoList setArgs;

	//SoapySDR::ArgInfo directSampArg;
	//directSampArg.key = "direct_samp";
	//directSampArg.value = "0";
	//directSampArg.name = "Direct Sampling";
	//directSampArg.description = "RTL-SDR Direct Sampling Mode";
	//directSampArg.type = SoapySDR::ArgInfo::STRING;
	//directSampArg.options.push_back("0");
	//directSampArg.optionNames.push_back("Off");
	//directSampArg.options.push_back("1");
	//directSampArg.optionNames.push_back("I-ADC");
	//directSampArg.options.push_back("2");
	//directSampArg.optionNames.push_back("Q-ADC");
	//setArgs.push_back(directSampArg);

	//SoapySDR::ArgInfo offsetTuneArg;
	//offsetTuneArg.key = "offset_tune";
	//offsetTuneArg.value = "false";
	//offsetTuneArg.name = "Settings - 1";
	//offsetTuneArg.description = "Settings - 1";
	//offsetTuneArg.type = SoapySDR::ArgInfo::BOOL;
	//setArgs.push_back(offsetTuneArg);

	//SoapySDR::ArgInfo iqSwapArg;
	//iqSwapArg.key = "iq_swap";
	//iqSwapArg.value = "false";
	//iqSwapArg.name = "I/Q Swap";
	//iqSwapArg.description = "RTL-SDR I/Q Swap Mode";
	//iqSwapArg.type = SoapySDR::ArgInfo::BOOL;
	//setArgs.push_back(iqSwapArg);

	//SoapySDR::ArgInfo digitalAGCArg;
	//digitalAGCArg.key = "digital_agc";
	//digitalAGCArg.value = "false";
	//digitalAGCArg.name = "Digital AGC";
	//digitalAGCArg.description = "RTL-SDR digital AGC Mode";
	//digitalAGCArg.type = SoapySDR::ArgInfo::BOOL;
	//setArgs.push_back(digitalAGCArg);

	SoapySDR_logf(SOAPY_SDR_INFO, "SETARGS?");

	return setArgs;
}

void SoapyICR8600::writeSetting(const std::string &key, const std::string &value)
{
	//if (key == "direct_samp")
	//{
	//    try
	//    {
	//        directSamplingMode = std::stoi(value);
	//    }
	//    catch (const std::invalid_argument &) {
	//        SoapySDR_logf(SOAPY_SDR_ERROR, "RTL-SDR invalid direct sampling mode '%s', [0:Off, 1:I-ADC, 2:Q-ADC]", value.c_str());
	//        directSamplingMode = 0;
	//    }
	//    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR direct sampling mode: %d", directSamplingMode);
	//    rtlsdr_set_direct_sampling(dev, directSamplingMode);
	//}
	//else if (key == "iq_swap")
	//{
	//    iqSwap = ((value=="true") ? true : false);
	//    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR I/Q swap: %s", iqSwap ? "true" : "false");
	//}
	//else if (key == "offset_tune")
	//{
	//    offsetMode = (value == "true") ? true : false;
	//    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR offset_tune mode: %s", offsetMode ? "true" : "false");
	//    rtlsdr_set_offset_tuning(dev, offsetMode ? 1 : 0);
	//}
	//else if (key == "digital_agc")
	//{
	//    digitalAGC = (value == "true") ? true : false;
	//    SoapySDR_logf(SOAPY_SDR_DEBUG, "RTL-SDR digital agc mode: %s", digitalAGC ? "true" : "false");
	//    rtlsdr_set_agc_mode(dev, digitalAGC ? 1 : 0);
	//}
}

std::string SoapyICR8600::readSetting(const std::string &key) const
{
	return "false";
	//if (key == "direct_samp") {
	//    return std::to_string(directSamplingMode);
	//if (key == "iq_swap") {
	//	return "false";
	//} else if (key == "offset_tune") {
	//	return "false";
	//} // else if (key == "digital_agc") {
	//    return digitalAGC?"true":"false";
	//}

	SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
	return "";
}


