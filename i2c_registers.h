#pragma once


#include "mal_i2c.h"

#include "etl/utility.h"

#include <cstdint>
#include <cstddef>


/** Byte-addressed register map served over I2C from the slave ISR.
 *
 * Protocol: a write transaction starts with the register address, followed by data bytes written to consecutive
 * registers. A read transaction returns consecutive registers starting at the last written address. Read transactions
 * are served from a snapshot taken at address match, so a multi-byte value is never torn by a concurrent setRegisters().
 */
class I2cSlaveRegisters
{
public:
	I2cSlaveRegisters(I2cSlave& i2cSlave): m_i2cSlave(i2cSlave) {};

	void init(uint8_t myAddress, uint8_t numRegisters, uint_fast8_t irqPriority = LOWEST_IRQ_PRIORITY, uint32_t timing = 0);

	size_t getNumRegisters() const;
	bool setRegisters(uint8_t startAddress, const void* data, size_t numBytes);
	bool getRegisters(uint8_t startAddress, void* data, size_t numBytes) const;

	template <typename T>
	bool setRegister(uint8_t address, T value)
	{
		return setRegisters(address, &value, sizeof(value));
	}

	template <typename T>
	etl::pair<T, bool> getRegister(uint8_t address) const
	{
		T value = 0;
		const auto rv = getRegisters(address, &value, sizeof(T));
		return {value, rv};
	}

	uint8_t fetchAndClear(uint8_t address);

	void setWritable(uint8_t startAddress, uint8_t numBytes);
	uint32_t getAccessCount() const;

	void logRegisters() const;

	static constexpr uint8_t MAX_REGISTERS = 32;		//limited by the writable mask

private:
	void addrMatchCallback();
	void rxCallback();
	void txisCallback();

	I2cSlave& m_i2cSlave;
	
	uint8_t* m_registers;
	uint8_t* m_shadow;				//snapshot of m_registers for the ongoing read transaction
	uint8_t m_numRegisters;
	uint32_t m_writableMask = UINT32_MAX;

	volatile uint8_t m_currentAddress = 0;
	volatile uint8_t m_lastRxTxPos = 0;
	volatile uint32_t m_accessCount = 0;
};