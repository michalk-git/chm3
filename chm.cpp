
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
#define KICK_SIGNAL_DELAY 0.1

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


		int curr_members_num;      //curr_members_num is the total number of users (both subscribed and not)
		int curr_subscribers;      //curr_subscribers is the number of subscribed users
		int curr_alive;            //curr_alive is the number of alive users (out of the subscribed users)

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
		: QActive(&initial), wd(singleton<WatchDog>::getInstance()), timeEvt_request_update(this,UPDATE_SIG,0U)
		  ,timeEvt_kick(this, KICK_SIG,0U)
	{
		//the starting default is: there are mo users, no subscribers and no subscribers who are alive
		curr_members_num = 0;
		curr_subscribers = 0;
		curr_alive = 0;
		//initialize the members array (the default setting is that there are no users)...
		for (int i = 0; i < N_MEMBER; i++) {
			members[i].subscribed = false;
			members[i].keep_alive_received = false;
			members[i].id = -1;
		}

		//start the watchdog
		wd.start(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC);
		
	};

	unsigned int SecondsToTicks(unsigned int seconds) {
		return (seconds * (BSP::TICKS_PER_SEC));
	}

	//${AOs::CHM::SM} ..........................................................
    //${AOs::CHM::SM::initial}
	Q_STATE_DEF(CHM, initial) {

		(void)e; // suppress the compiler warning about unused parameter

		
		
		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC seconds
		timeEvt_kick.armX( SecondsToTicks((CHMConfig_t::T_UPDATE_WATCHDOG_SEC + KICK_SIGNAL_DELAY)) , SecondsToTicks((CHMConfig_t::T_UPDATE_WATCHDOG_SEC + KICK_SIGNAL_DELAY)));
		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC seconds
		timeEvt_request_update.armX(SecondsToTicks(CHMConfig_t::T_AO_ALIVE_SEC) , SecondsToTicks(CHMConfig_t::T_AO_ALIVE_SEC));


		return tran(&active);
	}
	//${AOs::CHM::SM::active} ..................................................
	Q_STATE_DEF(CHM, active) {

		QP::QState status_;
		int index = -1;
		switch (e->sig) {

		case UPDATE_SIG: {
			std::cout << "update" << std::endl;
			//publish a time event with signal REQUEST_UPDATE_SIG; ie a signal published  
			//which will prompt an ALIVE_SIG from each subscribed user
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);

			status_ = Q_RET_HANDLED;
			break;
		}
		case NEW_USER_SIG: {
			//if received an NEW_USER_SIG signal, CHM needs to enter the user's data to the members array: 
			//update the members array to include the new user which by default will be unsubscribed
			members[curr_members_num].id = (Q_EVT_CAST(UserEvt)->id);
			members[curr_members_num].keep_alive_received = false;
			members[curr_members_num++].subscribed = false;
			status_ = Q_RET_HANDLED;
			break;
		}
		case ALIVE_SIG: {
			//if received an ALIVE_SIG signal,CHM needs to update the members array in the appropriate index.
			std::cout << "I'm alive" << std::endl;
			//find which user has sent the ALIVE_SIG signal (ie the appropriate system id)
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			
			//check if the user hasn't sent an ALIVE_SIG already: if he hasn't update the members array in 
			//the appropriate index
			if (members[index].keep_alive_received == false) {
				members[index].keep_alive_received = true;
				curr_alive++;
				if (curr_alive == curr_subscribers) wd.kick();
				//std::cout << "curr alive is " << curr_alive << std::endl;
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
		
		
		case UNSUBSCRIBE_SIG: {
			//update subscribers array to show that a user has unsubscribed and decrease the number of
			//subscribed members

			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			if (members[index].subscribed == true) {
				members[index].subscribed = false;	
				curr_subscribers--;
				//check if the user has sent an ALIVE_SIG signal recently; if so we will set
				//the keep_alive_received to false and decrease the number of live subscribers
				if (members[index].keep_alive_received == true) {
					members[index].keep_alive_received = false;
					curr_alive--;
				}
				members[index].id = -1;
			}

			status_ = Q_RET_HANDLED;
			break;
		}
		case SUBSCRIBE_SIG: {
			int index = -1;
			for (int i = 0 ; i < N_MEMBER ; i++) {
				if (members[i].id == -1) {
					members[i].id = (int)(Q_EVT_CAST(UserEvt)->id);
					members[i].subscribed = true;
					curr_subscribers++;
					//the subscription is an ALIVE signal
					members[i].keep_alive_received = true;
					curr_alive++;
					index = i;
				}
				if (index != -1) AO_Member[index]->postFIFO(Q_NEW(Core_Health::MemberEvt, Core_Health::SUBSCRIBE_SIG));
				else printf("System full\n");
			}
			status_ = Q_RET_HANDLED;
			break;
		}
		
		case KICK_SIG: {
		
			QP::QEvt ev;
			std::cout << "kick" << std::endl;
			//std::cout << "curr_alive is " << curr_alive << std::endl;
			//std::cout << "curr_subs is " << curr_subscribers << std::endl;

			//if all the subscribers have sent an ALIVE_SIG signal, kick the watchdog
			if (curr_alive == curr_subscribers) wd.kick();

			//for each user set the keep_alive_received to false for next cycle (and reset curr_alive)
			for (int i = 0; i < curr_members_num; i++) {
				if ((members[i].keep_alive_received == false) && (members[i].subscribed == true)) {
					std::cout << "Watchdog wasn't kicked because member id " << members[i].id << " didn't send ALIVE signal" << std::endl;
				}
				members[i].keep_alive_received = false;
			}
			curr_alive = 0;
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