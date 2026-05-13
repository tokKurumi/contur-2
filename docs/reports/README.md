# Отчёты по разработке Contur 2

В этой папке собрано восемь тематических отчётов, покрывающих этапы 0–17 реализации проекта.
Каждый отчёт описывает актуальное состояние кода (`src/`), CMake-таргетов и CI-workflow-ов.
Отчёты служат первичным источником для двух артефактов:

- онлайн-документации (Doxygen-сайт + статичная HTML/MD-публикация);
- typst-исходников учебной/курсовой работы.

## Состав

1. [01 Архитектурный фундамент](01_architecture_foundation.md) — этапы 0–1
    (scaffold, `core`, `arch`, типы, `Result<T>`, `IClock`, `Event<>`, ISA, регистровый файл).
2. [02 Память и процессы](02_memory_and_processes.md) — этапы 2–3
    (`IMemory`, `IMMU`, `PageTable`, стратегии замещения, `IVirtualMemory`, `PCB`, `ProcessImage`,
    жизненный цикл процесса).
3. [03 Ядро исполнения](03_execution_core.md) — этапы 4–5
    (ALU, CPU, `IDevice`/`DeviceManager`, `IExecutionEngine`, `InterpreterEngine`).
4. [04 Планирование процессов](04_scheduling_policies.md) — этап 6
    (`ISchedulingPolicy`, все 7 политик: FCFS / RR / SPN / SRT / HRRN / Priority / MLFQ,
    `Scheduler`, lane-ы и work-stealing, `Statistics` для EWMA-прогноза burst).
5. [05 Диспетчеризация, синхронизация и трассировка](05_dispatch_sync_tracing.md) —
    этапы 7, 11, 12 (`Dispatcher`, `MPDispatcher`, `IDispatchRuntime`, `DispatcherPool`,
    `ISyncPrimitive` + `SyncLayer`, `DeadlockDetector` с двумя графами,
    `ITracer`/`Tracer`/`NullTracer` + sink-и).
6. [06 Системные сервисы ядра](06_kernel_services.md) — этапы 8, 9, 10
    (IPC: `Pipe`/`SharedMemory`/`MessageQueue` + `IpcManager`; `SyscallId` + `SyscallTable`;
    `SimpleFS` (inode, BlockAllocator, FD-таблица); `IKernel` + `KernelBuilder`).
7. [07 TUI и приложение визуализации ядра](07_tui_and_demos.md) — этапы 13–14
    (DTO `Tui*`, `KernelDiagnostics`/`KernelReadModel`, `HistoryBuffer`, `TuiController`,
    `IRenderer` + конкретный backend `FtxuiRenderer` + оболочка `FtxuiApp` с полным набором
    горячих клавиш; CMake-инварианта «`contur2_lib` без UI»).
8. [08 Нативное исполнение, тестирование и CI](08_native_testing_ci.md) — этапы 15–17
    (`NativeEngine` с симметричными Win32/POSIX-путями, реальная карта 862 тестов,
    фактическая CI-матрица Clang/GCC × Release/TSAN-Release, отдельный workflow публикации
    Doxygen-доки на GitHub Pages).

## Правила оформления и использования отчётов

- **Язык**: весь повествовательный текст — на русском. Названия API, классов, файлов, флагов
    CMake, имён переменных — на английском (как в коде). Это нужно, чтобы фрагменты можно
    было переносить в typst и Doxygen без потери ссылок на код.
- **Источники истины**: каждый отчёт ссылается на конкретные заголовки и `.cpp` в разделе
    «Источники кода, использованные в отчёте». Если код меняется, отчёт обязан быть
    подкорректирован под него (а не наоборот).
- **Диаграммы**: схемы заданы в формате `mxGraphModel` (draw.io / diagrams.net) прямо в
    тексте — это позволяет редактировать их без отдельных бинарных файлов и одновременно
    рендерить в подключаемые SVG/PNG. Для финальной публикации рекомендуется собирать
    отдельную подпапку `docs/reports/diagrams/` (XML-исходники) и `docs/reports/images/`
    (PNG/SVG-экспорты).
- **Связанность с планом**: иерархия этапов и счётчики тестов синхронизированы с
    [`.github/IMPLEMENTATION_PLAN.md`](../../.github/IMPLEMENTATION_PLAN.md). Этот план —
    источник истины по составу этапов и тестов; отчёты пересказывают его на содержательном
    уровне.

## Что отчёты НЕ описывают

- Бизнес-процессы вокруг проекта (план релизов, организационные ритуалы и т. п.).
- Детальное API уровня методов — это задача Doxygen-сайта.
- Учебные задания/контрольные вопросы — это задача лабораторных работ.
