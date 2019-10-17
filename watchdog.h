#pragma once

#include <thread>
#include "system.h"
#include "singleton.h"
#include <mutex>
#include <chrono>


class WatchDog :public singleton<WatchDog>{
	std::chrono::duration<int> counter;
	std::thread watchdog;
	static void WatchDogFunction();
	std::mutex mtx;
	WatchDog() : counter(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC) {};
	friend singleton<WatchDog>;
public:
	
	void start() {
		watchdog = std::thread(&WatchDogFunction);
	}
	
	void stop() {

	}
	void kick() {
		mtx.lock();
		counter = std::chrono::duration<int>(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC);
		mtx.unlock();
	}
	void SetResetInterval(unsigned int interval) {
		Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC = interval;
	}
	void Decrement(std::chrono::duration<int> amount) {
		mtx.lock();
		counter -= (amount);
		mtx.unlock();
	}
	std::chrono::duration<int> GetCounter()const {
		return counter;
	}
};

