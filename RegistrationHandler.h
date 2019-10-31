#pragma once
#include "system.h"

class SubscriptionHandler {
	User members[N_MEMBER];
	

public:
	SubscriptionHandler() = default;

	bool IsIdInSystem(int user_id)const;

	int SubscribeUser(int user_id);

	void UnSubscribeUser(int sys_id);

	int FindFreeSpace()const;

	void LogUnResponsiveUsersAndReset();

	bool AreAllMembersAlive()const;

	void UpdateAliveStatus(int sys_id);

};


