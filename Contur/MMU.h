#pragma once
#include "Memory.h"
#include "Process.h"

class MMU
{
private:
	Memory * memory;

public:
	MMU(Memory &memory) { this->memory = &memory; } /* загрузка программы из образа процесса в основную память */

	virtual void swapIn(ProcessImage * processImage)
	{ // получить программу из образа процесса 
		Memory * code = processImage->getCode(); /* получить адрес размещения программы в оперативной памяти, если он был */
		int addrPCB = processImage->getAddr();
		int addrReal = getRealAddr(); /* адрес размещения программы */

		if (addrPCB == -1)
		{ //не присвоен
		  /* если PC = -1, то PC не проинициализирован присвоить начало программы */
			processImage->getSysReg()->setState(PC, addrReal);
		}
		else
		{
			/* размещение программ в памяти 1. определить смещение PC - addrPCB, изменить адрес PC */
			int addrOffPC = processImage->getSysReg()->getState(PC) - addrPCB;
			processImage->getSysReg()->setState(PC, addrOffPC + addrReal);
		}

		// запомнить адрес в блоке PCB 
		processImage->setAddr(addrReal);

		/* установить адрес размещения программы в оперативной памяти */
		setAlloc(addrReal);

		// прочитать блок кода, начало программы в образе 
		Block * block = NULL;
		for (int i = 0; i < code->getSizeMemory(); i++)
		{
			block = code->read(i);
			/* код программы читается с 0 адреса */
			/* записать блок в память только последовательно, на незанятые блоки */
			if (!block->getState())
			{
				memory->setCmd(block->getCode(), block->getNregister(), block->getOperand());
			}
		}
	}

	/* выгрузка программы из основной памяти, применяется в алгоритмах вытеснения */
	virtual void swapOut(ProcessImage * processImage)
	{
		/* получить адрес размещения программы в оперативной памяти из РСВ */
		int AddrFrom = processImage->getAddr();
		int AddrTo = AddrFrom + processImage->getCode()->getSizeMemory() - 1;
		memory->clearMemory(AddrFrom, AddrTo);
	}

	/* определить свободный адрес для загрузки программы ограничения, проверяется только один блок, остальные считаются свободными - это не всегда так */
	virtual int getRealAddr() { return this->memory->getAddrFreeBlock(); }

	// установить адрес размещения программы в памяти 
	virtual void setAlloc(int addr) { memory->setAddrBlock(addr); }

	// показать выполнение swapIn
	virtual void Debug(HANDLE * handle) {
		// получить программу из образа процесса 
		ProcessImage * processImage = (ProcessImage *)handle;

		// см. void swapin(Processimage * processimage) 
		Memory * code = processImage->getCode();

		/* получить адрес размещения программы в оперативной памяти, если он был */
		cout << endl;
		cout << "-get address of loading programm in memory & set PC" << endl;

		int addrPCB = processImage->getAddr();
		cout << "addrPCB = " << addrPCB << " address of loading programm from PCB" << endl;

		int addrReal = getRealAddr();
		/* адрес размещения программы */
		cout << "addrReal= " << addrReal << " address of loading programm Real" << endl;

		if (addrPCB == -1) {/* не присвоен, если PC = -1,то PC не проинициаливирован, присвоить начало программы */
			cout << " if address in PCB == -1 set addrReal PC = " << addrReal << endl;
		}
		else {	/* размещение программ в памяти 1. определить смещение PC - addrPCB и изменить адрес PC */
			cout << "PC = " << processImage->getSysReg()->getState(PC) << " address in PC " << endl;
			int addrOffPC = processImage->getSysReg()->getState(PC) - addrPCB;
			cout << "addrOffPC = PC - addrPCB = " << addrOffPC << " address off set" << endl;
			cout << "PC = addrOffPC + addrReal = " << addrOffPC + addrReal << endl;
		}

		// прочитать блок кода, начало программы в образе 
		Block * block = NULL;
		cout << endl;
		cout << "read block from process image code to memory" << endl;
		cout << "size code = " << code->getSizeMemory() << endl;
		for (int i = 0; i < code->getSizeMemory(); i++) {
			block = code->read(i); /* код программы читается с 0 адреса, записать блок в память только последовательно, на незанятые блоки */
			if (!block->getState()) {
				block->debugBlock();
			}
		}
	}

	virtual void DebugMemory() { memory->debugHeap(); }
};
