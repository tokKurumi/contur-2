#ifndef __HANDLE_H__
#define __HANDLE_H__

class HANDLE {
public:
	HANDLE() {
		ID = -1; /* идентификатор присваивается
				 системой, -1 - идентификатор не присвоен */
				 // process scheduling
		Tenter = -1; // Время входа
		Tbegin = -1; // Время начала
		Tservice = 0; // Время обслуживания в очередях
		Техес = rand() % (4 - 1 + 1) + 1; // Время выполнения
		Tterminate = 0; // Время завершения
	}
	virtual ~HANDLE()
	{
		cout << " object HANDLE deleted" << endl;
	}

	int getTenter() { return Tenter; }
	int getTbegin() { return Tbegin; }
	int	getTservice() { return Tservice; }
	int	getTterminate() { return Tterminate; }
	int	getTexec() { return Техес; }
	int	getTround() { return Техес + Tservice; } // Время оборота

	float getTnorm() { return (float)(Техес + Tservice) / Tservice; }

	void ProcessTime() {
		cout << "        ProcessTime ID =  " << ID << endl;
		cout << "Tenter:	" << Tenter << endl; // Время входа
		cout << "Tbegin:	" << Tbegin << endl; // Время начала
		cout << "Tservice:	" << Tservice << endl; // Время обслуживания в очередях
		cout << "Tterminate:	" << Tterminate << endl; // Время завершения
		cout << "Texec:	   " << Техес << endl; // Время выполнения
		cout << "Tround:	" << getTround() << endl; //Время оборота
		cout << "Tnorm:	" << getTnorm() << endl; // Нормализованное время
	}

private:
	int ID;
	// process scheduling
	int Tenter;	// Время входа
	int Tbegin;	// Время начала
	int Tservice;	// Время обслуживания в очередях
	int Tterminate;	// Время завершения
	int Техес;	// Время выполнения
protected:
	int getID() { return ID; }
	void setID(int ID) { this->ID = ID; } // process scheduling
	void setTenter(int Tenter) { this->Tenter = Tenter; }
	void setTbegin(int Tbegin) { this->Tbegin = Tbegin; }
	void setTservice(int Tservice) { this->Tservice = Tservice; }
	void setTterminate(int Tterminate) { this->Tterminate = Tterminate; }
	void setTexec(int Техес) { this->Техес = Техес; }
	void clearTime() { // применяется при завершении
		Tenter = -1;// Время входа
		Tbegin = -1; // Время начала
		Tservice = 0; // Время обслуживания
		Техес = 0; // Время выполнения
		Tterminate = 0; // Время завершения
	}
};

// для CriticalSection - критической секции 
class CS : HANDLE {
public:
	CS() {
		cs = false;
		// false - критическая секция не занята
		// true - критическая секция занята
	}
	virtual ~CS() { cout << " object CS deleted" << endl; }
	bool getCS() { return cs; }
	void setCS(bool cs) { this->cs = cs; }
private:
	bool cs;
protected:
};
#endif

