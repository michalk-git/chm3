#pragma once
#ifndef WD_CPP_
#define WD_CPP_



#include <thread>
#include "system.h"
#include "singleton.h"
#include <mutex>
#include <chrono>



class WatchDog {
	std::chrono::duration<int> counter;
	std::thread watchdog_thread;
	static void WatchDogFunction();
	std::mutex mtx;
	WatchDog() : counter(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC),running(false) {};
	friend singleton<WatchDog>;
	bool running;
public:
	
	void start() {
		if (running == false) {
			running = true;
			counter = std::chrono::seconds(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC);
			watchdog_thread = std::thread(&WatchDogFunction);
		}
	}
	
	void stop() {
		if (running == true) {
			watchdog_thread.join();
			running = false;
		}
	}
	void kick() {
		std::lock_guard<std::mutex> lg(mtx);
		counter = std::chrono::duration<int>(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC);
	}
	void SetResetInterval(unsigned int interval) {
		Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC = interval;
	}
	void Decrement(std::chrono::duration<int> amount) {
		std::lock_guard<std::mutex> lg(mtx);
		counter -= (amount);

	}
	std::chrono::duration<int> GetCounter()const {
		return counter;
	}

	//singleton<WatchDog> watchdog;

};

#endif