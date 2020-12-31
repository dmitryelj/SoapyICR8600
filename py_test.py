from __future__ import print_function

import SoapySDR
from SoapySDR import * #SOAPY_SDR_ constants
import numpy #use numpy for buffers
import time
import ctypes

# To test add path: set SOAPY_SDR_PLUGIN_PATH=C:\Users\DMITRII\SoapyICR8600\build\x64\Release;

print()

#enumerate devices
print('Enumerating SoapySDR Devices')
results = SoapySDR.Device.enumerate()
for result in results:
	print(result)
print('----------')
print()

#create device instance
#args can be user defined or from the enumeration result
print('Instantiating ICR8600')
args = dict(driver="icr8600")
sdr = SoapySDR.Device(args)
print('----------')
print()

print('Get Frequency Range')
freqs = sdr.getFrequencyRange(SOAPY_SDR_RX, 0)
for freqRange in freqs:
	print(freqRange)
print('----------')
print()

# set the frequency to the HF band
sdr.setSampleRate(SOAPY_SDR_RX, 0, 240000)
sdr.setFrequency(SOAPY_SDR_RX, 0, 10000000)

# antennas 
antennas = sdr.listAntennas(SOAPY_SDR_RX, 0)
print("Antennas:", antennas)
for antenna in antennas:
	sdr.setAntenna(SOAPY_SDR_RX, 0, antenna)
	print("\tAntenna Readback = " + sdr.getAntenna(SOAPY_SDR_RX, 0))

# set outside the HF band
sdr.setFrequency(SOAPY_SDR_RX, 0, 100000000)
# antennas 
antennas = sdr.listAntennas(SOAPY_SDR_RX, 0)
print("Antennas:", antennas)
for antenna in antennas:
	sdr.setAntenna(SOAPY_SDR_RX, 0, antenna)
	print("\tAntenna Readback = " + sdr.getAntenna(SOAPY_SDR_RX, 0))

# exercise gains
gains = sdr.listGains(SOAPY_SDR_RX, 0)
print("Gains:", gains)
for gain in gains:
	print("\t" + gain + " (min, max, step): " + str(sdr.getGainRange(SOAPY_SDR_RX, 0, gain)))

sdr.setGain(SOAPY_SDR_RX, 0, "PRE-AMP", 0.0)
print("PRE-AMP Gain Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "PRE-AMP")))
sdr.setGain(SOAPY_SDR_RX, 0, "PRE-AMP", 14.0)
print("PRE-AMP Gain Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "PRE-AMP")))

sdr.setGain(SOAPY_SDR_RX, 0, "ATTENUATOR", 0.0)
print("Attenuator Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "ATTENUATOR")))
sdr.setGain(SOAPY_SDR_RX, 0, "ATTENUATOR", -10.0)
print("Attenuator Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "ATTENUATOR")))
sdr.setGain(SOAPY_SDR_RX, 0, "ATTENUATOR", -20.0)
print("Attenuator Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "ATTENUATOR")))
sdr.setGain(SOAPY_SDR_RX, 0, "ATTENUATOR", -30.0)
print("Attenuator Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "ATTENUATOR")))

sdr.setGain(SOAPY_SDR_RX, 0, "RF", 0.0)
print("RF Gain Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "RF")))
sdr.setGain(SOAPY_SDR_RX, 0, "RF", -32.0)
print("RF Gain Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "RF")))
sdr.setGain(SOAPY_SDR_RX, 0, "RF", -63.75)
print("RF Gain Readback = " + str(sdr.getGain(SOAPY_SDR_RX, 0, "RF")))

#setup a stream (complex floats)
rxStream = sdr.setupStream(SOAPY_SDR_RX, "CS16")
sdr.activateStream(rxStream) #start streaming

time.sleep(2.0)

#create a re-usable buffer for rx samples
buff = numpy.array([0]*1024, numpy.complex64)

#receive some samples
for i in range(20):
    sr = sdr.readStream(rxStream, [buff], len(buff))
    print("Rec:", sr.ret, buff[:4]) #num samples or error code
    # print(sr.flags) #flags set by receive operation
    # print(sr.timeNs) #timestamp for receive buffer

#shutdown the stream
sdr.deactivateStream(rxStream) #stop streaming
sdr.closeStream(rxStream)
