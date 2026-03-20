#pragma once


#include "mal_i2c.h"

#include "etl/utility.h"

#include <cstdint>
#include <cstddef>


class I2cSlaveRegisters
{
public:
	I2cSlaveRegisters(I2cSlave& i2cSlave): m_i2cSlave(i2cSlave) {};

	void init(uint8_t myAddress, uint8_t numRegisters, uint_fast8_t irqPriority = LOWEST_IRQ_PRIORITY);

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

	void logRegisters() const;

private:
	void addrMatchCallback();
	void rxCallback();
	void txisCallback();

	I2cSlave& m_i2cSlave;
	
	uint8_t* m_registers;
	uint8_t m_numRegisters;

	volatile uint8_t m_currentAddress = 0;
	volatile uint8_t m_lastRxTxPos = 0;
};