#pragma once

template<class T>
class singleton{

protected:
	singleton() = default;
public:
	static T& getInstance()
	{
		static T instance; 
		return instance;
	}

	singleton(singleton const&) = delete;
	void operator=(singleton const&) = delete;

};