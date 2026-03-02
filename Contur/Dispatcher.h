#pragma once
#ifndef __Dispatcher_H__
#define __Dispatcher_H__

#include <iostream>

#include "ArchitectureCPU.h"
#include "MMU.h"
#include "Process.h"
#include "Scheduler.h"
#include "VirtualMemory.h"

using namespace std;

/* управляет ресурсами, изменяет состояние процессов */
class Dispatcher
{
public:
	Dispatcher(int SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES, Scheduler *scheduler, MMU *mmu)
	{
		virtualMemory = new VirtualMemory(SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES);
		this->scheduler = scheduler;
		this->mmu = mmu; // Model memory unit
		this->cs = new CS; // critical section
		this->timeSlice = -1; /* квант времени для планирования по методу RR */
	}

	ProcessImage * getVirtualMemory()
	{
		/* системные регистры не присвоены, процессор не выделен */

		int addr = virtualMemory->getAddrFreeImage();

		if (addr == -1)
		{
			return NULL;
		}

		// 1. выделить VirtualMemory
		virtualMemory->setAddrImage(addr);

		// 2. получить образ по виртуальному адресу
		ProcessImage * processImage = virtualMemory->getImage();

		// 3. сохранить виртуальный адрес в образе
		processImage->setVirtualAddr(addr);

		// 4. увеличить время обслуживания по установленному адресу 
		processImage->setTservice(processImage->getTservice() + 1);

		return processImage;
	}

	void freeVirtualMemory(HANDLE *handle)
	{
		ProcessImage * processImage = (ProcessImage *)handle;
		/* 0. только если процесс в очереди ExitProcess (выполнился) */

		State state = processImage->getState();
		int ID = processImage->getID();

		// 1. очистить регистры
		scheduler->getSysReg(ID)->setStatus(true);
		scheduler->getSysReg(ID)->clearSysReg();
		processImage->setSysReg(NULL);

		// 2. удалить из очереди
		scheduler->pop(state, processImage);

		// 3. сохранить наблюдение для метода SPN;
		if (scheduler->vParam.getStateTime() != noParam)
		{
			scheduler->setObservation(processImage);

			if (scheduler->vParam.getState(TimeExecNotPreem))
				scheduler->setTthreshold(TimeExec); // выполнения SPN

													/* 4. установить порог наименьшего ожидаемого времени */
			if (scheduler->vParam.getState(TimeExec))
				scheduler->setTthreshold(TimeExec); // выполнения SRT

			if (scheduler->vParam.getState(TimeServ))
				scheduler->setTthreshold(TimeServ); // обслуживания HRRN
		}

		/* 4. освободить VirtualMemory, инициализировать PCB */
		virtualMemory->clearImage(((PCB*)handle)->getVirtualAddr());
	}

	void initProcessID(ProcessImage *processImage, Memory *code)
	{
		// 1. определить ID
		int ID = scheduler->getID();

		if (ID >= 0)
		{
			//ID не -1, т.е. массив регистров 
			processImage->setID(ID);
		}
		else
		{
			// в очередь как новый 
			cout << "init Process ID > MAX_PROCESS in system" << endl;
		}

		// 2. выделить регистры no ID 
		scheduler->getSysReg(ID)->setStatus(false);
		processImage->setSysReg(scheduler->getSysReg(ID));
		// 3. состояние процесса
		processImage->setState(NotRunning);
		// 4. разместить программу 
		processImage->setCode(code);
		// к программе нет доступа из вне
		code = NULL;
		// 5. установить время входа 
		processImage->setTenter(Timer::getTime());
		// 6. инициализировать время обслуживания
		processImage->setTservice(0);
		// 7. в очередь как NotRunning
		scheduler->push(NotRunning, processImage);
	}

	/* номера пунктов приведены в соответствии с порядком изучения */
	virtual void dispatch()
	{
		ProcessImage * processImage = NULL;
		Timer::Tick();

		/* 1. просмотреть всю очередь Running, если
		есть завершившиеся процессы, убрать только из
		очереди в виртуальной памяти; объект образа
		удаляется только методом ядра */

		int size_ = scheduler->size(Running);

		for (int i = 0; i < size_; i++)
		{
			processImage = (ProcessImage *)(scheduler->front(Running));
			//Увеличить время обслуживания.
			processImage->setTservice(processImage->getTservice() + 1);

			if (processImage->getState() == ExitProcess) //процесс завершен
			{
				scheduler->pop(Running); //удаляем из очереди
				scheduler->push(ExitProcess, processImage); //ставим в очередь выполненных
			}
		}

		//3. Просмотреть очередь Blocked (в книге именно такая нумерация)
		size_ = scheduler->size(Blocked);

		for (int i = 0; i < size_; i++)
		{
			processImage = (ProcessImage *)(scheduler->front(Blocked));

			//Увеличить время обслуживания.
			processImage->setTservice(processImage->getTservice() + 1);

			if (!cs->getCS())// критическая секция свободна
			{
				scheduler->pop(Blocked);// удаляем из очереди
				processImage->setState(Running); /*изменяем состояние процесса*/
				processImage->setFlag(1);
				// не изменяем приоритет процесса
				scheduler->push(ExitProcess, processImage); //ставим в очередь выплоняемых
				cs->setCS(true);//закрыть секцию
				break;
			}
		}

		/*2. просмотреть очередь NotRunning и, если для образа определены все ресурссы
		(регистры и. т. д.) поставить на выполнение*/
		size_ = scheduler->size(NotRunning);

		for (int i = 0; i < size_; i++)
		{
			processImage = (ProcessImage *)(scheduler->front(NotRunning));

			//Увеличить время обслуживания.
			processImage->setTservice(processImage->getTservice() + 1);

			/*Выделены ли под образ регистры системы и процессор*/
			if (processImage->getSysReg() == NULL)
			{
				/*выделить регистры, если нет свободных оставить в очереди*/
				SysReg *sysReg = scheduler->getSysReg(scheduler->getID());

				if (sysReg == NULL)
				{
					scheduler->pop(NotRunning);// удаляем из очереди
					scheduler->push(NotRunning, processImage); //ставим в эту же очередь
					continue;
				}

				processImage->setSysReg(sysReg);
			}

			scheduler->pop(NotRunning);// удаляем из очереди

									   /*2.1. Code загрузить в память, процессор выделен обратиться к блоку управления памятью,
									   если есть ресурс*/
			if (processImage->getFlag() == 1)
			{
				processImage->setState(Running);
				/* изменяем состояние процесса; установить квант времени и приоритет,
				в зависимости от метода */
				processImage->setTimeSlice(this->timeSlice);

				// метод RR
				if (prSlice.getPrioritySlice())
				{
					// метод DP
					int pr = processImage->getPriority();
					processImage->setTimeSlice(prSlice.getTimeSlice(pr));
					processImage->setPriority(prSlice.getPriority(pr));
				}

				scheduler->push(Running, processImage);
				// ставим в очередь выполняемых
			}
			else
			{
				processImage->setState(Blocked);
				/* изменяем состояние процесса */
				scheduler->push(Blocked, processImage);
				// ставим в очередь блокированных
			}
		}

		/* 3. просмотреть очередь Swapped и применить метод DP
		выполняется только для метода DP */
		bool priority = prSlice.getPrioritySlice();

		if (priority)
		{ // метод DP
			size_ = scheduler->size(Swapped);

			for (int i = 0; i < size_; i++)
			{
				processImage = (ProcessImage *)(scheduler->front(Swapped));
				// 3.1. изменить приоритет процесса и TimeSlice 
				int pr = processImage->getPriority();
				processImage->setTimeSlice(prSlice.getTimeSlice(pr));
				processImage->setPriority(prSlice.getPriority(pr));
				scheduler->pop(Swapped); /* удаляем из очереди */
				scheduler->push(Swapped, processImage); // ставим в эту же очередь
			}
		}

		// подготовить работу
		if (scheduler->empty(Running) && scheduler->empty(Swapped))
		{
			cout << "dispatch: empty Running && Swapped" << endl;
			return;//нет заданий на выполнение
		}

		scheduleProcess(mmu, priority);

		/* выполнить процессы в соответствии со сформированным заданием */
		Interrupt interrupt = executeProcess(mmu);
	}

	/* выполнить процессы в соответствии со сформированным заданием */
	virtual void scheduleProcess(MMU * mmu, bool priority)
	{
		bool maxProcess = scheduler->scheduleJob(mmu, priority);
		/*задание сформировано maxProcess == true превышение MAX_PROCES,
		есть еще процессы
		maxProcess == false, все процессы вошли в задание */
	}

	/* выполнить процессы в соответствии со сформированным заданием */
	virtual Interrupt executeProcess(MMU * mmu)
	{
		return scheduler->execute(mmu);
	}

	// для управления критической секцией
	HANDLE * getHandleCS()
	{
		return (HANDLE *)cs;
	}

	void EnterCriticalSection(HANDLE* handle)
	{
		if (!cs->getCS())
		{ // ресурс не занят, для этого процесса критическая секция открыта
			((ProcessImage *)handle)->setFlag(1);
			cs->setCS(true); /* закрыть критическую секцию для других */
		}
		else {
			// закрыть критическую секцию для этого процесса 
			((ProcessImage *)handle)->setFlag(0);
		}
	}

	void LeaveCriticalSection(HANDLE* handle)
	{
		cs->setCS(false); /* открыть критическую секцию, не занят, false */
	}

	// для планирования по методу RR 
	void setTimeSlice(int timeSlice)
	{
		this->timeSlice = timeSlice;
	}

	void resetTimeSlice()
	{
		this->timeSlice = -1;
		this->prSlice.setPrioritySlice(-1, NULL);
	}

	/* для планирования по методу DP - динамических приоритетов */
	void setTimeSlice(int size, int * prSlice)
	{
		this->prSlice.setPrioritySlice(size, prSlice);
	}

	// для метода SRT и HRRN
	void setTpredict(StatTime time)
	{
		scheduler->vParam.setState(time);
	}

	void resetTpredict()
	{
		scheduler->vParam.clearVectorParam();
	}

	double getTpredict(string user, StatTime time)
	{
		return scheduler->getTpredict(user, time);
	}

	void DebugQueue(State state)
	{
		scheduler->DebugQueue(state);
	}

	void DebugVirtualMemory()
	{
		this->virtualMemory->Debug();
	}

	void DebugPCB(HANDLE* handle)
	{
		handle->ProcessTime();
		((ProcessImage *)handle)->Debug();
	}

	class Priorityslice
	{
	public:
		Priorityslice()
		{
			this->size = -1;
			this->prSlice = NULL;
		}

		void setPrioritySlice(int size, int * prSlice)
		{
			this->size = size;
			this->prSlice = prSlice;
		}

		bool getPrioritySlice()
		{
			if (prSlice == NULL)
				return false;
			return true;
		}

		int getTimeSlice(int priority)
		{
			if (size == -1)
				return -1; /* timeSlice не установлен */

			if (priority == -1)
				return prSlice[0]; // оставляем последний приоритет 

			if (priority == size - 1)
				return prSlice[size - 1];

			return prSlice[priority + 1];
		}

		int getPriority(int priority)
		{
			if (priority == -1)
				return 0;

			if (priority == size - 1)
				return size - 1;

			return priority + 1;
		}

	private:
		int size;
		int *prSlice; /* массив  приоритетов и квантов времени для планирования по методу DP */
	};

protected:
	VirtualMemory * virtualMemory;
	Scheduler* scheduler;
	MMU* mmu;
	CS* cs;	// critical section
	int timeSlice; /* квант времени для планирования по методу RR */
	Priorityslice prSlice; /* для планирования по методу DP */
};
#endif