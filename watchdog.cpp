#include "watchdog.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>
#include "system.h"

using namespace std;
using namespace std::chrono;

void WatchDog::WatchDogFunction() {

	WatchDog&                watchdog_instance = singleton<WatchDog>::getInstance();
	int                      last_counter_value = 0;
	int                      curr_counter_value = 0;
	duration<int>            time_diff;
	system_clock::time_point now;
	bool                     watchdog_reached_zero = false;
	
	//find the start time of the thread and initialize the variable 'last_time'
	system_clock::time_point last_time = system_clock::now();

	while (watchdog_instance.is_running) {

		//find the current time : now
		now = system_clock::now();

        //find the difference in time (ie duration) between now and last time in seconds time unit
		time_diff = duration_cast<seconds>(now - last_time);
		

		//decrease the watchdog by the number of seconds calculated
		watchdog_instance.DecrementCounter(time_diff);

		//check if the watchdog's counter reached zero; if so, terminate
		watchdog_reached_zero = (watchdog_instance.counter.count() <= 0);

		if (watchdog_reached_zero) {
			printf("WatchDog reached zero- terminating...\n");
			std::terminate();
		}

		//advance the 'last_time' variable by the number of seconds calculated
		last_time += time_diff;

		//make the thread go to sleep  for 500 msecs
		this_thread::sleep_for(milliseconds(500));

		/*used for debugging:*****************************************************************************************************************! TO BE REMOVED!*/
		curr_counter_value = watchdog_instance.GetCounterDurationInSecs().count();
		if (curr_counter_value != last_counter_value) {
			cout << "wd = " << curr_counter_value << endl;
			last_counter_value = curr_counter_value;
		}
		/************************************************************************/
	}
}