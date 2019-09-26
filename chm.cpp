
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>
#include <thread>
#include "watchdog.h"
#include <chrono>

Q_DEFINE_THIS_FILE

using namespace Core_Health;


// Active object class -------------------------------------------------------
//$declare${AOs::CHM} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

	//${AOs::CHM} ..............................................................
	class CHM : public QP::QActive {
	public:
		static CHM inst;

	private:
		bool subscribers[N_MEMBER];
		bool AOs_alive[N_MEMBER+1];
		QP::QTimeEvt timeEvt_request_update;
		QP::QTimeEvt timeEvt_kick;
		std::thread watchdog;
		QP::QEvt const * wdQueueSto[N_MEMBER];

		
	public:
		CHM();
		QP::QEQueue watch_dog_queue;

	protected:
		Q_STATE_DECL(initial);
		Q_STATE_DECL(active);

	};


}
//$skip${QP_VERSION} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Check for the minimum required QP version
#if (QP_VERSION < 650U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpcpp version 6.5.0 or higher required
#endif
//$endskip${QP_VERSION} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//$define${AOs::AO_CHM} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

//${AOs::AO_CHM} ...........................................................
QP::QActive * const AO_CHM = &CHM::inst; // "opaque" pointer to CHM AO

} // namespace Core_Health
//$enddef${AOs::AO_CHM} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//$define${AOs::CHM} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

	//${AOs::CHM} ..............................................................
	CHM CHM::inst;
	//${AOs::CHM::CHM} .......................................................
	CHM::CHM()
		: QActive(&initial), timeEvt_request_update(this,UPDATE_SIG,0U) ,timeEvt_kick(this,KICK_SIG,0U)
	{
	
		watch_dog_queue.init(wdQueueSto, Q_DIM(wdQueueSto));
		watchdog = std::thread(WatchDogFunction, &watch_dog_queue);
	
		//index = N_MEMBER in AOs_alive array is for the CHM system to signal activity: initializing it to false meaning the CHM hasn't sent an ALIVE signal yet
		AOs_alive[N_MEMBER] = false;
		//initialize AOs_alive array to false and subscribers array to true (the starting default is that all users are subscribers and haven't sent an ALIVE signal yet
		for (int i = 0; i < N_MEMBER ; i++) {
			AOs_alive[i] = false;
			subscribers[i] = true;
		}
		
	};

	//${AOs::CHM::SM} ..........................................................
    //${AOs::CHM::SM::initial}
	Q_STATE_DEF(CHM, initial) {

		(void)e; // suppress the compiler warning about unused parameter

		//the system needs to subscribe to REQUEST_UPDATE_SIG to know when to send ALIVE signal
		subscribe(REQUEST_UPDATE_SIG);
		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC seconds
		timeEvt_request_update.armX(BSP::TICKS_PER_SEC*(CHMConfig_t::T_AO_ALIVE_SEC), BSP::TICKS_PER_SEC * (CHMConfig_t::T_AO_ALIVE_SEC));
		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC seconds
		timeEvt_kick.armX(BSP::TICKS_PER_SEC*(CHMConfig_t::T_UPDATE_WATCHDOG_SEC), BSP::TICKS_PER_SEC*(CHMConfig_t::T_UPDATE_WATCHDOG_SEC));

		return tran(&active);
	}
	//${AOs::CHM::SM::active} ..................................................
	Q_STATE_DEF(CHM, active) {

		
		QP::QState status_;

		switch (e->sig) {

		case UPDATE_SIG: {
			//publish a time event with signal REQUEST_UPDATE_SIG
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);
			status_ = Q_RET_HANDLED;
			break;
		}
		case REQUEST_UPDATE_SIG: {
			//if CHM recevied REQUEST_UPDATE_SIG we need to update AOs_alive array in index = N_MEMBER to signal the CHM AO is active
			AOs_alive[N_MEMBER] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case ALIVE_SIG: {
			//if received an ALIVE_SIG CHM needs to update the AOs_alive array in the appropriate index 
			std::cout << "I'm alive" << std::endl;
			AOs_alive[(Q_EVT_CAST(MemberEvt)->memberNum)] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case Q_EXIT_SIG: {
			timeEvt_request_update.disarm();
			timeEvt_kick.disarm();
			//timeEvt.disarm();
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case TERMINATE_SIG: {
			std::terminate();
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case NOT_MEMBER_SIG: {
			//update subscribers array to show that a user has unsubscribed
			subscribers[(Q_EVT_CAST(MemberEvt)->memberNum)] = false;
			status_ = Q_RET_HANDLED;
			break;
		}
		case MEMBER_SIG: {
			//update subscribers array to show a user has subscribed
			subscribers[(Q_EVT_CAST(MemberEvt)->memberNum)] = true;
			//update AOs_alive array- if a user has subscribed it counts as an ALIVE signal
			AOs_alive[(Q_EVT_CAST(MemberEvt)->memberNum)] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case KICK_SIG: {
			QP::QEvt ev;
			bool kick = true;
			//pass through AOs_alive array and check whether any subscribed members aren't responsive.
			//if so print to error log and refrain from kicking watchdog
			for (int i = 0; i < N_MEMBER ; i++) {
                //check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if ((AOs_alive[i] == false) && (subscribers[i] == true)) {
					kick = false;
					if(i == N_MEMBER) std::cout<<"Watchdog wasn't kicked because CHM system didn't send ALIVE signal"<<std::endl;
					else std::cout <<"Watchdog wasn't kicked because member "<<i<< " didn't send ALIVE signal" <<std::endl;
				}
			}
			//check if the chm system is unresponsive
			if (AOs_alive[N_MEMBER] == false) kick = false;
			//if all subscribers are alive, kick watchdog
			if (kick == true) watch_dog_queue.post(&ev, QP::QF_NO_MARGIN);

			//set the AOs_alive array back to default (false) for next cycle
			for (int i = 0; i < N_MEMBER + 1; i++) AOs_alive[i] = false;
			status_ = Q_RET_HANDLED;
			break;
		}
		 /*
		case TIMEOUT_SIG: {
			watchdog_in_chm--;
			std::cout << "watchdog in chm : "<<watchdog_in_chm << std::endl;
			status_ = super(&top);
			break;
		
		}
		*/
		default: {
			status_ = super(&top);
			break;
		}
		}
		return status_;
	}
}