#include "i2c_registers.h"

#include "mal_salloc.h"
#include "mal_assert.h"
#include "mal_log.h"

#include <cstring>


/** Initializes the I2C slave and allocates the register map (all registers zeroed and writable by the master).
 *
 * @param myAddress		7-bit own address.
 * @param numRegisters	Size of the register map in bytes, at most MAX_REGISTERS.
 * @param irqPriority	Priority of the I2C interrupt.
 * @param timing		TIMINGR content passed to I2cSlave::init (sets the data setup/hold times).
 */
void I2cSlaveRegisters::init(uint8_t myAddress, uint8_t numRegisters, uint_fast8_t irqPriority, uint32_t timing)
{
	ASSERT(numRegisters <= MAX_REGISTERS);

	m_registers = (uint8_t*)mal::salloc(numRegisters);
	m_shadow = (uint8_t*)mal::salloc(numRegisters);
	memset(m_registers, 0x00, numRegisters);
	memset(m_shadow, 0x00, numRegisters);

	m_numRegisters = numRegisters;
	m_lastRxTxPos = 0;

	m_i2cSlave.init(myAddress, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::addrMatchCallback>, this}, irqPriority, timing);
	m_i2cSlave.setCallback(I2c::CallbackType::RX, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::rxCallback>, this});
	m_i2cSlave.setCallback(I2c::CallbackType::TXIS, Callback{&ClassCallbackHelper<I2cSlaveRegisters, &I2cSlaveRegisters::txisCallback>, this});
}


size_t I2cSlaveRegisters::getNumRegisters() const
{
	return m_numRegisters;
}


bool I2cSlaveRegisters::setRegisters(uint8_t startAddress, const void* data, size_t numBytes)
{
	if ((startAddress + numBytes) > m_numRegisters)
		return false;

	const auto pm = enterCritical();
	memcpy(&m_registers[startAddress], data, numBytes);
	exitCritical(pm);
	return true;
}


bool I2cSlaveRegisters::getRegisters(uint8_t startAddress, void* data, size_t numBytes) const
{
	if ((startAddress + numBytes) > m_numRegisters)
		return false;

	const auto pm = enterCritical();
	memcpy(data, &m_registers[startAddress], numBytes);
	exitCritical(pm);
	return true;
}


/** Atomically reads a single register and clears it to 0. Use for command registers written by the master, so that a
 * command arriving between a separate read and clear cannot be lost.
 *
 * @param address		Register address.
 * @return				Register value before clearing (0 for an invalid address).
 */
uint8_t I2cSlaveRegisters::fetchAndClear(uint8_t address)
{
	if (address >= m_numRegisters)
		return 0;

	const auto pm = enterCritical();
	const auto value = m_registers[address];
	m_registers[address] = 0;
	exitCritical(pm);
	return value;
}


/** Restricts master writes to the given range. The first call makes every other register read-only for the master;
 * further calls add more writable ranges. Local setRegisters() is not affected.
 *
 * @param startAddress	First writable register.
 * @param numBytes		Number of writable registers.
 */
void I2cSlaveRegisters::setWritable(uint8_t startAddress, uint8_t numBytes)
{
	ASSERT((startAddress + numBytes) <= m_numRegisters);

	uint32_t mask = (m_writableMask == UINT32_MAX) ? 0 : m_writableMask;
	for (uint8_t i = 0; i < numBytes; i++)
		mask |= 1UL << (startAddress + i);

	m_writableMask = mask;
}


/** Returns the number of I2C transactions addressed to this slave (wraps around). Compare two readouts to find out
 * whether the master is still talking to us.
 */
uint32_t I2cSlaveRegisters::getAccessCount() const
{
	return m_accessCount;
}


void I2cSlaveRegisters::logRegisters() const
{
	LOGI("[I2C REGS] Register contents\n\tReg\tVal\n");
	for (uint8_t i = 0; i < m_numRegisters; i++)
		LOGI_NOINTRO("\t%02X\t%02X\n", i, m_registers[i]);
}


void I2cSlaveRegisters::addrMatchCallback()
{
	m_lastRxTxPos = 0;
	m_accessCount = m_accessCount + 1;
	m_i2cSlave.flushTxRegister();

	if (m_i2cSlave.isReadRequest())		//runs in the ISR, so the copy is consistent with any setRegisters() call
		memcpy(m_shadow, m_registers, m_numRegisters);
}


void I2cSlaveRegisters::rxCallback()
{
	const auto value = m_i2cSlave.read();
	
	if (m_lastRxTxPos == 0)
		m_currentAddress = value;
	else
	{
		const auto index = m_currentAddress + m_lastRxTxPos - 1;
		if ((index < m_numRegisters) && (m_writableMask & (1UL << index)))
			m_registers[index] = value;	
	}	

	m_lastRxTxPos = m_lastRxTxPos + 1;
}


void I2cSlaveRegisters::txisCallback()
{
	const auto index = m_currentAddress + m_lastRxTxPos;
	const auto value = (index < m_numRegisters) ? m_shadow[index] : 0;
	m_i2cSlave.write(value);
	m_lastRxTxPos = m_lastRxTxPos + 1;
}