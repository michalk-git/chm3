
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>
Q_DEFINE_THIS_FILE

// Active object class -------------------------------------------------------
//$declare${AOs::CHM} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

	//${AOs::CHM} ..............................................................
	class CHM : public QP::QActive {
	public:
		static CHM inst;

	private:
		bool subscribers[N_MEMBER];
		bool subscribers_alive[N_MEMBER+1];
		int watchdog;
		QP::QTimeEvt timeEvt_update;
		QP::QTimeEvt timeEvt_timer;
		QP::QTimeEvt timeEvt_kick;

	public:
		CHM();
		void kick() {
			watchdog = Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC;
		}
		void SetResetInterval(unsigned int interval) {
			Core_Health::CHMConfig_t::T_UPDATE_WATCHDOG_SEC = interval;
		}

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
QP::QActive * const AO_CHM = &CHM::inst; // "opaque" pointer to Table AO

} // namespace Core_Health
//$enddef${AOs::AO_CHM} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//$define${AOs::CHM} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

	//${AOs::Table} ..............................................................
	CHM CHM::inst;
	//${AOs::CHM::CHM} .......................................................
	CHM::CHM()
		: QActive(&initial), watchdog(Core_Health::CHMConfig_t::T_WATCHDOG_RESET_SEC), timeEvt_update(this,UPDATE_SIG,0U),timeEvt_timer(this,TIMEOUT_SIG,0U),timeEvt_kick(this,KICK_SIG,0U)
	{
		//index = N_MEMBER is subscribers_alive array is for the CHM system to signal activity
		subscribers_alive[N_MEMBER] = false;
		//initialize subscribers_alive array to false and subscribers array to true (the default is that all users are subscribers and havn't sent an ALIVE signal yet
		for (int i = 0; i < N_MEMBER ; i++) {
			subscribers_alive[i] = false;
			subscribers[i] = true;
		}

	};

	//${AOs::CHM::SM} ..........................................................
	Q_STATE_DEF(CHM, initial) {
		//${AOs::CHM::SM::initial}
		(void)e; // suppress the compiler warning about unused parameter
		std::cout << "in table initial" << std::endl;
		//subscribe to REQUEST_SIG in order to check CHM AO is still active
		subscribe(REQUEST_UPDATE_SIG);
		//arm time event that fires the signal UPDATE_SIG every T_AO_ALIVE_SEC 
		timeEvt_update.armX(BSP::TICKS_PER_SEC*(Core_Health::CHMConfig_t::T_AO_ALIVE_SEC), BSP::TICKS_PER_SEC * (Core_Health::CHMConfig_t::T_AO_ALIVE_SEC));
		//arm time event that fires the signal TIMEOUT_SIG every second 
		timeEvt_timer.armX(BSP::TICKS_PER_SEC, BSP::TICKS_PER_SEC);
		//arm time event that fires  the signal KICK_SIG every T_UPDATE_WATCHDOG_SEC
		timeEvt_kick.armX(BSP::TICKS_PER_SEC*(Core_Health::CHMConfig_t::T_UPDATE_WATCHDOG_SEC), BSP::TICKS_PER_SEC*(Core_Health::CHMConfig_t::T_UPDATE_WATCHDOG_SEC));
		return tran(&active);
	}
	//${AOs::CHM::SM::active} ..................................................
	Q_STATE_DEF(CHM, active) {
		QP::QState status_;
		std::cout << "in table active" << std::endl;
		switch (e->sig) {

		case UPDATE_SIG: {
			//publish a time event with signal REQUEST_SIG
			QP::QEvt* e = Q_NEW(QP::QEvt, REQUEST_UPDATE_SIG);
			QP::QF::PUBLISH(e, this);
			status_ = Q_RET_HANDLED;
			break;
		}
		case REQUEST_UPDATE_SIG: {
			//if CHM recevied REQUEST_SIG we need to update subscribers_alive array in index = N_MEMBER to signal the CHM AO is active
			subscribers_alive[N_MEMBER] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case ALIVE_SIG: {
			//if received an ALIVE_SIG CHm needs to update the subscribers_alive array in the appropriate index 
			std::cout << " in alive\n" << std::endl;
			subscribers_alive[(Q_EVT_CAST(MemberEvt)->memberNum)] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case Q_EXIT_SIG: {
			timeEvt_update.disarm();
			timeEvt_kick.disarm();
			timeEvt_timer.disarm();
			status_ = Q_RET_HANDLED;
			break;
		}
		case TIMEOUT_SIG: {
			watchdog--;
			printf("watchdog : %d\n", watchdog);
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
			//update subscribers_alive array- if a user has subscribed it counts as an ALIVE signal
			subscribers_alive[(Q_EVT_CAST(MemberEvt)->memberNum)] = true;
			status_ = Q_RET_HANDLED;
			break;
		}
		case KICK_SIG: {
			bool kick = true;
			//pass through subscribers_alive array and check whether any subscribed members aren't responsive.
			//if so print error log and refrain from kicking watchdog
			for (int i = 0; i < N_MEMBER ; i++) {
                //check for each user if he is both subscribed and non-active: if so update kick to false (to make sure the system doesn't kick watchdog) and print to error log
				if (subscribers_alive[i] == false && subscribers[i] == true) {
					kick = false;
					if(i == N_MEMBER) printf("CHM system didn't send ALIVE signal\n");
					else printf("Member %d didn't send ALIVE signal\n", i);
				}
			}
			//check if the chm sys is unresponsive
			if (subscribers_alive[N_MEMBER] == false) kick = false;
			//if all subscribers are alive, kick watchdog
			if (kick == true) this->kick();;
			//set the subscribers_alive array back to default (false) for next cycle
			for (int i = 0; i < N_MEMBER + 1; i++) subscribers_alive[i] = false;
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