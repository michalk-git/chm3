#pragma once


#include "system.h"

void WatchDogFunction(QP::QEQueue* msg_queue);

class WatchDog {
	int counter;
public:
	WatchDog() : counter(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC) {};
	void kick() {
		counter = Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC;
	}
	void SetResetInterval(unsigned int interval) {
		Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC = interval;
	}
	void Decrement(uint8_t amount) {
		counter -= amount;
	}
	int GetCounter()const {
		return counter;
	}
};

