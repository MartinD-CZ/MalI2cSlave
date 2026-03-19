#include "i2c_registers.h"

#include "mal_salloc.h"
#include "mal_assert.h"
#include "mal_log.h"

#include <cstring>


void I2cSlaveRegisters::init(uint8_t myAddress, uint8_t numRegisters, uint_fast8_t irqPriority)
{
	m_i2cSlave.init(myAddress, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::addrMatchCallback>, this}, irqPriority);
	m_i2cSlave.setCallback(I2c::CallbackType::RX, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::rxCallback>, this});
	m_i2cSlave.setCallback(I2c::CallbackType::TXIS, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::txisCallback>, this});
	m_i2cSlave.setCallback(I2c::CallbackType::STOP, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::stopCallback>, this});

	m_registers = (uint8_t*)mal::salloc(numRegisters);
	memset(m_registers, 0x00, numRegisters);

	m_numRegisters = numRegisters;
	m_lastRxTxPos = 0;
}


size_t I2cSlaveRegisters::getNumRegisters() const
{
	return m_numRegisters;
}


bool I2cSlaveRegisters::setRegisters(uint8_t startAddress, const void* data, size_t numBytes)
{
	if ((startAddress + numBytes) > m_numRegisters)
		return false;

	memcpy(&m_registers[startAddress], data, numBytes);
	return true;
}


uint8_t I2cSlaveRegisters::getRegister(uint8_t address) const
{
	if (address >= m_numRegisters)
		return 0;

	return m_registers[address];
}


void I2cSlaveRegisters::logRegisters() const
{
	LOGI("[I2C REGS] Reg\tVal\n");
	for (uint8_t i = 0; i < m_numRegisters; i++)
		LOGI_NOINTRO("\t\t\t\t\t%02X\t%02X\n", i, m_registers[i]);
}


void I2cSlaveRegisters::addrMatchCallback()
{
	m_lastRxTxPos = 0;
	m_i2cSlave.flushTxRegister();
}


void I2cSlaveRegisters::rxCallback()
{
	const auto value = m_i2cSlave.read();
	
	if (m_lastRxTxPos == 0)
		m_currentAddress = value;
	else
	{
		const auto index = m_currentAddress + m_lastRxTxPos - 1;
		if (index < m_numRegisters)
			m_registers[index] = value;	
	}	

	m_lastRxTxPos = m_lastRxTxPos + 1;
}


void I2cSlaveRegisters::txisCallback()
{
	const auto index = m_currentAddress + m_lastRxTxPos;
	const auto value = (index < m_numRegisters) ? m_registers[index] : 0;
	m_i2cSlave.write(value);
	m_lastRxTxPos = m_lastRxTxPos + 1;
}