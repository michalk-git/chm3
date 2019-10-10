
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

		int curr_alive;                                                     //curr_alive is the number of subscribed users that have sent ALIVE signal in the current period
		int curr_subscribed;                                                //curr_subscribed is the number of subscribed users
		int curr_members_num;                                               //curr_members_num is the total number of users (both subscribed and not)

		
		 WatchDog& wd = WatchDog::getInstance();

		
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
		: QActive(&initial), timeEvt_request_update(this,UPDATE_SIG,0U) ,timeEvt_kick(this,KICK_SIG,0U)
	{
		//initialize the members array (the default setting is that there are no users)...Also intialize the curr_alive and curr_subscribed variables
		curr_alive = 0;
		curr_subscribed = 0;
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

		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC seconds
		timeEvt_request_update.armX(BSP::TICKS_PER_SEC*(CHMConfig_t::T_AO_ALIVE_SEC), BSP::TICKS_PER_SEC * (CHMConfig_t::T_AO_ALIVE_SEC));
		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC seconds
		timeEvt_kick.armX(BSP::TICKS_PER_SEC*(CHMConfig_t::T_UPDATE_WATCHDOG_SEC), BSP::TICKS_PER_SEC*(CHMConfig_t::T_UPDATE_WATCHDOG_SEC));

		return tran(&active);
	}
	//${AOs::CHM::SM::active} ..................................................
	Q_STATE_DEF(CHM, active) {

		QP::QState status_;
		int index = -1;
		switch (e->sig) {

		case UPDATE_SIG: {
			std::cout << "update" << std::endl;
			//publish a time event with signal REQUEST_UPDATE_SIG
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);
		//	if (curr_subscribed == 0) wd.kick();
			status_ = Q_RET_HANDLED;
			break;
		}
		case NEW_USER_SIG: {
			//if received an NEW_USER_SIG CHM needs to enter the user to the members array 
			//we also need to send an acknowledge signal to the member

			MemberEvt* ev = Q_NEW(MemberEvt, Core_Health::ACKNOWLEDGE_SIG);
			ev->memberNum = curr_members_num;
			Core_Health::AO_Member[curr_members_num]->postFIFO(ev);
			//update the members array to include the new user
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
			
			//check if the user hasn't sent an ALIVE_SIG already: if he hasn't update the members array in the appropriate index and advance the curr_alive variable
			if (members[index].keep_alive_received == false) {
				members[index].keep_alive_received = true;
				//curr_alive++;
			}
	
			status_ = Q_RET_HANDLED;
			break;
		}
		case Q_EXIT_SIG: {
			timeEvt_request_update.disarm();
			timeEvt_kick.disarm(); 
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case TERMINATE_SIG: {
			
			std::terminate();
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case NOT_MEMBER_SIG: {
			//update subscribers array to show that a user has unsubscribed and decrease the number of subscribed members
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			if (members[index].subscribed == true) {
				members[index].subscribed = false;
				//curr_subscribed--;
				//if the user sent an ALIVE_SIG in the past we need to decrease the curr_alive variable since 'curr_alive' is the live users from the subscribers subset
				//if (members[index].keep_alive_received == true) curr_alive--;
				
			}

			status_ = Q_RET_HANDLED;
			break;
		}
		case MEMBER_SIG: {
			//update subscribers array to show that a user has subscribed and increase the number of subscribed members
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			if (members[index].subscribed == false) {
				members[index].subscribed = true;
			//	curr_subscribed++;
				//the subscription is an ALIVE signal
				members[index].keep_alive_received = true;
			//	curr_alive++;
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
			for (int i = 0; i < N_MEMBER ; i++) {
                //check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if ((members[i].keep_alive_received == false) && (members[i].subscribed == true)) {
					kick = false;
					std::cout <<"Watchdog wasn't kicked because member id "<< members[i].id<< " didn't send ALIVE signal" <<std::endl;
				}
			}
			if (kick == true) wd.kick();
			//if all subscribers are alive, kick watchdog
			//if (curr_alive == curr_subscribed) wd.kick();
				

			//set the AOs_alive array back to default (false) for next cycle
			for (int i = 0; i < N_MEMBER ; i++) members[i].keep_alive_received = false;
			//curr_alive = 0;
			if(wd.GetCounter() <= 0) std::terminate();
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