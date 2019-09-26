#include "watchdog.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include "system.h"
void WatchDogFunction(QP::QEQueue* msg_queue) {

	WatchDog wd;
	int secs_in_dur = 0, last_wd = 0,curr_wd = 0;
	const QP::QEvt* event;
	std::chrono::duration<float, std::milli> dur;
	std::chrono::system_clock::time_point now;

	//find the start time of the thread and initialize the variable 'last_time'
	std::chrono::system_clock::time_point last_time = std::chrono::system_clock::now();

	while (1) {
		//find the current time : now
		now = std::chrono::system_clock::now();
        //find the difference in time (ie duration) between now and last time in millisecond time unit
		dur = now - last_time;
		//translate from type 'duration' to int (which represents the number of millisconds) and convert the result to seconds
		secs_in_dur = (dur.count() / 1000);
		//decrease the watchdog by the number of seconds calculated
		wd.Decrement(secs_in_dur);
		//advance the 'last_time' variable by the number of seconds calculated
		last_time += std::chrono::duration<int>(secs_in_dur);
        //check whether there are any events waiting in the watchdogs's message queue
		while (msg_queue->isEmpty() != true) {
			//if there is, get the event and kick the watchdog (the only event the watchdog can be sent is to signal "kick watchdog")
			event = (const QP::QEvt*)msg_queue->get();
			wd.kick();
			//wd.SetResetInterval(30);
			std::cout << "Watchdog has been reseted" << std::endl;
		}
		//make the thread go to sleep  for 500 msecs
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		curr_wd = wd.GetCounter();
		//if the watchdog reached 0, terminate the program
		if (curr_wd <= 0) {
			QP::QEvt* e = Q_NEW(QP::QEvt, Core_Health::TERMINATE_SIG);
			std::cout << "Terminating program" << std::endl;
			Core_Health::AO_CHM->postFIFO(e);
		}
		/*used for debugging:*****************************************************************************************************************! TO BE REMOVED!*/
		curr_wd = wd.GetCounter();
		if (curr_wd != last_wd) {
			std::cout << "wd = " << wd.GetCounter() << std::endl;
			last_wd = curr_wd;
		}
		/************************************************************************/
	}
}