
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include <iostream>

Q_DEFINE_THIS_FILE

// Active object class -------------------------------------------------------
//$declare${AOs::Member} vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
namespace Core_Health {

//${AOs::Member} ..............................................................
class Member : public QP::QActive {
public:
    static Member inst[N_MEMBER];

private:


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

// helper function to provide the ID of Member "me"
inline uint8_t Member_ID(Member const * const me) {
    return static_cast<uint8_t>(me - &Member::inst[0]);
}

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
		: QActive(&initial)
		
	{}

	//${AOs::Member::SM} ..........................................................
	Q_STATE_DEF(Member, initial) {
		//${AOs::Member::SM::initial}
		static bool registered = false; // starts off with 0, per C-standard
		(void)e; // suppress the compiler warning about unused parameter

		subscribe(REQUEST_UPDATE_SIG);
		return tran(&active);
	}
	//${AOs::Member::SM::active} ................................................
	Q_STATE_DEF(Member, active) {
		QP::QState status_;
		switch (e->sig) {
		case REQUEST_UPDATE_SIG: {
			//if a member AO recevied a REQUEST_UPDATE_SIG it needs to post an ALIVE_SIG to the CHM AO with the member index
			std::cout << "recived" << std::endl;
			Core_Health::MemberEvt* ae = Q_NEW(Core_Health::MemberEvt, ALIVE_SIG);
			ae->memberNum = Member_ID(this);
			printf("mem id %d\n", ae->memberNum);
			AO_CHM->postFIFO(ae, this);
			status_ = Q_RET_HANDLED;
			break;
		}
		case SUBSCRIBE_SIG: {
			subscribe(REQUEST_UPDATE_SIG);
			//send chm AO a signal to notify the return of a subscriber
			Core_Health::MemberEvt* ae = Q_NEW(Core_Health::MemberEvt, MEMBER_SIG);
			ae->memberNum = Member_ID(this);
			AO_CHM->postFIFO(ae, this);
			status_ = Q_RET_HANDLED;
			break;
		}
		case UNSUBSCRIBE_SIG: {
			 unsubscribe(REQUEST_UPDATE_SIG);
			 //send chm AO a signal to notify the leaving of a subscriber
			 Core_Health::MemberEvt* ae = Q_NEW(Core_Health::MemberEvt, NOT_MEMBER_SIG);
			 ae->memberNum = Member_ID(this);
			 AO_CHM->postFIFO(ae, this);
			 status_ = Q_RET_HANDLED;
			 break;
		}
		case MALFUNCTION_SIG: {
             
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
