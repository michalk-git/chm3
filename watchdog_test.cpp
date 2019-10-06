//#include "qpcpp.h"
//#include "system.h"
//#include "bsp.h"
#include <iostream>
//#include <thread>
#include "watchdog.h"

int main() {
	WatchDog wd;
	QP::QEvt const* wdQueueSto[N_MEMBER];
	QP::QEQueue watch_dog_queue;
	watch_dog_queue.init(wdQueueSto, Q_DIM(wdQueueSto));
	WatchDogFunction(&watch_dog_queue,&wd);
	
	return 0;
}