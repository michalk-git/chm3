
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>

using namespace std;
Q_DEFINE_THIS_FILE

// Active object class -------------------------------------------------------
//$declare${AOs::Member} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

//${AOs::Member} ..............................................................
class Member : public QP::QActive {
public:
    static Member inst[N_MEMBER];

private:
	int num_deactivated_cycles;
	int system_id;

public:
	Member();

protected:
    Q_STATE_DECL(initial);
    Q_STATE_DECL(active);

};

} // namespace Core_Health
//$enddecl${AOs::Member} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

namespace Core_Health {

// Local objects -------------------------------------------------------------
static Member l_Member[N_MEMBER];   // storage for all Members


} // namespace Core_Health

//$skip${QP_VERSION} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// Check for the minimum required QP version
#if (QP_VERSION < 650U) || (QP_VERSION != ((QP_RELEASE^4294967295U) % 0x3E8U))
#error qpcpp version 6.5.0 or higher required
#endif
//$endskip${QP_VERSION} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//$define${AOs::AO_Member[N_Member]} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

//${AOs::AO_Member[N_Member]} ..................................................
QP::QActive * const AO_Member[N_MEMBER] = { // "opaque" pointers to Member AO
    & Member::inst[0],
    & Member::inst[1],
    & Member::inst[2],
    & Member::inst[3],
    & Member::inst[4]
};




} // namespace Core_Health
//$enddef${AOs::AO_Member[N_Member]} ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
//$define${AOs::Member} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

	//${AOs::Member} ..............................................................
	Member Member::inst[N_MEMBER];
	//${AOs::Member::Member} .......................................................
	Member::Member()
		: QActive(&initial), num_deactivated_cycles(0){};
		
	

	//${AOs::Member::SM} ..........................................................
	Q_STATE_DEF(Member, initial) {
		//${AOs::Member::SM::initial}
		(void)e; // suppress the compiler warning about unused parameter

		return tran(&active);
	}
	//${AOs::Member::SM::active} ................................................
	Q_STATE_DEF(Member, active) {
		QP::QState status_;
		switch (e->sig) {
		case REQUEST_UPDATE_SIG: {
			//if a member AO recevied a REQUEST_UPDATE_SIG it needs to post an ALIVE_SIG to the HealthMonitor active object unless it has been deactivated
			if (num_deactivated_cycles == 0) {
				MemberEvt* member_evt = Q_NEW(MemberEvt, ALIVE_SIG);
				member_evt->memberNum = system_id;
				AO_HealthMonitor->postFIFO(member_evt, this);
				cout << "member " << system_id << " has sent ALIVE signal" <<endl;
			}
			else num_deactivated_cycles--;
			status_ = Q_RET_HANDLED;
			break;
		}

        
		case SUBSCRIBE_SIG: {
			
			//all users who wish to subscribe to health monitor system will subscribe to the REQUEST_UPDATE_SIG signal
			subscribe(REQUEST_UPDATE_SIG);
			system_id = (Q_EVT_CAST(MemberEvt)->memberNum);
			cout << "member " << system_id << " has subscribed" << endl;
			status_ = Q_RET_HANDLED;
			break;
		}
		case UNSUBSCRIBE_SIG: {
			//users who wish to unsubscribe will stop receiving REQUEST_UPDATE_SIG signal
			 unsubscribe(REQUEST_UPDATE_SIG);
			 //send the health monitor active object a signal to notify the leaving of a subscriber
			 MemberEvt* member_evt = Q_NEW(MemberEvt, UNSUBSCRIBE_SIG);
			 member_evt->memberNum = system_id;
			 AO_HealthMonitor->postFIFO(member_evt, this);
			 cout << "member " << (int)member_evt->memberNum << " has unsubscribed" << endl;
			 status_ = Q_RET_HANDLED;
			 break;
		}
		//used for testing purposes
		case DEACTIVATE_SIG: {
			num_deactivated_cycles = (Q_EVT_CAST(DeactivationEvt)->period_num);
			status_ = Q_RET_HANDLED;
			break;
		}
		//${AOs::Member::SM::active}
		case Q_EXIT_SIG: {

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
