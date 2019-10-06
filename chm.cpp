
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
		int curr_members_num;
		QP::QTimeEvt timeEvt_request_update;
		QP::QTimeEvt timeEvt_kick;
		std::map<unsigned int,unsigned int> users_ids;
		int curr_alive;
		int curr_subscribed;
		
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
		curr_alive = 0;
		curr_subscribed = 0;
		for (int i = 0; i < N_MEMBER; i++) {
			members[i].subscribed = false;
			members[i].keep_alive_received = false;
			members[i].id = 0;
		}
		
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

		WatchDog& wd = WatchDog::getInstance();
		QP::QState status_;
		int index = -1;
		switch (e->sig) {

		case UPDATE_SIG: {
			//publish a time event with signal REQUEST_UPDATE_SIG
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);
			status_ = Q_RET_HANDLED;
			break;
		}
		case NEW_USER_SIG: {
			//if received an NEW_USER_SIG CHM needs to enter the user to the members array 
			//but first we need to check we havn't reached the maximum users allowed
			if (curr_members_num >= N_MEMBER) {
				std::cout << "Reached maximum limit of users" << std::endl;
				break;
			}
			//if not we should add the users information to the users array 'members': the users id and the default setting for the user which is unsubscribed and hasn't sent ALIVE_SIG yet
			users_ids[(Q_EVT_CAST(UserEvt)->id)] = curr_members_num;
			members[curr_members_num].id = (Q_EVT_CAST(UserEvt)->id);
			members[curr_members_num].keep_alive_received = false;
			members[curr_members_num++].subscribed = false;
			status_ = Q_RET_HANDLED;
			break;
		}
		case ALIVE_SIG: {
			//if received an ALIVE_SIG CHM needs to members array in the appropriate index...but first we need to find the appropriate index associated with the id (with the hash table)
			std::cout << "I'm alive" << std::endl;
			index = users_ids[(Q_EVT_CAST(UserEvt)->id)];
			if (members[index].keep_alive_received == false) {
				members[index].keep_alive_received = true;
				curr_alive++;
			}
			if (curr_alive == curr_subscribed) {
				wd.kick();
				for (int i = 0; i < N_MEMBER; i++) members[i].keep_alive_received = false;
				curr_alive = 0;
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
			index = users_ids[(Q_EVT_CAST(UserEvt)->id)];
			if (members[index].subscribed == true) {
				members[index].subscribed = false;
				curr_subscribed--;
			}
			status_ = Q_RET_HANDLED;
			break;
		}
		case MEMBER_SIG: {
			//update subscribers array to show that a user has subscribed and increase the number of subscribed members
			index = users_ids[(Q_EVT_CAST(MemberEvt)->memberNum)];
			if (members[index].subscribed == false) {
				members[index].subscribed = true;
				curr_subscribed++;
			}
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case KICK_SIG: {
		/*
			QP::QEvt ev;
			bool kick = true;
			//pass through AOs_alive array and check whether any subscribed members aren't responsive.
			//if so print to error log and refrain from kicking watchdog
			for (int i = 0; i < N_MEMBER ; i++) {
                //check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if ((members[i].keep_alive_received == false) && (members[i].subscribed == true)) {
					kick = false;
					std::cout <<"Watchdog wasn't kicked because member "<< members[i].id<< " didn't send ALIVE signal" <<std::endl;
				}
			}

			//if all subscribers are alive, kick watchdog
			if (kick == true) wd.kick();
			*/
			//set the AOs_alive array back to default (false) for next cycle
			for (int i = 0; i < N_MEMBER ; i++) members[i].keep_alive_received = false;
			curr_alive = 0;
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