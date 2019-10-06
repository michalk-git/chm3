#pragma once

#include <thread>
#include "system.h"
#include "singleton.h"
#include <mutex>



class WatchDog :public singleton<WatchDog>{
	int counter;
	std::thread watchdog;
	static void WatchDogFunction();
	std::mutex mtx;
	WatchDog() : counter(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC) {};
	friend singleton<WatchDog>;
public:
	
	void start() {
		watchdog = std::thread(&WatchDogFunction);
	}
	
	void kick() {
		mtx.lock();
		counter = Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC;
		mtx.unlock();
	}
	void SetResetInterval(unsigned int interval) {
		Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC = interval;
	}
	void Decrement(uint8_t amount) {
		mtx.lock();
		counter -= amount;
		mtx.unlock();
	}
	int GetCounter()const {
		return counter;
	}
};

