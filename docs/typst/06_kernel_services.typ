#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 6,
  discipline: discipline,
  title: "Системные сервисы ядра: IPC, системные вызовы, файловая система",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Реализовать сервисный слой ядра Contur 2: межпроцессное взаимодействие (IPC) через единый
контракт каналов, формализованный граничный механизм системных вызовов, минимальную
инод-ориентированную файловую систему и фасад `IKernel`, собираемый через
`KernelBuilder`. Полученный слой делает ядро самостоятельной операционной средой, доступной
для пользовательских процессов и внешних потребителей.

Задачи работы:

+ описать контракт `IIpcChannel` и реализации `Pipe`, `SharedMemory`, `MessageQueue`;
+ реализовать реестр каналов `IpcManager` с поиском по имени;
+ описать перечень `SyscallId` и таблицу `SyscallTable`;
+ описать инод-ориентированную файловую систему `SimpleFS` за контрактом `IFileSystem`;
+ описать фасад `IKernel` и DI-сборку через `KernelBuilder`.

= Реализация

== Межпроцессное взаимодействие

Все каналы IPC реализуют единый контракт `IIpcChannel`, что позволяет писать клиентский
код, независимый от транспорта. Реализованы три модели:

- `Pipe` — bounded circular buffer с блокирующей семантикой `read` / `write` при заполнении;
- `SharedMemory` — именованная область с `attach`/`detach` на нескольких процессах;
- `MessageQueue` — типизированная FIFO с опциональным приоритетом.

`IpcManager` хранит реестр каналов и обеспечивает поиск по имени; повторный `create`
возвращает существующий канал, что исключает дубли. Благодаря такой адресации процессы
синхронизируются по именам, а не по сырым указателям, и это нужно для будущего пути через
syscall-слой. Взаимодействие двух процессов через `IpcManager` показано на рисунке
@fig:ipc.

#figure(
  diagram(
    spacing: (2.6cm, 1.5cm),
    node-inset: 8pt,
    node((0, 0), [Process A], fill: rgb("#dae8fc")),
    node((2, 0), [Process B], fill: rgb("#dae8fc")),
    node((1, 0), [IpcManager], fill: rgb("#d5e8d4")),
    node((0, 1), [Pipe], fill: rgb("#ffe6cc")),
    node((1, 1), [SharedMemory], fill: rgb("#ffe6cc")),
    node((2, 1), [MessageQueue], fill: rgb("#ffe6cc")),
    edge((0, 0), (1, 0), "->", [lookup/create]),
    edge((2, 0), (1, 0), "->", [lookup/create]),
    edge((1, 0), (0, 1), "-->", [IIpcChannel], label-side: right),
    edge((1, 0), (1, 1), "-->"),
    edge((1, 0), (2, 1), "-->"),
  ),
  caption: [Взаимодействие процессов через `IpcManager` и каналы IPC],
) <fig:ipc>

== Граница пользовательского и системного режимов

Перечень `SyscallId` фиксирует числовые идентификаторы всех системных вызовов и
сгруппирован по семантике. Сводка приведена в таблице @tab:syscalls.

#figure(
  table(
    columns: 2,
    align: (left, left),
    [Группа], [Идентификаторы],
    [Управление процессом], [`Exit`, `Fork`, `Exec`, `Wait`],
    [Базовый ввод-вывод], [`Write`, `Read`],
    [Файлы], [`Open`, `Close`],
    [IPC], [`CreatePipe`, `SendMessage`, `ReceiveMessage`, `ShmCreate`, `ShmAttach`],
    [Синхронизация], [`MutexLock`, `MutexUnlock`, `SemWait`, `SemSignal`],
    [Утилиты], [`GetPid`, `GetTime`, `Yield`],
  ),
  caption: [Группы системных вызовов и их идентификаторы],
) <tab:syscalls>

`SyscallTable` хранит отображение `SyscallId` → `SyscallHandlerFn` и диспетчеризует
вызовы. Регистрация обработчика возможна и через метод фасада
`IKernel::registerSyscallHandler(...)`, что делает граничный слой расширяемым без правок в
самом ядре. Путь системного вызова показан на рисунке @fig:syscall-path.

#figure(
  diagram(
    spacing: (2.6cm, 1.2cm),
    node-inset: 7pt,
    node((0, 0), [User code (Block::Interrupt)], fill: rgb("#dae8fc")),
    node((0, 1), [CPU: Interrupt::SystemCall], fill: rgb("#d5e8d4")),
    node((0, 2), [Dispatcher: StopReason::Interrupted], fill: rgb("#ffe6cc")),
    node((1, 2), [Kernel::syscall(pid, id, args)], fill: rgb("#fff2cc")),
    node((1, 1), [SyscallTable.dispatch(id)], fill: rgb("#e1d5e7")),
    node((1, 0), [SyscallHandlerFn], fill: rgb("#f8cecc")),
    edge((0, 0), (0, 1), "->", [Int]),
    edge((0, 1), (0, 2), "->", [interrupted]),
    edge((0, 2), (1, 2), "->", [handle]),
    edge((1, 2), (1, 1), "->"),
    edge((1, 1), (1, 0), "->", [invoke]),
  ),
  caption: [Прохождение системного вызова от пользовательского кода до обработчика],
) <fig:syscall-path>

== Файловая система

`IFileSystem` определяет восемь операций: `open`, `read`, `write`, `close`, `mkdir`,
`remove`, `listDir`, `stat`. Каждый вызов возвращает `Result<...>`; метод `stat` возвращает
структуру `InodeInfo` с идентификатором, типом узла, размером, числом блоков и временными
метками в тиках.

`SimpleFS` опирается на четыре артефакта:

- `Inode` — узел с типом (`File`/`Directory`), размером, указателями на блоки данных и
  временными метками;
- `BlockAllocator` — bitmap-ориентированный аллокатор с операциями `allocate`, `free`,
  `isFree`;
- `DirectoryEntry` — запись каталога, отображающая имя в идентификатор инода;
- `FileDescriptor` + таблица дескрипторов на процесс — пользовательский handle на открытый
  файл.

== Фасад ядра и `KernelBuilder`

Фасад `IKernel` объединяет все подсистемы под единым API: создание/завершение процессов
(`createProcess`, `terminateProcess`), исполнение тиков (`tick`, `runForTicks`), системные
вызовы (`syscall`, `registerSyscallHandler`), именованные примитивы синхронизации
(`registerSyncPrimitive`, `enterCritical`, `leaveCritical`) и неизменяемый снимок состояния
(`snapshot`).

`KernelBuilder` обеспечивает DI-сборку через 14 fluent-методов, начиная с
`withClock`/`withMemory`/`withMmu`/`withVirtualMemory`/`withCpu` и заканчивая
`withTracer`/`withRuntime`/`withFileSystem`/`withIpcManager`/`withSyscallTable`. Ключевой
инвариант сборки: рантайм диспетчера задаётся именно компонентом `IDispatchRuntime`;
никаких числовых параметров «host thread count» в `KernelSnapshot` или
`KernelDependencies` нет, конфигурация рантайма остаётся внутри dispatch-слоя.
Композиция показана на рисунке @fig:kernel-builder.

#figure(
  diagram(
    spacing: (2.6cm, 0.6cm),
    node-inset: 6pt,
    node((1, 0), [KernelBuilder], fill: rgb("#dae8fc")),
    node((0, 1), [IClock], fill: rgb("#d5e8d4")),
    node((0, 2), [IMemory], fill: rgb("#d5e8d4")),
    node((0, 3), [IMMU], fill: rgb("#d5e8d4")),
    node((0, 4), [IVirtualMemory], fill: rgb("#d5e8d4")),
    node((0, 5), [ICPU], fill: rgb("#d5e8d4")),
    node((0, 6), [IExecutionEngine], fill: rgb("#ffe6cc")),
    node((0, 7), [IScheduler], fill: rgb("#ffe6cc")),
    node((0, 8), [IDispatcher], fill: rgb("#ffe6cc")),
    node((0, 9), [IDispatchRuntime], fill: rgb("#ffe6cc")),
    node((0, 10), [ITracer], fill: rgb("#fff2cc")),
    node((0, 11), [IFileSystem], fill: rgb("#fff2cc")),
    node((0, 12), [IpcManager], fill: rgb("#fff2cc")),
    node((0, 13), [SyscallTable], fill: rgb("#fff2cc")),
    node((2, 7), [IKernel], fill: rgb("#f8cecc")),
    edge((1, 0), (0, 1), "->"),
    edge((1, 0), (0, 2), "->"),
    edge((1, 0), (0, 3), "->"),
    edge((1, 0), (0, 4), "->"),
    edge((1, 0), (0, 5), "->"),
    edge((1, 0), (0, 6), "->"),
    edge((1, 0), (0, 7), "->"),
    edge((1, 0), (0, 8), "->"),
    edge((1, 0), (0, 9), "->"),
    edge((1, 0), (0, 10), "->"),
    edge((1, 0), (0, 11), "->"),
    edge((1, 0), (0, 12), "->"),
    edge((1, 0), (0, 13), "->"),
    edge((1, 0), (2, 7), "=>", [build()], bend: 45deg, label-side: right, label-sep: 0.5em),
  ),
  caption: [Композиция ядра через `KernelBuilder`],
) <fig:kernel-builder>

= Заключение <s>

IPC, системные вызовы, файловая система и фасад `IKernel` собирают набор подсистем в
полноценное «ядро операционной системы» с предсказуемым DI-входом и наблюдаемым снимком
состояния. Любая подсистема подменяется для тестов или специальных сценариев без правок в
самом ядре, что обеспечивает работу TUI, нативного исполнения и многопроцессорного рантайма
поверх одного и того же ядра.
