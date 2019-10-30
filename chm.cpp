
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>
#include "watchdog.h"
#include <chrono>
#include <map>
#include "chm.h"

Q_DEFINE_THIS_FILE
using namespace std;
using namespace Core_Health;
#define KICK_SIGNAL_DELAY 0.05
#define EMPTY (-1)


namespace Core_Health {
	class HealthMonitor : public QP::QActive {
	public:
		static HealthMonitor inst;

	private:
		 
		User          members[N_MEMBER];
		QP::QTimeEvt  timeEvt_request_update;
		QP::QTimeEvt  timeEvt_kick;
		int           curr_subscribers;      //curr_subscribers is the number of subscribed users
		int           curr_alive;            //curr_alive is the number of alive users (out of the subscribed users)
		map<int, int> id_to_index_dict;
		WatchDog&     watchdog_instance;

		
	public:
		HealthMonitor();
		void SubscribeUser(int user_id);
		void UnSubscribeUser(int sys_id);
		void LogDeactivatedUsersAndReset();

	protected:
		Q_STATE_DECL(initial);
		Q_STATE_DECL(active);

	};
}

#if (QP_VERSION < 650U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpcpp version 6.5.0 or higher required
#endif

namespace Core_Health {
QP::QActive * const AO_HealthMonitor = &HealthMonitor::inst; // "opaque" pointer to HealthMonitor AO

}

namespace Core_Health {


	HealthMonitor HealthMonitor::inst;

	HealthMonitor::HealthMonitor(): QActive(&initial), watchdog_instance(singleton<WatchDog>::getInstance()), timeEvt_request_update(this, UPDATE_SIG, 0U), timeEvt_kick(this, KICK_SIG, 0U){
		//the starting default is: no subscribers and no subscribers who are alive
		curr_subscribers = 0;
		curr_alive = 0;

		//initialize the members array (the default setting is that there are no users)...
		for (int i = 0; i < N_MEMBER; i++) {
			members[i].subscribed = false;
			members[i].keep_alive_received = false;
			members[i].id = -1;
		}

		//start the watchdog
		watchdog_instance.Start(CHMConfig_t::T_WATCHDOG_RESET_SEC);
	};


	Q_STATE_DEF(HealthMonitor, initial) {
		(void)e; // suppress the compiler warning about unused parameter

		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC seconds
		timeEvt_kick.armX(ConvertSecondsToTicks((CHMConfig_t::T_UPDATE_WATCHDOG_SEC) + KICK_SIGNAL_DELAY), ConvertSecondsToTicks((CHMConfig_t::T_UPDATE_WATCHDOG_SEC) + KICK_SIGNAL_DELAY));
		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC seconds
		timeEvt_request_update.armX(ConvertSecondsToTicks(CHMConfig_t::T_AO_ALIVE_SEC), ConvertSecondsToTicks(CHMConfig_t::T_AO_ALIVE_SEC));

		return tran(&active);
	}

	//${AOs::HealthMonitor::SM::active} ..................................................
	Q_STATE_DEF(HealthMonitor, active) {
		QP::QState status_;
		int index = -1;

		switch (e->sig) {
		case UPDATE_SIG: {
			cout << "update" << endl;
			//publish signal REQUEST_UPDATE_SIG 
			QP::QEvt* update_evt = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(update_evt, this);
			status_ = Q_RET_HANDLED;
			break;
		}

		case ALIVE_SIG: {
			cout << "I'm alive" << endl;
			//find which user has sent the ALIVE_SIG signal (ie the appropriate system id)
			index = (Q_EVT_CAST(MemberEvt)->memberNum);
			
			//check if the user hasn't sent an ALIVE_SIG already: if not, update the members array in the appropriate index
			if (members[index].keep_alive_received == false) {
				members[index].keep_alive_received = true;
				curr_alive++;
			}
			// if all the subscribers are alive, kick the watchdog
			if (curr_alive == curr_subscribers) watchdog_instance.Kick();
			status_ = Q_RET_HANDLED;
			break;
		}
        
		case SUBSCRIBE_SIG: {
			int user_id = (int)(Q_EVT_CAST(RegisterNewUserEvt)->id);

			//check if the user is already in the system; if he is, print to error log 
			bool user_in_sys = (id_to_index_dict.find(user_id) != id_to_index_dict.end());
			if (user_in_sys) {
				printf("User %d is already in the system\n", user_id);
				status_ = Q_RET_HANDLED;
				break;
			}
			//else, subscribe the user (if there is free space)
			SubscribeUser(user_id);

			status_ = Q_RET_HANDLED;
			break;
		}

		case UNSUBSCRIBE_SIG: {
			// find the system id (ie index in members array) of the member to unsubscribe
			int sys_id = (Q_EVT_CAST(MemberEvt)->memberNum);
			UnSubscribeUser(sys_id);
			status_ = Q_RET_HANDLED;
			break;
		}

		case KICK_SIG: {
			cout << "kick" << endl;
			//if all the subscribers have sent an ALIVE_SIG signal, kick the watchdog
			if (curr_alive == curr_subscribers) watchdog_instance.Kick();
			// we need to log if users aren't active and then reset the system for the next cycle
			LogDeactivatedUsersAndReset();

			status_ = Q_RET_HANDLED;
			break;
		}
        

		case Q_EXIT_SIG: {
			timeEvt_request_update.disarm();
			timeEvt_kick.disarm();
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




	void HealthMonitor::SubscribeUser(int user_id){
		int index = -1;
		// find the first free index in the members array
		for (int i = 0; i < N_MEMBER; i++) {
			if (members[i].id == EMPTY) {
				//if we found an index in the members array not associated with another user, register the new subscriber
				members[i].id = user_id;
				members[i].subscribed = true;
				curr_subscribers++;

				//the subscription is an ALIVE signal
				members[i].keep_alive_received = true;
				curr_alive++;

				//if we succedded in registering the new user we need to update the dictionary and notify the associated AO_Member
				id_to_index_dict[user_id] = i;
				MemberEvt* member_evt = Q_NEW(MemberEvt, SUBSCRIBE_SIG);
				member_evt->memberNum = i;
				AO_Member[i]->postFIFO(member_evt);
				index = i;
				break;
			}
		}
		if (index == -1) printf("System full\n");
	}

	void HealthMonitor::UnSubscribeUser(int index) {
		map<int, int>::iterator it;
		//update subscribers array to show that a user has unsubscribed and decrease the number of
		//subscribed members
		if (members[index].subscribed == true) {
			members[index].subscribed = false;
			curr_subscribers--;
			//check if the user has sent an ALIVE_SIG signal recently; if so we will set
			//the keep_alive_received to false and decrease the number of live subscribers
			if (members[index].keep_alive_received == true) {
				members[index].keep_alive_received = false;
				curr_alive--;
			}
			//erase the user from the id-index dictionary
			it = id_to_index_dict.find(members[index].id);
			id_to_index_dict.erase(it);

			members[index].id = -1;

		}
	}

	void HealthMonitor::LogDeactivatedUsersAndReset() {
		bool subscriber_is_unresponsive = false;;
		//for each user set the keep_alive_received to false for next cycle (and reset curr_alive)
		for (int i = 0; i < N_MEMBER; i++) {
			subscriber_is_unresponsive = ((members[i].subscribed == true) && (members[i].keep_alive_received == false));
			if (subscriber_is_unresponsive) {
				cout << "Watchdog wasn't kicked because member id " << members[i].id << " didn't send ALIVE signal" << endl;
			}
			members[i].keep_alive_received = false;
		}
		curr_alive = 0;
	}

	float ConvertSecondsToTicks(float seconds) {
		return (seconds * (BSP::TICKS_PER_SEC));
	}
}

