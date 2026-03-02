#include "contur/process.h"
#include "contur/memory.h"
#include <iostream>

// Process

Process::Process() : user(""), state(NotRunning)
{
}

Process::~Process()
{
    std::cout << "object Process deleted" << '\n';
}

// PCB

PCB::PCB() : sysReg(nullptr), addr(-1), virtualAddr(-1), timeSlice(-1), priority(-1)
{
}

PCB::~PCB()
{
    std::cout << " object PCB deleted" << '\n';
}

std::string PCB::NameOf(State state)
{
    switch (state) {
    case NotRunning:
        return "NotRunning";
    case Running:
        return "Running";
    case Swapped:
        return "Swapped";
    case New:
        return "New";
    case Ready:
        return "Ready";
    case Blocked:
        return "Blocked";
    case ExitProcess:
        return "ExitProcess";
    default:
        return "";
    }
}

// ProcessImage

ProcessImage::ProcessImage() : status(true), memory(nullptr), flag(1)
{
}

ProcessImage::~ProcessImage()
{
    std::cout << " object ProcessImage deleted" << '\n';
}

void ProcessImage::Debug()
{
    std::cout << "-- begin Debug ProcessImage" << '\n';
    std::cout << "ID:\t" << getID() << '\n';
    std::cout << "addr:\t" << getAddr() << '\n';
    std::cout << "VirtualAddr:\t" << getVirtualAddr() << '\n';
    std::cout << "State:\t" << NameOf(getState()) << '\n';
    std::cout << "priority:\t" << getPriority() << '\n';
    std::cout << "flag:\t" << flag << '\n';
    std::cout << "SysReg:\t" << '\n';

    SysReg *sysReg = getSysReg();
    if (sysReg == nullptr) {
        std::cout << "SysReg: NULL" << '\n';
    } else {
        sysReg->Debug();
    }

    std::cout << "Code:" << '\n';
    if (memory == nullptr) {
        std::cout << "Code (memory): NULL" << '\n';
    } else {
        memory->debugHeap();
    }

    std::cout << "--- end Debug Processimage --" << '\n';
}

void ProcessImage::DebugTime()
{
    std::cout << "--Processimage: DebugTime" << '\n';
    std::cout << "ID\t= " << getID() << '\n';
    std::cout << "Timer tick\t= " << Timer::getTime() << '\n';
    std::cout << "TimeSlice\t= " << getTimeSlice() << '\n';
    std::cout << "Texe\t= " << getTexec() << '\n';
    std::cout << "Priority\t= " << getPriority() << '\n';
}
