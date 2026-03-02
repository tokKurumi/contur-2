#pragma once
#pragma once
#ifndef __Kernel_Н__
#define __Kernel_Н__

#include "Dispatcher.h" 
#include "Scheduler.h"

class Kernel
{
private:
	Dispatcher * dispatcher;

protected:

public:
	Kernel(Dispatcher *dispatcher)
	{
		this->dispatcher = dispatcher;
	}

	// создание процесса
	HANDLE * CreateProcess(string user, Memory * code)
	{
		/* обратиться к блоку управления VirtualMemory для выделнения памяти под образ процесса */
		ProcessImage *processImage = dispatcher->getVirtualMemory();

		if (processImage == NULL)
			return NULL; // нет памяти 
		
		processImage->setUser(user); 
		dispatcher->initProcessID(processImage,code);

						 // инициализировать процесс 
		return (HANDLE *)processImage;
	} // end CreateProcess

	// удаление процесса
	void TerminateProcess(HANDLE* handle)
	{
		// 1. очистить VirtualMemory и sysreg процесса 
		dispatcher->freeVirtualMemory(handle);
	}

	// методы критической секции
	HANDLE* CreateCriticalSection()
	{
		return dispatcher->getHandleCS();
	}

	void EnterCriticalSection(HANDLE* handle)
	{
		dispatcher->EnterCriticalSection(handle);
	}

	void LeaveCriticalSection(HANDLE* handle)
	{
		dispatcher->LeaveCriticalSection(handle);
	}

	void DebugProcessImage(HANDLE* handle)
	{
		((ProcessImage*)handle)->Debug();
	}
};
#endif