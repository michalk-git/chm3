
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"

#include <stdio.h>
#include <stdlib.h>
#include "watchdog.h"
#include <map>
#include <string>
#include <iostream>
Q_DEFINE_THIS_FILE

//****************************************************************************
namespace Core_Health {

// Local objects -------------------------------------------------------------
static uint32_t l_rnd; // random seed

#ifdef Q_SPY
    enum {
        PHILO_STAT = QP::QS_USER
    };
    static uint8_t const l_clock_tick = 0U;
#endif

//............................................................................
void BSP::init(int argc, char **argv) {
    printf("Press s to subscribe\n"
  		   "Press u to unsubscribe\n"
           "Press ESC to quit...\n" );

    BSP::randomSeed(1234U);

    (void)argc;
    (void)argv;

}
//............................................................................
void BSP::terminate(int16_t result) {
    (void)result;
    QP::QF::stop();
}

//............................................................................
void BSP::displayPaused(uint8_t paused) {
    printf("Paused is %s\n", paused ? "ON" : "OFF");
}
//............................................................................
uint32_t BSP::random(void) { // a very cheap pseudo-random-number generator
    // "Super-Duper" Linear Congruential Generator (LCG)
    // LCG(2^32, 3*7*11*13*23, 0, seed)
    //
    l_rnd = l_rnd * (3U*7U*11U*13U*23U);
    return l_rnd >> 8;
}
//............................................................................
void BSP::randomSeed(uint32_t seed) {
    l_rnd = seed;
}

} // namespace Core_Health


//****************************************************************************
namespace QP {

//............................................................................
void QF::onStartup(void) { // QS startup callback
    QF_consoleSetup();
    QF_setTickRate(Core_Health::BSP::TICKS_PER_SEC, 30); // desired tick rate/prio
}
//............................................................................
void QF::onCleanup(void) {  // cleanup callback
    printf("\nBye! Bye!\n");
    QF_consoleCleanup();
}


typedef enum {
	IDLE,
	SUBSCRIBE,
	UNSUBSCRIBE,
	DEACTIVATE,
	DEACTIVATE_GET_MISSES
}Request_t;

enum {
	NO_KEY = 0,
	ENTER_KEY = 13
};



void SubscribeHandler(int user_id) {

	//create an event with the new user's id and post it to the CHM system
	RegisterNewUserEvt* user_evt = Q_NEW(RegisterNewUserEvt, SUBSCRIBE_SIG);
	user_evt->id = user_id;                                          
	AO_HealthMonitor->postFIFO(user_evt);
	return;
}

void UnSubscribeHandler(int index) {
	 AO_Member[index]->postFIFO(Q_NEW(QEvt, UNSUBSCRIBE_SIG));
	 return;
}



void DeactivateHandler(int index, int misses) {
	// create new event to notify the appropriate member and post it
	DeactivationEvt* evt = Q_NEW(DeactivationEvt, DEACTIVATE_SIG);
	evt->memberNum = index;
	evt->period_num = misses;
	AO_Member[index]->postFIFO(evt);

	printf("Member %d is deactivated for %d cycles\n ", index,  misses);
}

bool GetInput(int& number) {
	static bool valid_input = false;
	// check if the user has entered a new key yet; if not we need to wait
	int ch = QF_consoleGetKey();

	// is_num variable checks whether the 'ch' character is a number
	bool is_num = (((ch - '0') >= 0) && ((ch - '0') <= 9));

	// if the user didn't enter a key nothing will be done
	if ((ch == NO_KEY) ) return false;

	// if the user entered a number, we need to add it (as a digit) to the number entered up to this point 
	else if (is_num) {
		valid_input = true;
		printf("%d", (ch - '0'));
		number *= 10;
		number += (ch - '0');
		return false;
		
	}

	// if the user entered 'ENTER' we will return true - to signify the end of the input. But only if the input is valid; ie the user has entered a number previously
	else if (ch == ENTER_KEY) {
		if (!valid_input) printf("No number has been entered...try again\n");
		else {
			valid_input = false;
			return true;
		}
	}
	return false;
}

//............................................................................
	void QF_onClockTick(void) {

    static Request_t          request = IDLE;
    int                       ch;
    bool                      is_num = false;
    static int                number = 0;
    static std::map<int, int> id_to_index_dict;
    static int                index = 0;
    bool                      entry_ready = 0;

		QF::TICK_X(0U, &Core_Health::l_clock_tick); // process time events at rate 0
		QS_RX_INPUT(); // handle the QS-RX input
		QS_OUTPUT();   // handle the QS output

		//check if we're in the middle of a request (ie we need to receive more input to complete request)
		if (request != IDLE) {
			//First get the user's input on character at a time
			entry_ready = GetInput(number);
			//the subroutine will return if the entry isn't ready yet
			if (entry_ready == false) return;

			// Reaching this line means we finished receiving the user's input and need to process it according to the current request
			if (request == SUBSCRIBE) {
				//add new user
				SubscribeHandler(number);
				request = IDLE;
			}
			
			else if (request == UNSUBSCRIBE) {
				//unsubscribe user
				UnSubscribeHandler(number);
				request = IDLE;
			}
			else if (request == DEACTIVATE) {
				//used for testing
				printf("Enter number of periods to miss: ");
				request = DEACTIVATE_GET_MISSES;
				index = number;
			}
			else if (request == DEACTIVATE_GET_MISSES) {
				//used for testing
				DeactivateHandler(index,number);
				request = IDLE;
				index = 0;
			}
			number = 0;
			
		}
		//if we're not in the middle of a request we need to check if the user will enter a new request
		else {
		
			switch (QF_consoleGetKey()) {
			case '\33': { // ESC pressed?
				Core_Health::BSP::terminate(0);
				break;
			}
			case 's': {
				printf("s\n");
				request = SUBSCRIBE;
				printf("Enter id\n");
				break;
			}

			case 'u': {
				printf("u\n");
				request = UNSUBSCRIBE;
				printf("Enter system id\n");
				break;
			}
			case 'd': {
				printf("d\n");
				request = DEACTIVATE;
				printf("Enter system id\n");
				break;
			}
			default: {
				request = IDLE;
				break;
			}

			}

		}
	}

//----------------------------------------------------------------------------
#ifdef Q_SPY

//............................................................................
//! callback function to execute a user command (to be implemented in BSP)
void QS::onCommand(uint8_t cmdId,
                   uint32_t param1, uint32_t param2, uint32_t param3)
{
    switch (cmdId) {
       case 0U: {
           break;
       }
       default:
           break;
    }

    // unused parameters
    (void)param1;
    (void)param2;
    (void)param3;
}

#endif // Q_SPY
//----------------------------------------------------------------------------

} // namespace QP

//............................................................................
extern "C" void Q_onAssert(char const * const module, int_t loc) {
    QS_ASSERTION(module, loc, (uint32_t)10000U); // report assertion to QS
    fprintf(stderr, "Assertion failed in %s:%d", module, loc);
    QP::QF::onCleanup();
    exit(-1);
}

