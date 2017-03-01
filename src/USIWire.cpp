/*
  USIWire.cpp - USI based TWI/I2C library for Arduino
  Copyright (c) 2017 Puuu.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3.0 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Based on TwoWire form Arduino https://github.com/arduino/Arduino.
*/

extern "C" {
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "USI_TWI_Master/USI_TWI_Master.h"
}

#include "USIWire.h"

// Initialize Class Variables //////////////////////////////////////////////////

uint8_t USIWire::rxBuffer[BUFFER_LENGTH];
uint8_t USIWire::rxBufferIndex = 0;
uint8_t USIWire::rxBufferLength = 0;

uint8_t USIWire::txBuffer[BUFFER_LENGTH];
uint8_t USIWire::txBufferIndex = 0;
uint8_t USIWire::txBufferLength = 0;

uint8_t USIWire::transmitting = 0;

// Constructors ////////////////////////////////////////////////////////////////

USIWire::USIWire() {
}

// Public Methods //////////////////////////////////////////////////////////////

void USIWire::begin(void) {
  rxBufferIndex = 0;
  rxBufferLength = 0;

  txBufferIndex = 0;
  txBufferLength = 0;

  transmitting = 0;

  USI_TWI_Master_Initialise();
}

void USIWire::begin(uint8_t address) {
  //twi_setAddress(address);
  //twi_attachSlaveTxEvent(onRequestService);
  //twi_attachSlaveRxEvent(onReceiveService);
  begin();
}

void USIWire::begin(int address) {
  begin((uint8_t)address);
}

void USIWire::end(void) {
  //twi_disable();
}

void USIWire::setClock(uint32_t clock) {
  // XXX: to be implemented.
  (void)clock; //disable warning
}

uint8_t USIWire::requestFrom(uint8_t address, uint8_t quantity,
                             uint32_t iaddress, uint8_t isize,
                             uint8_t sendStop) {
  if (isize > 0) {
    // send internal address; this mode allows sending a repeated
    // start to access some devices' internal registers. This function
    // is executed by the hardware TWI module on other processors (for
    // example Due's TWI_IADR and TWI_MMR registers)

    beginTransmission(address);

    // the maximum size of internal address is 3 bytes
    if (isize > 3) {
      isize = 3;
    }

    // write internal register address - most significant byte first
    while (isize-- > 0) {
      write((uint8_t)(iaddress >> (isize*8)));
    }
    endTransmission(false);
  }

  // reserve one byte for slave address
  quantity++;
  // clamp to buffer length
  if (quantity > BUFFER_LENGTH) {
    quantity = BUFFER_LENGTH;
  }
  // set address of targeted slave and read mode
  rxBuffer[0] = (address << TWI_ADR_BITS) | (1 << TWI_READ_BIT);
  // perform blocking read into buffer
  uint8_t ret = USI_TWI_Start_Transceiver_With_Data_Stop(rxBuffer, quantity,
                                                         sendStop);
  // set rx buffer iterator vars
  rxBufferIndex = 1; // ignore slave address
  // check for error
  if (ret == FALSE) {
    rxBufferLength = rxBufferIndex;
    return 0;
  }
  rxBufferLength = quantity;

  return quantity - 1; // ignore slave address
}

uint8_t USIWire::requestFrom(uint8_t address, uint8_t quantity,
                             uint8_t sendStop) {
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint32_t)0,
                     (uint8_t)0, (uint8_t)sendStop);
}

uint8_t USIWire::requestFrom(uint8_t address, uint8_t quantity) {
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t USIWire::requestFrom(int address, int quantity) {
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)true);
}

uint8_t USIWire::requestFrom(int address, int quantity, int sendStop) {
  return requestFrom((uint8_t)address, (uint8_t)quantity, (uint8_t)sendStop);
}

void USIWire::beginTransmission(uint8_t address) {
  // indicate that we are transmitting
  transmitting = 1;
  // set address of targeted slave and write mode
  txBuffer[0] = (address << TWI_ADR_BITS) | (0 << TWI_READ_BIT);
  // reset tx buffer iterator vars
  txBufferIndex = 1; // reserved by slave address
  txBufferLength = txBufferIndex;
}

void USIWire::beginTransmission(int address) {
  beginTransmission((uint8_t)address);
}

uint8_t USIWire::endTransmission(uint8_t sendStop) {
  // transmit buffer (blocking)
  uint8_t ret = USI_TWI_Start_Transceiver_With_Data_Stop(txBuffer,
                                                         txBufferLength,
                                                         sendStop);
  // reset tx buffer iterator vars
  txBufferIndex = 0;
  txBufferLength = 0;
  // indicate that we are done transmitting
  transmitting = 0;
  // check for error
  if (ret == FALSE) {
    switch (USI_TWI_Get_State_Info()) {
    case USI_TWI_DATA_OUT_OF_BOUND:
      return 1; //data too long to fit in transmit buffer
    case USI_TWI_NO_ACK_ON_ADDRESS:
      return 2; //received NACK on transmit of address
    case USI_TWI_NO_ACK_ON_DATA:
      return 3; //received NACK on transmit of data
    }
    return 4; //other error
  }
  return 0; //success
}

uint8_t USIWire::endTransmission(void) {
  return endTransmission(true);
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
size_t USIWire::write(uint8_t data) {
  if (transmitting) { // in master transmitter mode
    // don't bother if buffer is full
    if (txBufferLength >= BUFFER_LENGTH) {
      setWriteError();
      return 0;
    }
    // put byte in tx buffer
    txBuffer[txBufferIndex] = data;
    ++txBufferIndex;
    // update amount in buffer
    txBufferLength = txBufferIndex;
  } else { // in slave send mode
    // reply to master
    //twi_transmit(&data, 1);
  }
  return 1;
}

// must be called in:
// slave tx event callback
// or after beginTransmission(address)
size_t USIWire::write(const uint8_t *data, size_t quantity) {
  size_t numBytes = 0;
  for (size_t i = 0; i < quantity; ++i){
    numBytes += write(data[i]);
  }
  return numBytes;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int USIWire::available(void) {
  return rxBufferLength - rxBufferIndex;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int USIWire::read(void) {
  int value = -1;

  // get each successive byte on each call
  if (rxBufferIndex < rxBufferLength) {
    value = rxBuffer[rxBufferIndex];
    ++rxBufferIndex;
  }

  return value;
}

// must be called in:
// slave rx event callback
// or after requestFrom(address, numBytes)
int USIWire::peek(void) {
  int value = -1;

  if (rxBufferIndex < rxBufferLength) {
    value = rxBuffer[rxBufferIndex];
  }

  return value;
}

void USIWire::flush(void) {
  // XXX: to be implemented.
}

// sets function called on slave write
void USIWire::onReceive( void (*function)(int) ) {
  //user_onReceive = function;
}

// sets function called on slave read
void USIWire::onRequest( void (*function)(void) ) {
  //user_onRequest = function;
}

// Preinstantiate Objects //////////////////////////////////////////////////////

USIWire Wire = USIWire();
