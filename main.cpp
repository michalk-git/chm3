
#include "qpcpp.h"
#include "system.h"
#include "bsp.h"
#include "watchdog.h"
#include <iostream>
using namespace Core_Health;

//............................................................................
int main(int argc, char *argv[]) {
    static QP::QEvt const *chmQueueSto[N_MEMBER];
    static QP::QEvt const *memberQueueSto[N_MEMBER][N_MEMBER];
    static QP::QSubscrList subscrSto[Core_Health::MAX_PUB_SIG];
    static QF_MPOOL_EL(Core_Health::MemberEvt) smlPoolSto[2*N_MEMBER];

    QP::QF::init();  // initialize the framework and the underlying RT kernel

    Core_Health::BSP::init(argc, argv); // initialize the BSP

    QP::QF::psInit(subscrSto, Q_DIM(subscrSto)); // init publish-subscribe

    // initialize event pools...
    QP::QF::poolInit(smlPoolSto,
                     sizeof(smlPoolSto), sizeof(smlPoolSto[0]));

    // start the active objects...
    for (uint8_t n = 0U; n < N_MEMBER; ++n) {
        AO_Member[n]->start((uint8_t)(n + 1U),
                           memberQueueSto[n], Q_DIM(memberQueueSto[n]),
                           (void *)0, 0U);
    }
	
    AO_CHM->start((uint8_t)(N_MEMBER + 1U),
                    chmQueueSto, Q_DIM(chmQueueSto),
                    (void *)0, 0U);




	
    return QP::QF::run(); // run the QF application
}
