#pragma once


#include "mal_i2c.h"

#include <cstdint>
#include <cstddef>


class I2cSlaveRegisters
{
public:
	enum State
	{
		IDLE,
		ADDR_MATCHED,
		RX,
		STOP
	};

	I2cSlaveRegisters(I2cSlave& i2cSlave): m_i2cSlave(i2cSlave) {};

	void init(uint8_t myAddress, size_t numRegisters, uint_fast8_t irqPriority = LOWEST_IRQ_PRIORITY);
	void process();

	void addRegister(uint8_t address, size_t sizeBytes);
	size_t getNumRegisters() const;
	bool setRegister(uint8_t address, const void* data, size_t numbytes);

	template <typename T>
	bool setRegister(uint8_t address, T value)
	{
		return setRegister(address, &value, sizeof(value));
	}

	size_t getRegister(uint8_t address, void* data, size_t maxBytes) const;

private:
	struct RegEntry
	{
		uint8_t regAddress;
		uint8_t* dataPtr;
		size_t sizeBytes;
	};	

	void addrMatchCallback();
	void rxCallback();
	void stopCallback();
	void txisCallback();

	RegEntry* getRegisterPtr(uint8_t address) const;

	I2cSlave& m_i2cSlave;
	
	RegEntry* m_registers;
	size_t m_numRegisters;
	size_t m_maxRegisters;

	volatile State m_state = State::IDLE;
	volatile uint8_t m_lastRx[5];
	volatile size_t m_lastRxTxPos;
};