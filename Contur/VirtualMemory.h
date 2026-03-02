#pragma once
#ifndef __VirtualMemory_H__ 
#define __VirtualMemory_H__

#include "ArchitectureCPU.h"
#include "Memory.h"
#include "Process.h"
#include <iostream> 

using namespace std;

class VirtualMemory
{
private:
	int SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES;
	int currImage;
	ProcessImage* image;

public:
	VirtualMemory(int SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES)
	{
		this->SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES = SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES;
		image = new ProcessImage[SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES];
		currImage = 0;
	}
	/* примен€етс€ при считывании инструкций функци€ устанавливает начальный адрес дл€ последовательной записи */
	virtual void setAddrImage(int addr)
	{
		currImage = addr;
	}

	// получить по установленному адресу образ процесса 
	ProcessImage * getImage()
	{
		return &image[currImage];
	}

	/* примен€етс€ при инициализации образа
	по установленному адресу, записать в виртуальную пам€ть программу */
	virtual void setMemory(Memory* memory)
	{
		image[currImage].setStatus(false); // образ зан€т 
		image[currImage].setCode(memory);
	}

	/* read примен€етс€ при организации свопинга: из виртуальной пам€ти в оперативную */
	virtual Memory* read(int addr)
	{
		return image[addr].getCode();
	}

	/* получить индекс следующего образа, если нет образа, то Error, -1 использовать функции дл€ управлени€ пам€тью */
	virtual int getAddrFreeImage()
	{
		/* выбираетс€ первый свободный образ при просмотре */
		for (int i = 0; i < SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES; i++)
			if (image[i].getStatus())
			{
				// true VirtualMemory образ свободен 
				image[i].setStatus(false);
				return i; // false зан€т
			}
		return -1; // нет свободных образов
	}
	/* при удалении программы из системы: освободить virtualMemory, инициализировать –ќ¬ */
	virtual void clearImage(int addr)
	{
		image[addr].setStatus(true);
		image[addr].setCode(NULL);
		image[addr].setAddr(-1);
		image[addr].setID(-1);
		image[addr].setPriority(-1);
		image[addr].setTimeSlice(-1);
		image[addr].setState(NotRunning);
		image[addr].setSysReg(NULL);
		image[addr].setUser("");
		image[addr].setVirtualAddr(-1);
		image[addr].clearTime();
		image[addr].setTimeSlice(-1);
	}

	virtual void clearMemory()
	{
		for (int i = 0; i < SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES; i++)
			clearImage(i);
		currImage = 0;
	}
	/* можно посмотреть содержимое образа процесса в виртуальной пам€ти */
	virtual void Debug()
	{
		for (int i = 0; i < SIZE_OF_VIRTUAL_MEMORY_IN_IMAGES; i++)
		{
			cout << " VertualAddress = " << i << " status = " << image[i].getStatus() << " ID = " << image[i].getID() << endl;
		}
	}
};
#endif