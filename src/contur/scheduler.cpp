#include "contur/scheduler.h"
#include <iostream>
#include <valarray>

// Job

Job::Job() : addr(-1), id(-1), done(false), processImage(nullptr)
{
}

void Job::set(int addr, int id, bool done)
{
    this->addr = addr;
    this->id = id;
    this->done = done;
}

void Job::set(int addr, int id, bool done, ProcessImage *processImage)
{
    this->addr = addr;
    this->id = id;
    this->done = done;
    this->processImage = processImage;
}

void Job::Debug()
{
    std::cout << " job: addr = " << addr << " id = " << id << " done = " << done << '\n';
}

// Scheduler

Scheduler::Scheduler(CPU &cpu, int MAX_PROCESS)
    : cpu(cpu), MAX_PROCESS(MAX_PROCESS), sliceCPU(20), job(nullptr), quotaProcess(-1), j(0), sysreg(MAX_PROCESS)
{
    statistic = std::make_shared<Statistic>();
}

Scheduler::Scheduler(Scheduler &scheduler)
    : cpu(scheduler.cpu),
      MAX_PROCESS(scheduler.MAX_PROCESS),
      sysreg(scheduler.sysreg),
      sliceCPU(scheduler.sliceCPU),
      job(scheduler.job),
      statistic(scheduler.statistic),
      quotaProcess(-1),
      j(0)
{
}

void Scheduler::preemptive(ProcessImage &processImage, MMU &mmu)
{
    ProcessImage *processImage_w = nullptr;
    int size_ = size(Running);
    for (int i = 0; i < size_; i++) {
        processImage_w = static_cast<ProcessImage *>(front(Running));
        if (processImage_w == &processImage) {
            pop(Running);
            push(Swapped, processImage);
            mmu.swapOut(processImage);
            processImage.setTservice(processImage.getTservice() + 1);
        } else {
            pop(Running);
            push(Running, *processImage_w);
        }
    }
}

Interrupt Scheduler::execute(Job *job, MMU &mmu)
{
    Interrupt interrupt;
    ProcessImage *processImage = nullptr;

    for (int i = 0; i < MAX_PROCESS; i++) {
        if (job[i].getAddr() != -1) {
            processImage = job[i].getProcessImage();
            if (processImage == nullptr) {
                sysreg[job[i].getId()].setState(PC, job[i].getAddr());
            }
        }
    }

    while (true) {
        Timer::Tick();
        for (int i = 0; i < MAX_PROCESS; i++) {
            if (job[i].getAddr() == -1) {
                continue;
            }

            if (job[i].getDone()) {
                processImage = job[i].getProcessImage();
                if (processImage == nullptr) {
                    std::cout << "Process number " << job[i].getId();
                    interrupt = cpu.exeInstr(job[i].getAddr(), sysreg[job[i].getId()]);
                } else if (processImage != nullptr) {
                    if (processImage->getTbegin() < 0) {
                        processImage->setTbegin(Timer::getTime());
                    }

                    int AddrPC = processImage->getSysReg()->getState(PC);
                    interrupt = cpu.exeInstr(AddrPC, *processImage->getSysReg());

                    processImage->setTexec(processImage->getTexec() + 1);

                    if (processImage->getTimeSlice() > 0) {
                        if (processImage->getTexec() % processImage->getTimeSlice() == 0) {
                            job[i].setDone(false);
                            preemptive(*processImage, mmu);
                        }
                    }

                    paramPreemptive(*processImage, mmu);
                }

                if (interrupt == Exit) {
                    job[i].setDone(false);
                    processImage = job[i].getProcessImage();

                    if (processImage != nullptr) {
                        processImage->setTterminate(Timer::getTime() + 1);
                        mmu.swapOut(*processImage);
                        processImage->setState(ExitProcess);
                    }
                }

                if ((interrupt == Error) || (interrupt == Empty)) {
                    std::cout << "interrupt == " << interrupt << '\n';
                    job[i].Debug();
                    return interrupt;
                }
            }
        }

        if (!isJob(job)) {
            return OK;
        }
    }
    return OK;
}

bool Scheduler::isJob(Job *job)
{
    bool isJob = false;
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (job[i].getAddr() == -1) {
            continue;
        }
        if (job[i].getDone()) {
            isJob = true;
        }
    }
    return isJob;
}

Interrupt Scheduler::execute(int addr, int id, CPU &cpu)
{
    Interrupt interrupt;
    sysreg[id].setState(PC, addr);
    SysReg &sysreg_ = sysreg[id];

    for (int i = 0; i < sliceCPU; i++) {
        interrupt = cpu.exeInstr(addr, sysreg_);
        if ((interrupt == Exit) || (interrupt == Error) || (interrupt == Empty)) {
            return interrupt;
        }
    }
    return interrupt;
}

bool Scheduler::scheduleJob(MMU &mmu, bool priority)
{
    DebugSPNQueue();
    job = std::shared_ptr<Job[]>(new Job[MAX_PROCESS]);
    ProcessImage *processImage = nullptr;

    int size_ = size(Swapped);
    for (int i = 0; i < size_; i++) {
        processImage = static_cast<ProcessImage *>(front(Swapped));
        pop(Swapped);
        processImage->setTservice(processImage->getTservice() + 1);
        push(Running, *processImage);
    }

    if (vParam.getStateTime() != noParam) {
        if (vParam.getStateTime() == TimeExecNotPreem) {
            ProcessNext(TimeExec);
        } else {
            ProcessNext(vParam.getStateTime());
        }
    }

    if (priority) {
        ProcessNext(noParam);
    }

    size_ = size(Running);

    if (quotaProcess == -1) {
        j = 0;
        for (int i = j; i < size_; i++) {
            if (j > MAX_PROCESS - 1) {
                return true;
            }
            processImage = static_cast<ProcessImage *>(front(Running));
            mmu.swapIn(*processImage);
            job[j].set(processImage->getAddr(), processImage->getID(), true, processImage);
            pop(Running);
            push(Running, *processImage);
            j++;
        }
        return false;
    } else {
        int k = j;
        for (int i = j; i < size_; i++) {
            if (j > MAX_PROCESS - 1) {
                return false;
            }
            if (j > quotaProcess - 1) {
                return true;
            }
            processImage = static_cast<ProcessImage *>(front(Running));
            mmu.swapIn(*processImage);
            job[j].set(processImage->getAddr(), processImage->getID(), true, processImage);
            pop(Running);
            push(Running, *processImage);
            j++;
        }
        j = 0;
        return false;
    }
}

Interrupt Scheduler::execute(MMU &mmu)
{
    return execute(job.get(), mmu);
}

int Scheduler::getID()
{
    for (int i = 0; i < MAX_PROCESS; i++) {
        if (sysreg[i].getStatus()) {
            return i;
        }
    }
    return -1;
}

void Scheduler::DebugBlock(int id, CPU &cpu)
{
    cpu.Debug(sysreg[id]);
}
void Scheduler::DebugSysReg(int id)
{
    sysreg[id].Debug();
}

void Scheduler::push(State state, ProcessImage &processImage)
{
    processQueue[state].push(&processImage);
}

Process *Scheduler::pop(State state)
{
    if (processQueue[state].empty()) {
        return nullptr;
    }
    Process *process = processQueue[state].front();
    processQueue[state].pop();
    return process;
}

void Scheduler::pop(State state, ProcessImage &processImage)
{
    ProcessImage *processImage_ = nullptr;
    for (int i = 0; i < (int)processQueue[state].size(); i++) {
        processImage_ = static_cast<ProcessImage *>(processQueue[state].front());
        processQueue[state].pop();
        if (processImage_ != &processImage) {
            processQueue[state].push(&processImage);
        }
    }
}

Process *Scheduler::front(State state)
{
    return processQueue[state].front();
}
bool Scheduler::empty(State state)
{
    return processQueue[state].empty();
}
int Scheduler::size(State state)
{
    return processQueue[state].size();
}

void Scheduler::setObservation(ProcessImage &processImage)
{
    statistic->setObservation(processImage.getUser(), processImage.getTexec(), processImage.getTservice());
}

void Scheduler::clearTpredict()
{
    statistic->clearTpredict();
}

double Scheduler::getTpredict(const std::string &user, StatTime time)
{
    return statistic->getTpredict(user, time);
}

void Scheduler::ProcessNext(StatTime time)
{
    sortQueue(Running, time);
    sortQueue(Blocked, time);
}

void Scheduler::sortQueue(State state, StatTime time)
{
    int size_ = size(state);
    double wk = 0;
    std::valarray<ProcessImage *> vaProcess(size_);
    std::valarray<double> va(size_);
    ProcessImage *processImage = nullptr;

    for (int i = 0; i < size_; i++) {
        processImage = static_cast<ProcessImage *>(front(state));
        vaProcess[i] = processImage;

        if (time == noParam) {
            va[i] = processImage->getPriority();
        } else {
            va[i] = statistic->getTpredict(vaProcess[i]->getUser(), time);
        }

        pop(state);
        push(state, *processImage);
    }

    for (int i = 0; i < size_ - 1; i++) {
        for (int j = i + 1; j < size_; j++) {
            if (va[j] < va[i]) {
                wk = va[j];
                processImage = vaProcess[j];
                va[j] = va[i];
                vaProcess[j] = vaProcess[i];
                va[i] = wk;
                vaProcess[i] = processImage;
            }
        }
    }

    for (int i = 0; i < size_; i++) {
        pop(state);
        push(state, *vaProcess[i]);
    }
}

void Scheduler::setTthreshold(StatTime time)
{
    vParam.setTthreshold(time, statistic->getTimeThreshold(time));
}

void Scheduler::methodPreemptive(ProcessImage &processImage, MMU &mmu, StatTime time)
{
    double Tpredict = statistic->getTpredict(processImage.getUser(), time);
    if (Tpredict == -1) {
        return;
    }
    if ((Tpredict - processImage.getTexec()) > vParam.getTthreshold(time)) {
        return;
    }

    for (int i = 0; i < MAX_PROCESS; i++) {
        if (job[i].getAddr() != -1) {
            if (job[i].getProcessImage()->getID() != processImage.getID()) {
                job[i].setDone(false);
                preemptive(*job[i].getProcessImage(), mmu);
            }
        }
    }
}

void Scheduler::paramPreemptive(ProcessImage &processImage, MMU &mmu)
{
    if (vParam.getState(TimeExecNotPreem)) {
        return;
    }
    if (vParam.getStateTime() != noParam) {
        methodPreemptive(processImage, mmu, vParam.getStateTime());
    }
}

void Scheduler::DebugQueue(State state)
{
    bool empty_ = empty(state);
    if (empty_) {
        std::cout << " queue is empty " << state << '\n';
        return;
    }

    int size = processQueue[state].size();
    if (size == 0) {
        return;
    }
    ProcessImage *processImage = static_cast<ProcessImage *>(processQueue[state].front());
    std::cout << '\n';
    std::cout << "Queue " << processImage->NameOf(state) << " size: " << size << '\n';
    std::cout << "ID = " << processImage->getID() << '\n';
    std::cout << "PC = " << processImage->getSysReg()->getState(PC) << '\n';
    std::cout << "ProcessImage" << '\n';
    processImage->Debug();
    std::cout << '\n';
}

void Scheduler::DebugSPNQueue(State state)
{
    if (empty(state)) {
        std::cout << " is empty " << '\n';
        return;
    }
    int size_ = size(state);
    ProcessImage *processImage = nullptr;

    for (int i = 0; i < size_; i++) {
        processImage = static_cast<ProcessImage *>(front(state));
        std::cout << '\n';
        std::cout << "user = " << processImage->getUser() << '\n';

        if (vParam.getState(TimeExecNotPreem) || vParam.getState(TimeExec)) {
            std::cout << "TpredictExec = " << statistic->getTpredict(processImage->getUser(), TimeExec) << '\n';
        }

        std::cout << "Texec = " << processImage->getTexec() << '\n';
        if (vParam.getState(TimeServ)) {
            std::cout << "TpredictServ = " << statistic->getTpredict(processImage->getUser(), TimeServ) << '\n';
            std::cout << "Tserv = " << processImage->getTservice() << '\n';
        }

        std::cout << "Queue state = " << processImage->getState() << '\n';
        std::cout << "Priority = " << processImage->getPriority() << '\n';
        std::cout << "TimeSlice = " << processImage->getTimeSlice() << '\n';

        pop(state);
        push(state, *processImage);
    }
}

void Scheduler::DebugSPNQueue()
{
    std::cout << "Method : queue order " << '\n';
    if (vParam.getState(TimeExec)) {
        std::cout << "Tthreshold SRT = " << vParam.getTthreshold(TimeExec) << '\n';
    }

    if (vParam.getState(TimeServ)) {
        std::cout << "Tthreshold HRRN = " << vParam.getTthreshold(TimeServ) << '\n';
    }

    std::cout << " queue Running " << '\n';
    DebugSPNQueue(Running);

    std::cout << " queue Swapped " << '\n';
    DebugSPNQueue(Swapped);

    std::cout << " queue Blocked " << '\n';
    DebugSPNQueue(Blocked);

    std::cout << " queue ExitProcess " << '\n';
    DebugSPNQueue(ExitProcess);
}
