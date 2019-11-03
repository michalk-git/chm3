#pragma once
#include "system.h"

class SubscriptionHandler {
	User members[N_MEMBER];
	

public:
	SubscriptionHandler() = default;

	// IsIdInSystem : checks if a user with id = 'user_id' is already in the system
	bool IsIdInSystem(int user_id)const;

	// SubscribeUser : checks if the id = 'user_id' is new to the system and if there is free space in the system; If so, registers the new user and returns the index of the 
	// members array associated to the new user. If the user was already in the system or the system was full, returns -1
	int SubscribeUser(int user_id);

	// UnSubscribeUser :  updates the members array by removing the user associated with the system id = 'sys_id'. 
	bool UnSubscribeUser(int sys_id);

	// FindFreeSpace : returns index of empty cell in members array unless the members array is full; In that case, will return -1
	int FindFreeSpace()const;

	// LogUnResponsiveUsersAndReset : check if the users were responsive in the past cycle; If not, print to error log. In any case, reset the ALIVE signals sent to the system for next cycle
	void LogUnResponsiveUsersAndReset();

	// AreAllMembersAlive :  check if all the users were responsive during the past cycle; If so, return true
	bool AreAllMembersResponsive()const;

	// UpdateAliveStatus : sets the 'keep_alive_received' parameter to true for the user associated with system id = 'sys_id'
	void UpdateAliveStatus(int sys_id);

};


