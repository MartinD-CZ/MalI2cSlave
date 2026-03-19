#include "i2c_slave.h"

#include "mal_salloc.h"
#include "mal_assert.h"
#include "mal_log.h"

#include <cstring>


void I2cSlaveRegisters::init(uint8_t myAddress, size_t numRegisters, uint_fast8_t irqPriority)
{
	m_i2cSlave.init(myAddress, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::addrMatchCallback>, this}, irqPriority);
	m_i2cSlave.setCallback(I2c::CallbackType::RX, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::rxCallback>, this});
	m_i2cSlave.setCallback(I2c::CallbackType::STOP, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::stopCallback>, this});
	m_registers = (RegEntry*)mal::salloc(numRegisters * sizeof(RegEntry));
	m_numRegisters = 0;
	m_maxRegisters = numRegisters;
}


void I2cSlaveRegisters::process()
{
	/*if (m_state == State::STOP)
	{
		m_state = State::IDLE;
		LOGI("[I2C REG] RX finished, data: %02X %02X %02X %02X %02X", m_lastRx[0], m_lastRx[1], m_lastRx[2], m_lastRx[3], m_lastRx[4]);
	}*/
}


void I2cSlaveRegisters::addRegister(uint8_t address, size_t sizeBytes)
{
	ASSERT(m_numRegisters < m_maxRegisters);
	m_registers[m_numRegisters].regAddress = address;
	m_registers[m_numRegisters].dataPtr = (uint8_t*)mal::salloc(sizeBytes);
	m_registers[m_numRegisters].sizeBytes = sizeBytes;
	m_numRegisters++;
}


bool I2cSlaveRegisters::setRegister(uint8_t address, const void* data, size_t numbytes)
{
	auto const reg = getRegisterPtr(address);
	if (reg == nullptr || numbytes > reg->sizeBytes)
		return false;

	memcpy(reg->dataPtr, data, numbytes);
	return true;
}


size_t I2cSlaveRegisters::getRegister(uint8_t address, void* data, size_t maxBytes) const
{
	auto const reg = getRegisterPtr(address);
	if (reg == nullptr || maxBytes < reg->sizeBytes)
		return 0;

	memcpy(data, reg->dataPtr, reg->sizeBytes);
	return reg->sizeBytes;
}


void I2cSlaveRegisters::addrMatchCallback()
{
	m_lastRxPos = 0;
	for (auto& rx: m_lastRx)
		rx = 0;
	m_state = State::ADDR_MATCHED;
}


void I2cSlaveRegisters::rxCallback()
{
	m_state = State::RX;
	if (m_lastRxPos < sizeof(m_lastRx))
		m_lastRx[m_lastRxPos++] = m_i2cSlave.read();
}


void I2cSlaveRegisters::stopCallback()
{
	m_state = State::STOP;
	if (!m_i2cSlave.isReadRequest() && m_lastRxPos > 1)		//if this is a write request and at least 2 bytes were received (register address + at least 1 byte of data)
	{
		const auto reg = getRegisterPtr(m_lastRx[0]);
		if ((reg != nullptr) && (reg->sizeBytes == m_lastRxPos - 1))		//if the register exists and the number of received data bytes matches the register size
			for (size_t i = 0; i < reg->sizeBytes; i++)			//we can't use memcpy here
				reg->dataPtr[i] = m_lastRx[i + 1];
	}
}


I2cSlaveRegisters::RegEntry* I2cSlaveRegisters::getRegisterPtr(uint8_t address) const
{
	for (size_t i = 0; i < m_numRegisters; i++)
		if (m_registers[i].regAddress == address)
			return &m_registers[i];
		
	return nullptr;
}