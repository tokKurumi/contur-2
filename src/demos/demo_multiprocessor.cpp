#include "demos/demos.h"

#include "contur/kernel.h"
#include "contur/mmu.h"
#include "contur/mp_dispatcher.h"

#include <iostream>
#include <valarray>

void demoMultiprocessor(DemoContext &ctx)
{
    char ch;

    /* Схема 10. Многопроцессорное планирование
    D  — Dispatcher (диспетчер)
    P1 — Программа 1, P2 — Программа 2
    K  — Kernel (ядро)
    M  — Memory (память)
    Pr — Processor (процессоры)
    Lan — сеть
    F  — Flag (флаг процесса)
    Dev — Device (устройство)
    T  — Timer (таймер)
    CPU — центральный процессор

    Tenter — Время входа
    Tbegin — Время начала
    Tservice — Время обслуживания в очередях
    Texec — Время выполнения
    Tterminate — Время завершения
    Tround = Tservice + Texec — Время оборота
    Tround/Tservice — Нормализованное время */

    // P1: (6 * 7 + 3) / 5 – 4 = 5
    auto code_1 = std::make_shared<Code>(12);
    code_1->setCmd(Mov, r1, 6);  // перемещение содержимого второго операнда в первый (регистр)
    code_1->setCmd(Mov, r2, 7);
    code_1->setCmd(Mul, r1, r2); // регистр * регистр
    code_1->setCmd(Add, r1, 3);
    code_1->setCmd(Mov, r2, 5);
    code_1->setCmd(Div, r1, r2);
    code_1->setCmd(Sub, r1, 4);
    code_1->setCmd(Wmem, r1, 69); // запись содержимого r1 в память по адресу 69
    code_1->setCmd(Rmem, r3, 69); // чтение содержимого r3 из памяти по адресу 69
    code_1->setCmd(Int, r1, Dev); // обращение к устройству (Dev) для печати содержимого r1
    code_1->setCmd(Int, r3, Lan); // посыл содержимого r3 по сети (Lan)
    code_1->setCmd(Int, 0, Exit);

    // P2: 9 * 12 + 2 = 110
    auto code_2 = std::make_shared<Code>(9);
    code_2->setCmd(Mov, r1, 9);
    code_2->setCmd(Mov, r2, 12);
    code_2->setCmd(Mul, r1, r2);
    code_2->setCmd(Add, r1, 2);
    code_2->setCmd(Wmem, r1, 70); // запись содержимого r1 в память по адресу 70
    code_2->setCmd(Rmem, r3, 70); // чтение содержимого r3 из памяти по адресу 70
    code_2->setCmd(Int, r1, Dev); // обращение к устройству (Dev) для печати содержимого r1
    code_2->setCmd(Int, r3, Lan); // посыл содержимого r3 по сети (Lan)
    code_2->setCmd(Int, 0, Exit);

    /* Multiprocessor scheduling (MP) — многопроцессорное
    планирование. Метод Global queue — планирование очередей */
    int maxProcessor = 4; // максимальное количество процессоров

    // CPU — создание планировщика для многопроцессорной системы
    Scheduler mpScheduler(ctx.cpu(), ctx.maxProcess());

    // D — диспетчер многопроцессорной системы
    MPDispatcher mpDispatcher(ctx.maxProcess(), mpScheduler, ctx.mmu(), maxProcessor);

    // K — ядро операционной системы
    Kernel mpKernel(mpDispatcher);

    std::cout << '\n';
    std::cout << "Scheme 10. MultiProcessor scheduling" << '\n';
    std::cout << "P1 -> P2, K (Kernel), M (Memory), Pr (Processors), Lan, Dev, T (Timer), CPU" << '\n';

    // --- 1. Pr — Состояние процессоров (Processors) ---
    std::cout << '\n';
    std::cout << "1. Pr - Processor state Y/N-any" << '\n';
    std::cin >> ch;
    if (ch == 'Y') {
        mpDispatcher.MPDebug();
    }

    // --- 2. T — Таймер, K — Ядро создаёт P1 и P2 ---
    std::cout << '\n';
    std::cout << "2. T (Timer), K (Kernel) create P1 and P2 Y/N-any" << '\n';
    std::cin >> ch;
    if (ch == 'Y') {
        Timer::setTime(); // T — устанавливаем таймер в нулевое значение

        // K — создать процесс P1
        Handle *handle_1 = mpKernel.CreateProcess("P1", code_1);
        // K — создать процесс P2
        Handle *handle_2 = mpKernel.CreateProcess("P2", code_2);

        // --- 3. D — Диспетчер: очереди процессов ---
        std::cout << '\n';
        std::cout << "3. D - Dispatcher queue NotRunning Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.DebugQueue(NotRunning);
        }

        // --- 4. D — Диспетчер: виртуальная память и PCB ---
        std::cout << '\n';
        std::cout << "4. D - Virtual memory & PCB (P1, P2) Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.DebugVirtualMemory();
            mpDispatcher.DebugPCB(*handle_1);
            mpDispatcher.DebugPCB(*handle_2);
        }

        // --- 5. D, Pr — Диспетчер: планирование и выполнение на процессорах ---
        std::cout << '\n';
        std::cout << "5. D dispatch & Pr execute (step 1) Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.dispatch(); // изменяет состояние системы
            mpDispatcher.MPDebug();  // Pr — состояние процессоров
        }

        std::cout << '\n';
        std::cout << "   D dispatch & Pr execute (step 2) Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.dispatch(); // для модели из трех очередей
            mpDispatcher.MPDebug();  // Pr — состояние процессоров
        }

        std::cout << '\n';
        std::cout << "   D dispatch & Pr execute (step 3) Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.dispatch();
            mpDispatcher.MPDebug(); // Pr — состояние процессоров
        }

        // --- 6. D — Очереди после выполнения ---
        std::cout << '\n';
        std::cout << "6. D - Dispatcher queue Running Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.DebugQueue(Running);
        }

        std::cout << '\n';
        std::cout << "   D - Dispatcher queue ExitProcess Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpDispatcher.DebugQueue(ExitProcess);
        }

        // --- 7. M — Состояние памяти ---
        std::cout << '\n';
        std::cout << "7. M - Memory dump Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            ctx.memory().debugHeap();
        }

        // --- 8. T — Время выполнения процессов P1, P2 ---
        std::cout << '\n';
        std::cout << "8. T (Timer) - P1, P2 ProcessTime Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            handle_1->ProcessTime();
            handle_2->ProcessTime();
        }

        // --- 9. K — Завершение процессов ---
        std::cout << '\n';
        std::cout << "9. K (Kernel) - TerminateProcess P1, P2 Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            mpKernel.TerminateProcess(*handle_1); // завершить процесс P1
            mpKernel.TerminateProcess(*handle_2); // завершить процесс P2
        }

        // --- 10. M — Очистка памяти ---
        std::cout << '\n';
        std::cout << "10. M - Clear memory Y/N-any" << '\n';
        std::cin >> ch;
        if (ch == 'Y') {
            ctx.memory().clearMemory();
            ctx.memory().debugHeap();
        }
    }
}
