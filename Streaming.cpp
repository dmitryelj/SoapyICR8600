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
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>
#include <climits> 
#include <cstring> 

std::vector<std::string> SoapyICR8600::getStreamFormats(const int direction, const size_t channel) const {
	std::vector<std::string> formats;
	formats.push_back(SOAPY_SDR_CS16);
	formats.push_back(SOAPY_SDR_CF32);
	return formats;
}

std::string SoapyICR8600::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const {
	if (direction != SOAPY_SDR_RX) {
		throw std::runtime_error("IC-R8600 is RX only, use SOAPY_SDR_RX");
	}

	fullScale = 32767;
	return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList SoapyICR8600::getStreamArgsInfo(const int direction, const size_t channel) const {
	if (direction != SOAPY_SDR_RX) {
		throw std::runtime_error("IC-R8600 is RX only, use SOAPY_SDR_RX");
	}

	SoapySDR::ArgInfoList streamArgs;

	return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

// Not implemented

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDR::Stream *SoapyICR8600::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args) {
	if (direction != SOAPY_SDR_RX)
	{
		throw std::runtime_error("IC-R8600 is RX only, use SOAPY_SDR_RX");
	}

	//check the channel configuration
	if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0))
	{
		throw std::runtime_error("setupStream invalid channel selection");
	}

	//check the format
	if (format == SOAPY_SDR_CS16)
	{
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
		rxFormat = RX_FORMAT_INT16;
	} else if (format == SOAPY_SDR_CF32)
	{
		 SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
		 rxFormat = RX_FORMAT_FLOAT32;
	} else {
		throw std::runtime_error("setupStream invalid format '" + format + "' -- Only CS16 is supported by SoapyICR8600 module.");
	}

	bufferLength = DEFAULT_BUFFER_LENGTH;
	if (args.count("bufflen") != 0) {
		try
		{
			int bufferLength_in = std::stoi(args.at("bufflen"));
			if (bufferLength_in > 0) {
				bufferLength = bufferLength_in;
			}
		}
		catch (const std::invalid_argument &) {}
	}
	SoapySDR_logf(SOAPY_SDR_INFO, "SoapyICR8600::setupStream Using buffer length %d", bufferLength);

	bufferData = (unsigned char*)malloc(2 * bufferLength * sizeof(unsigned char));

	//Set parameters
	//SoapySDR_logf(SOAPY_SDR_INFO, "ICR8600SetFrequency: %d", centerFrequency);
	//ICR8600SetFrequency(deviceData.WinusbHandle, centerFrequency);
	//SoapySDR_logf(SOAPY_SDR_INFO, "ICR8600SetSampleRate: %d", sampleRate);
	//ICR8600SetSampleRate(deviceData.WinusbHandle, sampleRate);

	return (SoapySDR::Stream *) this;
}

void SoapyICR8600::closeStream(SoapySDR::Stream *stream) {
	SoapySDR_logf(SOAPY_SDR_INFO, "SoapyICR8600::closeStream");
	this->deactivateStream(stream, 0, 0);
	free(bufferData);
	bufferData = NULL;
}

size_t SoapyICR8600::getStreamMTU(SoapySDR::Stream *stream) const {
	return bufferLength / BYTES_PER_SAMPLE;
}

int SoapyICR8600::activateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs, const size_t numElems) {
	if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;


	return 0;
}

int SoapyICR8600::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs) {
	if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;

	return 0;
}

int SoapyICR8600::readStream(SoapySDR::Stream *stream, void * const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs) {
	std::unique_lock<std::mutex> lock(_buf_mutex);
	
	SoapySDR_logf(SOAPY_SDR_TRACE, "SoapyICR8600::readStream: %d, flags: %d", numElems, flags);

	ULONG cbRead = ICR8600ReadPipe(deviceData.WinusbHandle, bufferData, bufferLength);
	int16_t *source = (int16_t *)bufferData;
	UCHAR *s0 = (UCHAR*)bufferData;

	// The user's buffer for channel 0
	void *buff0 = buffs[0];
	int returnedElems = 0;
	if (rxFormat == RX_FORMAT_INT16) {
		int16_t *itarget = (int16_t *)buff0;
		for (ULONG p = 0; p < cbRead/4; p++) {
			UCHAR c1 = s0[4 * p], c2 = s0[4*p + 1], c3 = s0[4 * p + 2], c4 = s0[4 * p + 3];
			if (c1 != 0x00 || c2 != 0x80 || c3 != 0x00 || c4 != 0x80) {
				itarget[returnedElems++] = source[2*p];
				itarget[returnedElems++] = source[2*p + 1];
			}
			else {
				printf("%Xh", source[p]);
			}
		}
	}
	if (rxFormat == RX_FORMAT_FLOAT32) {
		float *ftarget = (float *)buff0;
		for (ULONG p = 0; p < cbRead / 4; p++) {
			UCHAR c1 = s0[4 * p], c2 = s0[4 * p + 1], c3 = s0[4 * p + 2], c4 = s0[4 * p + 3];
			if (c1 != 0x00 || c2 != 0x80 || c3 != 0x00 || c4 != 0x80) {
				ftarget[returnedElems++] = (float)(source[2*p]) / 32768;
				ftarget[returnedElems++] = (float)(source[2*p + 1]) / 32768;
			}
		}
	}

	return returnedElems/2;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyICR8600::getNumDirectAccessBuffers(SoapySDR::Stream *stream) {
	return 0; // _buffs.size();
}

int SoapyICR8600::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs) {
	//buffs[0] = (void *)bufferData;// _buffs[handle].data();
	return 0;
}

int SoapyICR8600::acquireReadBuffer(SoapySDR::Stream *stream, size_t &handle, const void **buffs, int &flags, long long &timeNs, const long timeoutUs) {
	SoapySDR_logf(SOAPY_SDR_INFO, "SoapyICR8600::acquireReadBuffer, flags: %d", flags);

	return 0; 
}

void SoapyICR8600::releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle) {
}
