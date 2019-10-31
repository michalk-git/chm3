#include "RegistrationHandler.h"
#include <stdio.h>

bool SubscriptionHandler::IsIdInSystem(int id)const {
	bool id_found = false;
	//pass through the members array and check any member has an ID = 'id'. If so, return true
	for (int i = 0; i < N_MEMBER; i++) {
		if (members[i].id == id) {
			id_found = true;
			break;
		}
	}
	return id_found;
}


int SubscriptionHandler::FindFreeSpace()const {
	int index = -1;
	for (int i = 0; i < N_MEMBER; i++) {
		if (members[i].id == NONE) {
			index = i;
			break;
		}
	}
	//if (index == -1) printf("System full\n");
	return index;
}




int SubscriptionHandler::SubscribeUser(int user_id) {
	bool user_in_sys = IsIdInSystem(user_id);
	if (user_in_sys) printf("User %d is already in the system\n", user_id);
	
	// find the first free index in the members array
	int index = FindFreeSpace();
	// if there a free space was found, subscribe user
    if(index != -1){
			//if we found an index in the members array not associated with another user, register the new subscriber
			members[index].id = user_id;
			//the subscription is an ALIVE signal
			members[index].keep_alive_received = true;
		
	}
	else printf("System full\n");
	return index;
}


void SubscriptionHandler::UnSubscribeUser(int index) {
	if((index < 0) || (index >= N_MEMBER)) return;
	//update subscribers array to show that a user has unsubscribed
	if (members[index].id != NONE) {
		members[index].id = NONE;
		//check if the user has sent an ALIVE_SIG signal recently; if set 'keep_alive_received' to false
		if (members[index].keep_alive_received == true) {
			members[index].keep_alive_received = false;
		}
	}
	return;
}



void SubscriptionHandler::LogUnResponsiveUsersAndReset() {
	bool subscriber_is_unresponsive = false;;
	//for each user set the keep_alive_received to false for next cycle (and reset curr_alive)
	for (int i = 0; i < N_MEMBER; i++) {
		subscriber_is_unresponsive = ((members[i].id != NONE) && (members[i].keep_alive_received == false));
		if (subscriber_is_unresponsive) printf("Watchdog wasn't kicked because member id %d didn't send ALIVE signal\n", members[i].id);
		members[i].keep_alive_received = false;
	}
}

bool SubscriptionHandler::AreAllMembersAlive()const {
	bool all_alive = true;
	bool member_unresponsive = false;
	// For each member check if there has been an ALIVE signal in the current cycle
	for (int i = 0; i < N_MEMBER; i++) {
		member_unresponsive = ((members[i].id != NONE) && (members[i].keep_alive_received));
		if (member_unresponsive) {
			all_alive = false;
			break;
		}
	}
	return all_alive;
}

void SubscriptionHandler::UpdateAliveStatus(int sys_id) {
	if ((sys_id < 0) || (sys_id >= N_MEMBER)) return;
	//check if the user hasn't sent an ALIVE_SIG already: if not, update the members array in the appropriate index
	if(members[sys_id].keep_alive_received == false) {
		members[sys_id].keep_alive_received = true;
	}
}