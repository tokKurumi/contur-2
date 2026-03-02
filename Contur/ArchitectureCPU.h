#ifndef __ArchitectureCPU_H__
#define __ArchitectureCPU_H__

#include <iostream>
using namespace std;

enum Instruction {
	Mov, Add, Sub, Mul, Div, And, Or, Xor, Shlu,
	Shru, Shls, Shrs, Cmp, Je, Jne, Jge, Jgt, Jle,
	Jlt, Int, Pushw, Pushc, Popw, Popc, Rmem, Wmem
};
/*
OK - нормальное выполнение программы
Error - ошибка в программе
Empty - чтение пустого блока памяти
Все программы на ассемблере должны завершаться прерыванием Exit
*/
enum Interrupt { OK = 0, Error = -1, Empty = 2, Exit = 16, Sys = 11, Div_0 = 3, Lan = 32, Dev = 254 };

// формат инструкции соответствует размеру блока памяти

class Block
{
public:
	/* state состояние блока занят - false,
	свободен - true */
	Block()
	{
		state = true; code = 0; nregister = 0; operand = 0;
	}
	void setState(bool state) { this->state = state; }
	bool getState() { return state; }

	void setCode(int code) { this->code = code; }
	int getCode() { return code; }

	void setNregister(int nregister) { this->nregister = nregister; }
	int getNregister() { return nregister; }

	void setOperand(int operand) { this->operand = operand; }
	int getOperand() { return operand; }

	void debugBlock()
	{
		cout << " state=" << getState()
			<< " code=" << getCode()
			<< " nregister=" << getNregister()
			<< " operand = " << getOperand() << endl;
	}

	/* формат инструкции [команда] г[номер регистра]
	[$,г][номер регистра или константа] А В С */
private:
	bool state;	     // блок занят или свободен
	_int8 code;	     // А - байт, код команды
	_int8 nregister; //В - байт, номер регистра
	int operand;     // С - int, операнд [номер регистра или константа]
};

#define NUMBER_OF_REGISTERS 16
#define SIZE_OF_REGISTER_NAMES 4 /* один символ требуется для завершения строки strcat_s */ 
enum Name { r1, r2, r3, r4, r5, гб, r7, r8, r9, rlO, rll, rl2, rl3, rl4, PC, SP };

/* Определение постификсного инкриментного operator для SysReg */
inline Name operator++ (Name &rs, int) { return rs = (Name)(rs + 1); };

/* PC программный счетчик, указывающий на содержащий адрес ячейки, по которому нужно извлечь очередную команду */

class SysReg {
public:
	SysReg()
	{
		int j = 0, i = 1; /* j регистр в массиве от 0, i - номер c 1*/
		for (Name curName = r1; curName <= SP; curName++)
		{
			register_[j++] = new Register(i++, -1, curName);
			status = true;/* регистры не используются процессом, для планирования */
		}
	}

	int getState(Name name)
	{
		for (int i = 0; i < NUMBER_OF_REGISTERS; i++)
		{
			if (register_[i]->getName() == name)
			{
				return (register_[i]->getState());
			}
		}
		return -1; //some system Error
	}

	void setState(_int8 nregister, int operand)
	{
		// данные заносятся в таком виде
		for (int i = 0; i < NUMBER_OF_REGISTERS; i++)
		{
			if (register_[i]->getName() == nregister)
			{
				register_[i]->setState(operand);
				return;
			}
		}
	}
	/* очистить содержимое регистров после окончания программы */
	void clearSysReg()
	{
		for (int i = 0; i < NUMBER_OF_REGISTERS; i++)
		{
			register_[i]->setState(-1);
		}
	}

	void Debug()
	{ // информация о регистрах
		for (int i = 0; i < NUMBER_OF_REGISTERS; i++)
			cout << "register_[" << i << "] " << " numreg= " <<
			register_[i]->getNumReg() << " state = " <<
			register_[i]->getState() << " name = " <<
			register_[i]->getName() << " NameOf= " <<
			register_[i]->NameOf() << endl;
		cout << endl;
	}

	class Register
	{
	public:
		/* объявление двух конструкторов: по умолчанию и конструктора,
		который устанавливает состояние и значение имени регистра */
		Register() {};

		Register(int numreg, int state, Name name)
		{
			this->numreg = numreg;
			this->state = state;
			this->name = name;
		};

		/* get and set functions.
		numReg - внутреннее представление номера регистра,
		например, для r1 numReg = 1, согласовано с именем */

		int	getNumReg() { return numreg; }; /* получить rардинальное число */
		void setNumReg(int numreg) { this->numreg = numreg; }; // установить кардинальное число 
		int getState() { return state; }; /* получить состояние регистра */
		void setState(int state) { this->state = state; }; // установить состояние регистра

		Name getName() { return name; }; /* получить code регистра в enum */
		void setName(Name name) { this->name = name; };	// установить code регистра в enum */

		char* NameOf() {
			/* получить string представление регистра */
			static char szName[SIZE_OF_REGISTER_NAMES];
			const char* Numbers[] = { "1" , "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14" }; /////////// char изменил на const

			if (getName() <= 13)
			{
				strcpy_s(szName, SIZE_OF_REGISTER_NAMES, "r");
				strcat_s(szName, SIZE_OF_REGISTER_NAMES, Numbers[getName()]);
				return szName;
			}

			switch (getName())
			{
			case PC:
				strcpy_s(szName, SIZE_OF_REGISTER_NAMES, "PC");
				break;

			case SP:
				strcpy_s(szName, SIZE_OF_REGISTER_NAMES, "SP");
				break;
			}
			return szName;
		}
	private:
		int numreg;
		Name name;
		int state;
	}; // end Register


	bool getStatus() { return status; }
	void setStatus(bool status) { this->status = status; }

private:
	Register * register_[NUMBER_OF_REGISTERS];
	bool status;
	/* true - регистры не используются процессом
	false - регистры использует процесс */
}; // end SysReg

   // состояния процесса
#define NUMBER_OF_STATE 7
enum State { NotRunning = 0, Running = 1, Blocked = 2, Swapped = 3, ExitProcess = 4, New = 5, Ready = 6 };

/* New - Новый, для выполнения программы создается новый процесс (не используется)
Ready - Готовый (не используется)
Running - Выполняющийся
Blocked - Блокированный
ExitProcess - Завершающийся
Swapped - Выгруженный, готовый к выполнению
NotRunning - Невыполняющийся */

class Timer
{
public:
	Timer() { time = 0; }
	static void setTime() { time = 0; }
	static int getTime() { return time; }
	static void Tick() { time++; }
private:
	/* на операции диспетчирования затрачивается время, на CPU на обслуживание - ожидание и блокирование */
	static int time;
};

int Timer::time;
/* при выполнении функций класса Statistic, задается один из параметров расчета
TimeExec - предполагаемое время выполнения
TimeServ - предполагаемое время обслуживания
значения StatTime присваиваются в соответствии с
индексами массива, что применяется при просмотре вектора */
enum StatTime { TimeExecNotPreem = 0, TimeExec = 1, TimeServ = 2, noParam = -1 };

#define NUMBER_OF_PARAM 3

/* предположительное время выполнения вычисляется
методом getShortestRemainTime, который вызывается
объектом класса Dispatcher после первого наблюдения */

class VectorParam
{
	/* вектор параметров расчета, применяется в Scheduler */
public:
	VectorParam()
	{
		for (int i = 0; i < NUMBER_OF_PARAM; i++)
		{
			param[i] = new Param();
		}
	} //

	void setState(StatTime time) { param[time]->state = true; }
	bool getState(StatTime time) { return param[time]->state; }

	StatTime getStateTime()
	{
		for (int i = 0; i < NUMBER_OF_PARAM; i++)
		{
			if (param[i]->state)
			{
				if (i == 0)	return	TimeExecNotPreem;
				if (i == 1)	return	TimeExec;
				if (i == 2)	return	TimeServ;
			}
		}
		return noParam;
	} //

	void resetState(StatTime time)
	{
		param[time]->state = false;
	}

	void setTthreshold(StatTime time, double Tthreshold)
	{
		param[time]->Tthreshold = Tthreshold;
	}

	double getTthreshold(StatTime time)
	{
		return param[time]->Tthreshold;
	}

	void clearVectorParam()
	{
		for (int i = 0; i < NUMBER_OF_PARAM; i++)
		{
			param[i]->state = false;
			param[i]->Tthreshold = -1;
		}
	}

private:
	class Param
	{
	public:
		Param() { Tthreshold = -1; state = false; }
		bool state; /* индикатор включения накопления времени */
		double Tthreshold; /* порог предположительного времени */
	};
	Param* param[NUMBER_OF_PARAM];
};
#endif