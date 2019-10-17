
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>
//#include <thread>
#include "watchdog.h"
#include <chrono>
#include <map>

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

		User members[N_MEMBER];
	
		QP::QTimeEvt timeEvt_request_update;
		QP::QTimeEvt timeEvt_kick;


		int curr_members_num;           //curr_members_num is the total number of users (both subscribed and not)
		
		 WatchDog& wd ;

		
	public:
		CHM();

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
		: QActive(&initial),wd(WatchDog::getInstance()) ,timeEvt_request_update(this,UPDATE_SIG,0U) ,timeEvt_kick(this,KICK_SIG,0U)
	{
		//initialize the members array (the default setting is that there are no users)...
		curr_members_num = 0;

		for (int i = 0; i < N_MEMBER; i++) {
			members[i].subscribed = false;
			members[i].keep_alive_received = false;
			members[i].id = -1;
		}

		//start the watchdog
		wd.start();
		
	};

	//${AOs::CHM::SM} ..........................................................
    //${AOs::CHM::SM::initial}
	Q_STATE_DEF(CHM, initial) {

		(void)e; // suppress the compiler warning about unused parameter

		
		
		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC seconds
		timeEvt_kick.armX(BSP::TICKS_PER_SEC * (CHMConfig_t::T_UPDATE_WATCHDOG_SEC), BSP::TICKS_PER_SEC * (CHMConfig_t::T_UPDATE_WATCHDOG_SEC));
		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC seconds
		timeEvt_request_update.armX(BSP::TICKS_PER_SEC * (CHMConfig_t::T_AO_ALIVE_SEC), BSP::TICKS_PER_SEC * (CHMConfig_t::T_AO_ALIVE_SEC));


		return tran(&active);
	}
	//${AOs::CHM::SM::active} ..................................................
	Q_STATE_DEF(CHM, active) {

		QP::QState status_;
		int index = -1;
		switch (e->sig) {

		case UPDATE_SIG: {
			std::cout << "update" << std::endl;
			//publish a time event with signal REQUEST_UPDATE_SIG; ie a signal published to all members 
			//which will prompt an ALIVE_SIG from each member
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);
			//check if the watchdogs's counter has reache zero; if so terminate the program
			if (wd.GetCounter().count() <= 0) std::terminate();
			status_ = Q_RET_HANDLED;
			break;
		}
		case NEW_USER_SIG: {
			//if received an NEW_USER_SIG CHM needs to enter the user to the members array 
			//we also need to send an acknowledge signal to the member
			MemberEvt* ev = Q_NEW(MemberEvt, Core_Health::ACKNOWLEDGE_SIG);
			ev->memberNum = curr_members_num;
			Core_Health::AO_Member[curr_members_num]->postFIFO(ev);
			//update the members array to include the new user which by default will be unsubscribed
			members[curr_members_num].id = (Q_EVT_CAST(UserEvt)->id);
			members[curr_members_num].keep_alive_received = false;
			members[curr_members_num++].subscribed = false;
			status_ = Q_RET_HANDLED;
			break;
		}
		case ALIVE_SIG: {
			//if received an ALIVE_SIG CHM needs to update the members array in the appropriate index.
			std::cout << "I'm alive" << std::endl;
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			
			//check if the user hasn't sent an ALIVE_SIG already: if he hasn't update the members array in 
			//the appropriate index
			if (members[index].keep_alive_received == false) {
				members[index].keep_alive_received = true;
			}
			bool kick = true;
			//pass through AOs_alive array and check whether any subscribed members aren't responsive.
			//if so refrain from kicking watchdog
			for (int i = 0; i < curr_members_num; i++) {
				//check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if ((members[i].subscribed == true) && (members[i].keep_alive_received == false) ) {
					kick = false;
					break;
				}
			}
			if (kick == true) wd.kick();
			
	
			status_ = Q_RET_HANDLED;
			break;
		}
		case Q_EXIT_SIG: {
			timeEvt_request_update.disarm();
			timeEvt_kick.disarm(); 
			status_ = Q_RET_HANDLED;
			break;
		}
		
		
		case NOT_MEMBER_SIG: {
			//update subscribers array to show that a user has unsubscribed and decrease the number of
			//subscribed members
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			if (members[index].subscribed == true) {
				members[index].subscribed = false;		

			}

			status_ = Q_RET_HANDLED;
			break;
		}
		case MEMBER_SIG: {
			//update subscribers array to show that a user has subscribed 
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			if (members[index].subscribed == false) {
				members[index].subscribed = true;
				//the subscription is an ALIVE signal
				members[index].keep_alive_received = true;

			}

			status_ = Q_RET_HANDLED;
			break;
		}
		
		case KICK_SIG: {
		
			QP::QEvt ev;
			std::cout << "kick" << std::endl;
			
			
			bool kick = true;
			//pass through AOs_alive array and check whether any subscribed members aren't responsive.
			//if so print to error log and refrain from kicking watchdog
			for (int i = 0; i < curr_members_num ; i++) {
                //check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if ((members[i].keep_alive_received == false) && (members[i].subscribed == true)) {
					kick = false;
					std::cout <<"Watchdog wasn't kicked because member id "<< members[i].id<< " didn't send ALIVE signal" <<std::endl;
				}
			}
			if (kick == true) wd.kick();

			//set the AOs_alive array back to default (false) for next cycle
			for (int i = 0; i < curr_members_num ; i++) members[i].keep_alive_received = false;
			status_ = Q_RET_HANDLED;
			break;
		}

		default: {
			status_ = super(&top);
			break;
		}
		}
		return status_;
	}
}