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
#include <SoapySDR/Registry.hpp>
#include "WinUSBDevice.h"

static std::vector<SoapySDR::Kwargs> findICR(const SoapySDR::Kwargs &args)
{
	std::vector<SoapySDR::Kwargs> results;
	BOOL icr8600 = FindICR8600Device();
	if (icr8600) {
		SoapySDR::Kwargs devInfo;
		devInfo["label"] = "IC-R8600";
		devInfo["available"] = "Yes"; 
		devInfo["product"] = "IC-R8600";
		devInfo["serial"] = "-";
		devInfo["manufacturer"] = "Icom";
		results.push_back(devInfo);
	}
	SoapySDR_logf(SOAPY_SDR_DEBUG, "SoapyICR8600::findICR: %d", icr8600);

	return results;
}

static SoapySDR::Device *makeICR(const SoapySDR::Kwargs &args)
{
	return new SoapyICR8600(args);
}

static SoapySDR::Registry registerICR("icr8600", &findICR, &makeICR, SOAPY_SDR_ABI_VERSION);
