#pragma warning (disable:4267)
#ifndef  __Scheduler_H__
#define  __Scheduler_H__

#include "ArchitectureCPU.h" 
#include "Process.h"
#include "Statistic.h"
#include "CPU.h"
#include "MMU.h"
#include <queue>
#include <unordered_map>
#include <valarray>
#include <iostream>

using namespace stdext; // for hash_map 
//using namespace System; // for Math:: 

/* Задание для выполнения процессором CPU программ в многозадачном режиме */

class Job
{
private:
	int addr;// начальный адресе выполнения программы 
	int id; // идентификатор процесса
	ProcessImage * processImage; // образ процесса
	bool done; // индикатор выполнения задания 

public:
	Job()
	{ // конструктор по умолчанию 
		this->addr = -1;
		this->id = -1;
		this->done = false; // выполнен 
		this->processImage = NULL; /* указатель на образ процесса */
	}

	void set(int addr, int id, bool done)
	{
		this->addr = addr;
		this->id = id;
		this->done = done;
	}

	void set(int addr, int id, bool done, ProcessImage * processImage)
	{
		this->addr = addr;
		this->id = id;
		this->done = done;
		this->processImage = processImage;
	}

	void setAddr(int addr) { this->addr = addr; }
	int getAddr() { return addr; }

	void setId(int id) { this->id = id; }
	int getId() { return id; }

	void setDone(bool done) { this->done = done; }
	bool getDone() { return done; }

	void setProcessImage(ProcessImage * processImage)
	{
		this->processImage = processImage;
	}

	ProcessImage * getProcessImage()
	{
		return processImage;
	}

	void Debug()
	{
		cout << " job: addr = " << addr << " id = " << id << " done = " << done << endl;
	}
};

class Scheduler
{
private:
	CPU * cpu;
	int MAX_PROCESS;
	Job* job;
	SysReg *sysreg;
	int sliceCPU;

	/* очереди процессов в соответствии с их состояниями The queue class supports a first-in, first-service (FIFS, FIFO) method */
	queue <Process*> processQueue[NUMBER_OF_STATE];
	Statistic * statistic;

	// статистический расчет 
	/* распределение процессов по процессорам в методе Multi Processing */
	int quotaProcess;
	int j; // используются в подготовке задания 

		   // вытеснение процесса: метод RR и SRT, HRRN 
	void preemptive(ProcessImage * processImage, MMU * mmu)
	{
		ProcessImage * processImage_w = NULL;
		int size_ = size(Running);
		for (int i = 0; i < size_; i++)
		{
			processImage_w = (ProcessImage *)(front(Running));
			if (processImage_w == processImage)
			{
				pop(Running);
				// 1. поставить процесс в очередь Swapped 
				push(Swapped, processImage);
				// 2. освободить память swap, Занятую программой 
				mmu->swapOut(processImage);
				// 3. увеличить время обслуживания 
				processImage->setTservice(processImage->getTservice() + 1);
			}
			else {
				pop(Running);
				push(Running, processImage_w);
			}
		}
	}

public:
	// вектор параметров для планирования 
	VectorParam vParam;
	Scheduler(CPU *cpu, int MAX_PROCESS)
	{
		this->cpu = cpu; 	/* каждый процесс виртуально владеет своими регистрами */
		this->MAX_PROCESS = MAX_PROCESS;
		/* в многозадачном режиме, после прерывания для каждого процесса сохраняется состояние регистров, т .е . каждый процесс виртуально владеет своими регистрами */
		sysreg = new SysReg[MAX_PROCESS];
		sliceCPU = 20; /* slice (квота), при которой полностью выполняются программы не превышающие 20 блоков/строк в многозадачном режиме */
		job = NULL; // очередь заданий пуста 
		this->statistic = new Statistic;
		// метод Multi Processing 
		this->quotaProcess = -1;
		this->j = 0;
	}

	// конструктор прототиптрования 
	Scheduler(Scheduler *scheduler)
	{
		this->cpu = scheduler->cpu;
		this->MAX_PROCESS = scheduler->MAX_PROCESS;
		this->sysreg = scheduler->sysreg;
		this->sliceCPU = scheduler->sliceCPU;
		this->job = scheduler->job;
		this->statistic = scheduler->statistic;
	}

	Interrupt execute(Job *job, MMU * mmu)
	{
		Interrupt interrupt; /* OK, Error, Exit = 16, 		Sys, Div_0, Timer, Lan = 32, Dev = 254 */
		ProcessImage * processImage = NULL;

		//установить PC для процессов 
		for (int i = 0; i < MAX_PROCESS; i++)
		{
			/* -1 задание отсутствует, т.е. реальных процессов меньше, чем MAX_PROCESS */
			if (job[i].getAddr() != -1)
			{
				processImage = job[i].getProcessImage();

				if (processImage == NULL)
				{ /* задание без ProcessImage, lab 1, 2 */
					sysreg[job[i].getId()].setState(PC, job[i].getAddr());
				}
				// начало программы в памяти
			}
		}

		while (true)
		{
			// увеличить счетчик времени на цикл CPU
			Timer::Tick();
			for (int i = 0; i < MAX_PROCESS; i++)
			{
				// -1 задание отсутствует 
				if (job[i].getAddr() == -1) continue;

				// ресурсы процесса 
				if (job[i].getDone())
				{
					processImage = job[i].getProcessImage();
					// задание не c ProcessImage 
					if (processImage == NULL) {
						cout << "Process number " << job[i].getId();
						interrupt = cpu->exeInstr(job[i].getAddr(), &sysreg[job[i].getId()]);
					}
					else if (processImage != NULL)
					{/* задание c ProcessImage */
					 // -1 по умолчанию 
						if (processImage->getTbegin() < 0)
							processImage->setTbegin(Timer::getTime());

						int AddrPC = processImage->getSysReg()->getState(PC);

						interrupt = cpu->exeInstr(AddrPC, &sysreg[job[i].getId()]);

						// увеличить Texe 
						processImage->setTexec(processImage->getTexec() + 1); // время выполнения 

																			  /* метод RR только если установлена квота. По умолчанию -1. То же, если установлен массив приоритетов с квотой DP */
						if (processImage->getTimeSlice() > 0)
						{
							// время выполнения процесса соответствует квоте 
							if (processImage->getTexec() % processImage->getTimeSlice() == 0)
							{
								// 1. убрать процесс из задания 
								job[i].setDone(false);
								// 2. вытеснить процесс 
								preemptive(processImage, mmu);
							}
						}

						/* выполнение вытеснения процесса по установленным параметрам, каждому параметру соответствует метод, если несколько параметров, то первый в векторе SRT,HRRN */
						paramPreemptive(processImage, mmu);
					}

					if (interrupt == Exit)
					{ // процесс завершен
						job[i].setDone(false); //задание выполнено 
						processImage = job[i].getProcessImage();

						// задание c ProcessImage 
						if (processImage != NULL)
						{
							//время завершения 
							processImage->setTterminate(Timer::getTime() + 1);

							// очистить память 
							mmu->swapOut(processImage);
							/* удаляем процесс из очереди Running и ставим в очередь ExitProcess в следующем вызове dispatch() отмечаем процесс как завершенный, информация о процессе в РСВ */
							processImage->setState(ExitProcess);
						}
						// +1, еще будет находиться в очереди 
					}

					if ((interrupt == Error) || (interrupt == Empty))
					{
						cout << "interrupt == " << interrupt << endl; 	// результат не предсказуем; 
						job[i].Debug();
						return interrupt;
					}
				}
			}

			// все процессы завершены 
			if (!isJob(job)) return OK;
		}
		return OK;
	}

	// проверка есть ли в задании процесс для выполнения 
	bool isJob(Job *job)
	{
		bool isJob = false;
		for (int i = 0; i < MAX_PROCESS; i++)
		{
			// -1 задание отсутствует 
			if (job[i].getAddr() == -1) continue;
			if (job[i].getDone()) isJob = true;
		}
		return isJob;
	}

	Interrupt execute(int addr, int id, CPU *cpu)
	{
		// id идентификатор процесса 
		// цикл исполнения программы CPU 
		Interrupt interrupt;

		// начало программы в памяти 
		sysreg[id].setState(PC, addr);
		SysReg *sysreg_ = &sysreg[id];

		for (int i = 0; i < sliceCPU; i++)
		{
			interrupt = cpu->exeInstr(addr, sysreg_);

			if ((interrupt == Exit) || (interrupt == Error) || (interrupt == Empty))
				return interrupt;
		} // end while 

		return interrupt;
	}// end execute

	bool scheduleJob(MMU* mmu, bool priority)
	{
		/* подготовить задания на выполнение, если не пустая очередь */
		DebugSPNQueue();
		// сформировать задание job для очереди Running 
		// моделирование многозадачного режима 
		job = new Job[MAX_PROCESS];
		ProcessImage * processImage = NULL;
		/* формируем задание для процессов < MAX_PROCESS

		1. из очерели Swapped ставим в очередь Running */
		int size_ = size(Swapped);

		for (int i = 0; i < size_; i++)
		{
			processImage = (ProcessImage *)(front(Swapped));
			pop(Swapped);

			// 1.2. увеличить время обслуживания 
			processImage->setTservice(processImage->getTservice() + 1);
			push(Running, processImage);
		}

		// 2. сортируем 
		if (vParam.getStateTime() != noParam)
		{
			if (vParam.getStateTime() == TimeExecNotPreem)
				// по времени выполнения 
				ProcessNext(TimeExec);
			else
				ProcessNext(vParam.getStateTime());
		}

		// упорядочить очередь по приоритету 
		if (priority)
		{// метод DP 
		 // без параметров предсказуемого времени 
			ProcessNext(noParam);
		}

		/* подготовить задание и загрузить программы в память maxProcess == true превышение MAX_PROCESS, есть еще процессы maxProcess == false, все процессы вошли в задание */
		size_ = size(Running);

		if (quotaProcess == -1)
		{
			j = 0;

			for (int i = j; i < size_; i++)
			{
				if (j > MAX_PROCESS - 1)
					return true;

				processImage = (ProcessImage *)(front(Running));

				mmu->swapIn(processImage);
				job[j].set(processImage->getAddr(),
					processImage->getID(), true, processImage);
				pop(Running);
				push(Running, processImage);
				j++;
			}
			return false;
		}
		else
		{ // для многопроцессорного метода 
			int k = j; // начало выдачи квоты 

			for (int i = j; i < size_; i++)
			{
				if (j > MAX_PROCESS - 1)
					return false; // нет задач 
				if (j > quotaProcess - 1)
					return true; //квота дана 

				processImage = (ProcessImage *)(front(Running));
				mmu->swapIn(processImage);
				job[j].set(processImage->getAddr(), processImage->getID(), true, processImage);
				pop(Running);
				push(Running, processImage);
				j++;
			}

			j = 0; // задача выдана 
			return false; // квота выдана, нет больше задач
		}
	} // end schedulejob 

	  // выполнить задание 
	Interrupt execute(MMU * mmu)
	{
		return execute(job, mmu);
	}

	// применяется при многопроцессорном планировании 
	// Multiprocessor scheduling (MP) 
	void setJob(Job *job) { this->job = job; }
	Job * getJob() { return this->job; }

	/*установить квоту для CPU, т.e.сколько циклов выполнит CPU.Применяется для моделирования многозадачного режима и передачи управления */
	void setSliceCPU(int sliceCPU) { this->sliceCPU = sliceCPU; }

	/* получить идентификатор процесса, идентификатор процесса соответствует незанятому индексу в пуле регистров */
	int getID()
	{
		for (int i = 0; i < MAX_PROCESS; i++)
		{
			if (sysreg[i].getStatus())
				return i;
		}

		return -1; // все регистры заняты
	} //

	SysReg * getSysReg(int ID)
	{
		return &sysreg[ID];
	}

	// трассировка состояния регистров для одного блока void DebugBlock(int id, CPU *cpu){ cpu->Debug(&sysreg[id]);
	void DebugBlock(int id, CPU *cpu)
	{
		cpu->Debug(&sysreg[id]);
	}

	// трассировка состояния регистров 
	void DebugSysReg(int id)
	{
		sysreg[id].Debug();
	}

	void push(State state, ProcessImage * processImage)
	{
		processQueue[state].push(processImage);
	}

	Process* pop(State state)
	{
		if (processQueue[state].empty())
			return NULL;
		Process* process = processQueue[state].front();
		// удалить элемент из очереди первый 
		processQueue[state].pop();
		return process;
	}

	void pop(State state, ProcessImage * processImage)
	{
		ProcessImage * processImage_ = NULL;
		for (int i = 0; i < (int)processQueue[state].size(); i++)
		{
			processImage_ = (ProcessImage *)processQueue[state].front();
			// удалить элемент из очереди первый 
			processQueue[state].pop();
			if (processImage_ != processImage)
				processQueue[state].push(processImage);
		} /* перебираем до конца очереди, чтобы сохранить ее порядок */
	}

	Process * front(State state)
	{
		return processQueue[state].front();
	}

	bool empty(State state)
	{
		return processQueue[state].empty();
	}

	int size(State state)
	{
		return processQueue[state].size();
	}

	void setObservation(ProcessImage * processImage)
	{
		statistic->setObservation(processImage->getUser(), processImage->getTexec(), processImage->getTservice());
	}

	void clearTpredict()
	{
		statistic->clearTpredict();
	}

	double getTpredict(string user, StatTime time)
	{
		return statistic->getTpredict(user, time);
	}

	/* упорядочить очереди по предсказанному времени, в соответствии с параметром */
	void ProcessNext(StatTime time)
	{
		//1. Running 
		sortQueue(Running, time);
		//2. Blocked 
		sortQueue(Blocked, time);
	}

	void sortQueue(State state, StatTime time)
	{
		int size_ = size(state);
		double wk = 0;
		// представляем очередь как массив 
		valarray<ProcessImage*> vaProcess(size_);
		/* для сортировки очереди процессов по времени */
		valarray<double> va(size_);
		ProcessImage * processImage = NULL;

		for (int i = 0; i < size_; i++)
		{
			processImage = (ProcessImage *)(front(state));
			vaProcess[i] = processImage
				;
			/* упорядочивать по наименьшему оставшемуся времени, не надо, т.к. метод FCFS */
			if (time == noParam) // метод DP 
				va[i] = processImage->getPriority();
			else
				va[i] = statistic->getTpredict(vaProcess[i]->getUser(), time);

			pop(state);
			push(state, processImage);
		}

		// сортировка простым выбором 
		for (int i = 0; i < size_ - 1; i++)
		{
			// просмотр массива, начиная со второго элемента 
			for (int j = i + 1; j < size_; j++)
			{
				/* 1. выбирается наименьший элемент по сравнению с первым */
				if (va[j] < va[i])
				{
					// 2. он меняется местами с первым элементом swap 
					wk = va[j];
					processImage = vaProcess[j];
					va[j] = va[i];
					vaProcess[j] = vaProcess[i];
					va[i] = wk;
					vaProcess[i] = processImage;
				}
			} // end for 				  // 3. изменяется первый элемент, идем вправо 
		} // end for 

		for (int i = 0; i < size_; i++)
		{
			pop(state);
			// удаляем из очереди state 
			// ставим упорядоченную очередь после сортировки 
			push(state, vaProcess[i]);
		}
	} // end sortQueue 

	void setTthreshold(StatTime time)
	{
		vParam.setTthreshold(time, statistic->getTimeThreshold(time));
	}

	// выполнение процесса по методу SRT и HRRN 
	void methodPreemptive(ProcessImage * ProcessImage, MMU * mmu, StatTime time) 
	{
		double Tpredict = statistic->
			getTpredict(ProcessImage->getUser(), time);
		if (Tpredict == -1)
			return; // нет наблюдений 
		if ((Tpredict - ProcessImage->getTexec()) > vParam.getTthreshold(time))
			return;
		/* 1. иначе убрать все процессы из задания, кроме данного */
		for (int i = 0; i < MAX_PROCESS; i++) {
			/* -1 задание отсутствует, т.е. реальных процессов меньше, чем MAX_PROCESS */
			if (job[i].getAddr() != -1) {
				if (job[i].getProcessImage()->getID() != ProcessImage->getID()) {
					job[i].setDone(false);
					/* 1.1. убрать процесс из задания
					1.2. вытеснить все процессы, оставить текущий ProcessImage */
					preemptive(job[i].getProcessImage(), mmu);
				}
			}
		}
	}

	// вытеснение в соответствии с заданными параметрами 
	void paramPreemptive(ProcessImage *processImage, MMU * mmu) 
	{
		// нет вытеснения SPN 
		if (vParam.getState(TimeExecNotPreem))
			return;
		// вытеснение процесса согласно параметру 
		if (vParam.getStateTime() != noParam)
			methodPreemptive(processImage, mmu, vParam.getStateTime());
	}

	int getProcess() 
	{
		return MAX_PROCESS;
	}

	void setQuotaProcess(int quotaProcess) 
	{
		this->quotaProcess = quotaProcess;
	}

	void DebugQueue(State state) 
	{
		bool empty_ = empty(state);
		if (empty_) {
			cout << " queue is empty " << state << endl;
			return;
		}

		int size = processQueue[state].size();
		if (size == 0)
			return;
		ProcessImage * processImage = (ProcessImage *)processQueue[state].front();
		cout << endl;
		cout << "Queue " << processImage->NameOf(state) << " size: " << size << endl;
		cout << "ID = " << processImage->getID()<<endl;
		cout << "PC = " << processImage->getSysReg()->getState(PC) << endl;
		cout << "ProcessImage" << endl;
		processImage->Debug();
		cout << endl;
	}

	void DebugSPNQueue(State state) 
	{
		if (empty(state)) {
			cout << " is empty " << endl;
			return;
		}
		int size_ = size(state);
		ProcessImage * processImage = NULL;

		for (int i = 0; i < size_; i++) {
			processImage = (ProcessImage *)(front(state));
			cout << endl;
			cout << "user = "<< processImage->getUser() << endl;

			if (vParam.getState(TimeExecNotPreem) || vParam.getState(TimeExec)) {
				cout << "TpredictExec = " << statistic->getTpredict(processImage->getUser(), TimeExec) << endl;
			}

			cout << "Texec = " << processImage->getTexec() << endl;
			if (vParam.getState(TimeServ)) {
				cout << "TpredictServ = " << statistic->getTpredict(processImage->getUser(), TimeServ) << endl;
				cout << "Tserv = " << processImage->getTservice() << endl;
			}

			cout << "Queue state = " << processImage->getState() << endl;
			cout << "Priority = " << processImage->getPriority() << endl;
			cout << "TimeSlice = " << processImage->getTimeSlice() << endl;

			pop(state); push(state, processImage);
		}
	} // end DebugsPNQueue 

	void DebugSPNQueue() 
	{
		cout << "Method : queue order " << endl;
		if (vParam.getState(TimeExec)) // метода SRT 
			cout << "Tthreshold SRT = " << vParam.getTthreshold(TimeExec) << endl;

		if (vParam.getState(TimeServ)) // метода SRT 
			cout << "Tthreshold HRRN = " << vParam.getTthreshold(TimeServ) << endl;

		cout << " queue Running " << endl;
		DebugSPNQueue(Running);

		cout << " queue Swapped " << endl;
		DebugSPNQueue(Swapped);

		cout << " queue Blocked " << endl;
		DebugSPNQueue(Blocked);

		cout << " queue ExitProcess " << endl;
		DebugSPNQueue(ExitProcess);
	}
};
#endif