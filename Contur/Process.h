#ifndef  __Process_H__
#define  __Process_H__

#include "HANDLE.h"
#include "ArchitectureCPU.h"

/* состояния процесса см.
State {New, Ready, Running, Blocked, ExitProcess, NotRunning);
New - Новый, для выполнения программы создается новый процесс Ready - Готовый
Running - Выполняющийся
Blocked - Блокированный
ExitProcess - Завершающийся
NotRunning - Невыполняющийся */

// реализация модели из двух состояний 
class Process : public HANDLE
{
public:
	Process() {
		this->user = "";
		state = NotRunning;
	}

	virtual ~Process() { cout << "object Process deleted" << endl; }

	string getUser() { return user; }

	void setUser(string user) { this->user = user; }

	int getID() { return HANDLE::getID(); }

	void setID(int ID) { HANDLE::setID(ID); }

	// состояние процесса
	void setState(State state) { this->state = state; }

	State getState() { return state; }

private:
	State state; // модель состояния процесса

protected:
	string user;

};

class PCB : public Process
{ // Process Control Block 

private:
	int addr;
	int virtualAddr;
	SysReg *sysReg;
	int timeSlice; /* квота времени, выделенная процессу для метода RR */
	int priority; // приоритет процесса protected:

public:
	PCB()
	{
		sysReg = NULL;
		addr = -1;			/* адрес размещения в оперативной памяти начала программы */
		virtualAddr = -1;	/* адрес размещения в виртуальной памяти */

							// Scheduling
		timeSlice = -1; 	/* квота времени, выделенная процессу для метода RR */
		priority = -1; 		/* приоритет процесса может быть изменен системой DP */
	}

	virtual ~PCB() { cout << " object РСВ deleted" << endl; }

	void setSysReg(SysReg *sysReg) { this->sysReg = sysReg; }

	SysReg * getSysReg() { return sysReg; }

	// адрес размещения в оперативной памяти 
	int getAddr() { return addr; }
	void setAddr(int addr) { this->addr = addr; }

	// адрес размещения в виртуальной памяти 
	int getVirtualAddr() { return virtualAddr; }
	void setVirtualAddr(int virtualAddr) { this->virtualAddr = virtualAddr; }

	// приоритет процесса
	int getPriority() { return priority; }
	void setPriority(int priority) { this->priority = priority; }

	// квант непрерывного времени для процесса 
	int getTimeSlice() { return timeSlice; }
	void setTimeSlice(int timeSlice) { this->timeSlice = timeSlice; }

	// получить string представление состояния 
	char *NameOf(State state) {
		const int SIZE_OF_STATE_NAME = 15;
		static char szName[SIZE_OF_STATE_NAME];
		switch (state) {
		case NotRunning:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "NotRunning"); break;
		case Running:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "Running"); break;
		case Swapped:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "Swapped"); break;
		case New:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "New"); break;
		case Ready:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "Ready"); break;
		case Blocked:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "Blocked"); break;
		case ExitProcess:
			strcpy_s(szName, SIZE_OF_STATE_NAME, "ExitProcess"); break;
		}

		return szName;
	}
};


class ProcessImage : public PCB
{
private:
	Memory * memory;
	// для выделения памяти VirtualMemory 
	bool status;
	int flag; /* доступ к ресурсу, например 1 - ресурс свободен, 0 - занят */

protected:
	friend class Dispatcher;
	friend class Scheduler;

public:
	ProcessImage()
	{
		status = true; /* Processimage в VirtualMemory свободен */ this->memory = NULL;
		flag = 1; // ресурс свободен
	}

	virtual ~ProcessImage() { cout << " object Processimage deleted" << endl; }

	void Debug()
	{
		cout << "-- begin Debug ProcessImage deleted" << endl;
		//cout << "user:\t" << user << endl;
		cout << "ID:\t" << getID() << endl;
		cout << "addr:\t" << getAddr() << endl;
		cout << "VirtualAddr:\t" << getVirtualAddr() << endl;
		cout << "State:\t" << NameOf(getState()) << endl;
		cout << "priority:\t" << getPriority() << endl;
		cout << "flag:\t" << flag << endl;
		cout << "SysReg:\t" << endl;

		SysReg * sysReg = getSysReg();
		if (sysReg == NULL)
			cout << "SysReg: NULL" << endl;
		else {
			sysReg->Debug();
		}

		cout << "Code:" << endl;

		if (memory == NULL)
			cout << "Code (memory): NULL" << endl;
		else memory->debugHeap();

		cout << "--- end Debug Processimage --" << endl;
	}

	void DebugTime()
	{
		cout << "--Processimage: DebugTime" << endl;
		cout << "ID\t= " << getID() << endl;
		cout << "Timer tick\t= " << Timer::getTime() << endl;
		cout << "TimeSlice\t= " << getTimeSlice() << endl;
		cout << "Texe\t= " << getTexec() << endl;
		cout << "Priority\t= " << getPriority() << endl;
	}

	Memory* getCode() { return memory; }

	void setCode(Memory* memory) { this->memory = memory; }

	/*статус образа процесса, используется только в классе VirtualMemory для выделения памяти: true Processimage свободен */
	void setStatus(bool status) { this->status = status; }
	bool getStatus() { return status; }

	/*флаг, для моделирования доступа и ожидания (блокирования) ресурса, требуемому процессу для выполнения */
	void setFlag(int flag) { this->flag = flag; }
	int getFlag() { return flag; }
	void clearTime() { HANDLE::clearTime(); }
};
#endif
