
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
    printf("Press n to create account\n"
		   "Press s to subscribe\n"
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

int GetId() {
	std::string id;
	std::cout << "Enter ID" << std::endl;
	std::cin >> id;
	return stoi(id);
}


//............................................................................
void QF_onClockTick(void) {
	QF::TICK_X(0U, &Core_Health::l_clock_tick); // process time events at rate 0

	QS_RX_INPUT(); // handle the QS-RX input
	QS_OUTPUT();   // handle the QS output
	static std::map<unsigned int, unsigned int> id_to_index;                    //dictionary in which the key is the users id the value is the members array index
	int index = 0, misses = 0;
	static int num_users = 0;
	int id = -1;
	switch (QF_consoleGetKey()) {
	case '\33': { // ESC pressed?
		Core_Health::BSP::terminate(0);
		break;
	}
	case 'n': {	
		printf("n\n");
		id = GetId();
		//check if the user is already in the system
		if (id_to_index.find(id) != id_to_index.end()) {
			printf("User already exists\n");
			break;
		}
		// we need to check we havn't reached the maximum users allowed
		else if (num_users >= N_MEMBER) {
			printf("Reached maximum limit of users\n");
			break;
		}
		//else add the key-value pair of id-index to the id_to_index dictionary, increase the users count and print out the user's id
		else {
			printf("Your system id is %d\n",num_users);
			id_to_index[id] = num_users;
			num_users++;
			Core_Health::UserEvt* ae = Q_NEW(Core_Health::UserEvt, Core_Health::NEW_USER_SIG);
			ae->id = id;
			Core_Health::AO_CHM->postFIFO(ae);
			
		}
		
		break;
	}

	case 's': {

		printf("s\n");
		printf("num users:  %d: ", num_users);
		printf("Enter system id");
		index = QF_consoleWaitForKey() - '0';
		printf("%d\n", index);
		if (index >= num_users) {
			printf("User doesn't exist\n");
			break;
		}
		Core_Health::AO_Member[index]->postFIFO(Q_NEW(QEvt, Core_Health::SUBSCRIBE_SIG));
		break;
	}
	case 'u': {
		printf("u\n");
		printf("Enter system id");
		index = QF_consoleWaitForKey() - '0';
		printf("%d\n", index);
		if (index >= num_users ) {
			printf("User doesn't exist\n");
			break;
		}

		Core_Health::AO_Member[index]->postFIFO(Q_NEW(QEvt, Core_Health::UNSUBSCRIBE_SIG));
		break;
	}
	case 'm': {
		printf("m\n");
		printf("Enter system ID ");
		index = QF_consoleWaitForKey() - '0';
		printf("%d\n", index);
		if (index >= num_users) {
			printf("User doesn't exist\n");
			break;
		}
		printf("Enter number of periods to miss: ");
		misses = QF_consoleWaitForKey() - '0';
		printf("%d\n", misses);
		Core_Health::MalfunctionEvt* e = Q_NEW(Core_Health::MalfunctionEvt, Core_Health::MALFUNCTION_SIG);
		e->memberNum = index;
		e->period_num = misses;
		Core_Health::AO_Member[index]->postFIFO(e);
		break;
	}
	default: {
		break;
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

