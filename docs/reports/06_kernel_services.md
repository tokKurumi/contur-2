# Отчет 6. Системные сервисы ядра

## Покрываемые стадии

- Стадия 8: IPC & Syscalls
- Стадия 9: File System
- Стадия 10: Kernel

## Контекст и цель

На предыдущих этапах проект уже получил процессы, память, CPU, планирование и диспетчеризацию. Следующий шаг — сервисный слой ядра, то есть те механизмы, которые делают симулируемую ОС пригодной для практического использования: межпроцессное взаимодействие, системные вызовы, файловую подсистему и финальную композицию всех зависимостей в единый фасад ядра.

На этом этапе Contur 2 перестает быть только машиной исполнения и становится целостной OS-like платформой, где процессы могут обмениваться данными, обращаться к файлам, вызывать ядро через syscall boundary и работать через единый объект `IKernel`.

Цель отчета состоит в том, чтобы:

- показать, как устроены IPC-каналы и их диспетчеризация;
- объяснить, где проходит граница user-kernel через `SyscallTable`;
- описать внутреннюю структуру `SimpleFS` и ее файловые операции;
- показать композицию ядра через `KernelBuilder` и роль `IKernel` как фасада.

Иллюстрация для вставки в Word:

- Рисунок: пример взаимодействия двух процессов через IPC.

## Что обязательно описать

1. `IIpcChannel` и реализации: Pipe, SharedMemory, MessageQueue.
2. `SyscallTable` как граница user-kernel.
3. `SimpleFS`: inode table, block allocator, FD table.
4. `IKernel` как фасад и DI-сборка в `KernelBuilder`.

## Межпроцессное взаимодействие

IPC в проекте построено вокруг общего канала `IIpcChannel`. Это минимальный byte-oriented контракт, который скрывает тип транспорта, но сохраняет единый способ записи и чтения данных.

```cpp
class IIpcChannel
{
    public:
    virtual ~IIpcChannel() = default;

    [[nodiscard]] virtual Result<std::size_t> write(std::span<const std::byte> data) = 0;
    [[nodiscard]] virtual Result<std::size_t> read(std::span<std::byte> buffer) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};
```

Такой интерфейс удобен тем, что разные механизмы IPC могут быть использованы через один и тот же API. Пользователь кода не знает, идет ли обмен через FIFO pipe, shared memory или message queue; он видит только чтение и запись байтов.

### Реализации IPC

#### Pipe

`Pipe` моделирует однонаправленный FIFO-канал с ограниченной емкостью. Он полезен для потоковой передачи данных, когда важен порядок байтов и контролируемое переполнение буфера.

```cpp
class Pipe final : public IIpcChannel
{
    public:
    explicit Pipe(std::string name, std::size_t capacity = 1024);

    [[nodiscard]] Result<std::size_t> write(std::span<const std::byte> data) override;
    [[nodiscard]] Result<std::size_t> read(std::span<std::byte> buffer) override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;
};
```

#### SharedMemory

`SharedMemory` представляет именованную фиксированную область памяти, которую могут attach/detach несколько процессов. Она подходит для более быстрого обмена данными, когда процессы разделяют общий буфер.

```cpp
class SharedMemory final : public IIpcChannel
{
    public:
    explicit SharedMemory(std::string name, std::size_t bytes);

    [[nodiscard]] Result<std::size_t> write(std::span<const std::byte> data) override;
    [[nodiscard]] Result<std::size_t> read(std::span<std::byte> buffer) override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] Result<void> attach(ProcessId pid);
    [[nodiscard]] Result<void> detach(ProcessId pid);
};
```

#### MessageQueue

`MessageQueue` хранит структурированные сообщения и может работать как FIFO-очередь или как приоритетная очередь. Это делает его более удобным для событийного обмена, чем сырой поток байтов.

```cpp
class MessageQueue final : public IIpcChannel
{
    public:
    explicit MessageQueue(std::string name, std::size_t maxMessages = 64, bool priorityMode = false);

    [[nodiscard]] Result<std::size_t> write(std::span<const std::byte> data) override;
    [[nodiscard]] Result<std::size_t> read(std::span<std::byte> buffer) override;
    void close() override;
    [[nodiscard]] bool isOpen() const noexcept override;
    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] Result<void> send(const Message &message);
    [[nodiscard]] Result<Message> receive();
};
```

### IPC Manager

`IpcManager` выступает реестром каналов. Он хранит именованные объекты и гарантирует безопасный доступ к ним через mutex внутри менеджера.

```cpp
Result<void> IpcManager::createPipe(const std::string &name, std::size_t capacity);
Result<void> IpcManager::createSharedMemory(const std::string &name, std::size_t bytes);
Result<void> IpcManager::createMessageQueue(const std::string &name, std::size_t maxMessages, bool priorityMode);
Result<std::reference_wrapper<IIpcChannel>> IpcManager::getChannel(const std::string &name);
Result<void> IpcManager::destroyChannel(const std::string &name);
```

Это удобный слой композиции: пользователь не создает канал вручную через конкретный класс, а просит менеджер зарегистрировать канал по имени и потом обращается к нему через единый lookup.

Иллюстрация для вставки в Word:

- Рисунок: взаимодействие двух процессов через IPC-канал.

## Граница user-kernel: SyscallTable

Системные вызовы в проекте оформлены как таблица диспетчеризации. Это и есть формальная граница между пользовательским кодом и ядром: процесс вызывает syscall id, а ядро находит обработчик и выполняет его в безопасном контексте.

### Коды системных вызовов

```cpp
enum class SyscallId : std::uint16_t
{
    Exit = 0,
    Write = 1,
    Read = 2,
    Fork = 3,
    Exec = 4,
    Wait = 5,
    Open = 10,
    Close = 11,
    CreatePipe = 20,
    SendMessage = 21,
    ReceiveMessage = 22,
    ShmCreate = 23,
    ShmAttach = 24,
    MutexLock = 30,
    MutexUnlock = 31,
    SemWait = 32,
    SemSignal = 33,
    GetPid = 40,
    GetTime = 41,
    Yield = 42,
};
```

Набор id показывает, что syscall layer объединяет не только классические процессы и файлы, но и IPC, синхронизацию и служебные операции runtime.

### Таблица syscall handlers

`SyscallTable` хранит отображение `SyscallId -> handler` и позволяет регистрировать обработчики двумя способами: через лямбду/functor или через объект, реализующий `ISyscallHandler`.

```cpp
Result<void> SyscallTable::registerHandler(SyscallId id, HandlerFn handler);
Result<void> SyscallTable::registerHandler(SyscallId id, ISyscallHandler &handler);
Result<void> SyscallTable::unregisterHandler(SyscallId id);
Result<RegisterValue> SyscallTable::dispatch(SyscallId id, std::span<const RegisterValue> args, ProcessImage &caller) const;
```

Смысл этой архитектуры в том, что user-kernel boundary становится не набором `switch` по всему ядру, а централизованной точкой маршрутизации. Если handler не зарегистрирован, вызывающая сторона получает `NotFound`, а не неопределенное поведение.

### Схема: syscall dispatch

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="120" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="User Program" vertex="1"><mxGeometry x="40" y="80" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="121" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="CPU Interrupt" vertex="1"><mxGeometry x="200" y="80" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="122" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="SyscallTable" vertex="1"><mxGeometry x="360" y="80" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="123" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Handler" vertex="1"><mxGeometry x="520" y="80" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="124" edge="1" parent="1" source="120" target="121" style="endArrow=classic;html=1;" value="int syscall"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="125" edge="1" parent="1" source="121" target="122" style="endArrow=classic;html=1;" value="dispatch(id)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="126" edge="1" parent="1" source="122" target="123" style="endArrow=classic;html=1;" value="invoke"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Файловая подсистема

### Контракт IFileSystem

Файловая подсистема в проекте скрыта за абстрактным интерфейсом. Это позволяет kernel-слою работать с файловой системой без знания о внутренней структуре inode, блоков и таблицы дескрипторов.

```cpp
class IFileSystem
{
    public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual Result<FileDescriptor> open(const std::string &path, OpenMode mode) = 0;
    [[nodiscard]] virtual Result<std::size_t> read(FileDescriptor fd, std::span<std::byte> buffer) = 0;
    [[nodiscard]] virtual Result<std::size_t> write(FileDescriptor fd, std::span<const std::byte> data) = 0;
    [[nodiscard]] virtual Result<void> close(FileDescriptor fd) = 0;
    [[nodiscard]] virtual Result<void> mkdir(const std::string &path) = 0;
    [[nodiscard]] virtual Result<void> remove(const std::string &path) = 0;
    [[nodiscard]] virtual Result<std::vector<DirectoryEntry>> listDir(const std::string &path) const = 0;
    [[nodiscard]] virtual Result<InodeInfo> stat(const std::string &path) const = 0;
};
```

Это классический OS abstraction boundary: процессы работают с путями и дескрипторами, а не с физическими блоками и inode напрямую.

### SimpleFS

`SimpleFS` реализует файловую систему на основе трех внутренних механизмов:

- `inode`-структуры для метаданных;
- `BlockAllocator` для управления дисковыми блоками;
- `FileDescriptorTable` для отслеживания открытых файлов.

Внутренняя модель хранит узлы дерева каталогов как `Inode + children`, а корень всегда имеет inode id 1.

Ключевой фрагмент состояния:

```cpp
std::size_t blockSize;
std::vector<std::vector<std::byte>> disk;
BlockAllocator allocator;
FileDescriptorTable fdTable;
std::unordered_map<InodeId, Node> nodes;
InodeId nextInodeId = ROOT_INODE_ID + 1;
Tick tick = 0;
```

### Как устроены open/read/write

`open(...)` сначала ищет существующий inode по пути. Если файла нет, но открыт режим `Create`, создается новый inode и привязывается к родительскому каталогу. Затем формируется `OpenFileState` и выдается дескриптор через `fdTable`.

`read(...)` и `write(...)` всегда проходят через файловый дескриптор: сначала проверяется mode, затем inode, после чего операция переводится в чтение/запись с учетом текущего offset.

Это важный момент, потому что file descriptor table отделяет объект файла от факта его открытия. Один inode может иметь несколько открытых дескрипторов с разными режимами и offset-ами.

### Управление блоками

`ensureFileBlocks(...)` отвечает за выделение и освобождение дисковых блоков в зависимости от требуемого размера файла. При увеличении размера блоки выделяются через `BlockAllocator`, при уменьшении — освобождаются и очищаются.

```cpp
auto ensure = ensureFileBlocks(node, std::max(node.inode.size, endOffset));
```

Это дает корректную модель роста и уменьшения файла, а не просто перезапись буфера поверх массива памяти.

### Метаданные inode

Публичная структура `InodeInfo` возвращает наружу только то, что нужно для диагностики и `stat()`:

```cpp
struct InodeInfo
{
    InodeId id = INVALID_INODE_ID;
    InodeType type = InodeType::File;
    std::size_t size = 0;
    std::size_t blockCount = 0;
    Tick createdAt = 0;
    Tick modifiedAt = 0;
};
```

Таким образом внутренний layout inode скрыт, но интерфейс ядра остается достаточным для UI, CLI и тестов.

### Структура inode и блоков

Иллюстрация для вставки в Word:

- Рисунок: структура inode и отображение блоков файла.

## Фасад ядра и DI-сборка

### IKernel

`IKernel` — это единая фасадная точка, через которую пользователь кода создает процессы, вызывает tick/run, регистрирует syscall handlers, работает с IPC и синхронизацией, а также получает snapshot состояния системы.

```cpp
class IKernel
{
    public:
    virtual ~IKernel() = default;

    [[nodiscard]] virtual Result<ProcessId> createProcess(const ProcessConfig &config) = 0;
    [[nodiscard]] virtual Result<void> terminateProcess(ProcessId pid) = 0;
    [[nodiscard]] virtual Result<void> tick(std::size_t tickBudget = 0) = 0;
    [[nodiscard]] virtual Result<void> runForTicks(std::size_t cycles, std::size_t tickBudget = 0) = 0;
    [[nodiscard]] virtual Result<RegisterValue>
    syscall(ProcessId pid, SyscallId id, std::span<const RegisterValue> args) = 0;

    [[nodiscard]] virtual Result<void> registerSyscallHandler(SyscallId id, SyscallHandlerFn handler) = 0;
    [[nodiscard]] virtual Result<void>
    registerSyncPrimitive(const std::string &name, std::unique_ptr<ISyncPrimitive> primitive) = 0;
    [[nodiscard]] virtual Result<void> enterCritical(ProcessId pid, std::string_view primitiveName) = 0;
    [[nodiscard]] virtual Result<void> leaveCritical(ProcessId pid, std::string_view primitiveName) = 0;

    [[nodiscard]] virtual KernelSnapshot snapshot() const = 0;
    [[nodiscard]] virtual Tick now() const noexcept = 0;
    [[nodiscard]] virtual bool hasProcess(ProcessId pid) const noexcept = 0;
    [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;
};
```

Этот фасад объединяет все ключевые сервисы ядра в одну точку доступа. Для пользователя это означает, что он не обязан самостоятельно соединять dispatcher, scheduler, IPC manager, syscall table и filesystem — он работает через единый kernel object.

### KernelBuilder

`KernelBuilder` реализует dependency injection сборку. Он аккумулирует все зависимости и создает готовый `Kernel` только если собраны все обязательные компоненты.

```cpp
KernelBuilder &KernelBuilder::withClock(std::unique_ptr<IClock> clock);
KernelBuilder &KernelBuilder::withMemory(std::unique_ptr<IMemory> memory);
KernelBuilder &KernelBuilder::withMmu(std::unique_ptr<IMMU> mmu);
KernelBuilder &KernelBuilder::withVirtualMemory(std::unique_ptr<IVirtualMemory> virtualMemory);
KernelBuilder &KernelBuilder::withCpu(std::unique_ptr<ICPU> cpu);
KernelBuilder &KernelBuilder::withExecutionEngine(std::unique_ptr<IExecutionEngine> executionEngine);
KernelBuilder &KernelBuilder::withScheduler(std::unique_ptr<IScheduler> scheduler);
KernelBuilder &KernelBuilder::withDispatcher(std::unique_ptr<IDispatcher> dispatcher);
KernelBuilder &KernelBuilder::withTracer(std::unique_ptr<ITracer> tracer);
KernelBuilder &KernelBuilder::withRuntime(std::unique_ptr<IDispatchRuntime> runtime);
KernelBuilder &KernelBuilder::withFileSystem(std::unique_ptr<IFileSystem> fileSystem);
KernelBuilder &KernelBuilder::withIpcManager(std::unique_ptr<IpcManager> ipcManager);
KernelBuilder &KernelBuilder::withSyscallTable(std::unique_ptr<SyscallTable> syscallTable);
Result<std::unique_ptr<IKernel>> KernelBuilder::build();
```

Перед сборкой выполняется проверка обязательных зависимостей. Если хотя бы один компонент отсутствует, build возвращает `InvalidState`. Если не задан tick budget по умолчанию, возвращается `InvalidArgument`.

Это правильный для архитектуры подход: собранный kernel должен быть либо полностью готов, либо явно отвергнут, а не частично инициализирован.

### Схема: KernelBuilder composition

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="130" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="KernelBuilder" vertex="1"><mxGeometry x="260" y="40" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="131" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Dispatcher" vertex="1"><mxGeometry x="80" y="140" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="132" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Scheduler" vertex="1"><mxGeometry x="220" y="140" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="133" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="MMU" vertex="1"><mxGeometry x="360" y="140" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="134" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="IPC/FS/Syscalls" vertex="1"><mxGeometry x="500" y="140" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="135" edge="1" parent="1" source="130" target="131" style="endArrow=classic;html=1;" value="wire"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="136" edge="1" parent="1" source="130" target="132" style="endArrow=classic;html=1;" value="wire"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="137" edge="1" parent="1" source="130" target="133" style="endArrow=classic;html=1;" value="wire"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="138" edge="1" parent="1" source="130" target="134" style="endArrow=classic;html=1;" value="wire"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Проверка и воспроизводимость

Стандартный сценарий проверки после интеграции сервисов ядра выглядит так:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Проверяются следующие свойства:

- корректный переход через syscall boundary и вызов зарегистрированных handlers;
- создание, чтение, запись и закрытие IPC-каналов;
- корректность open/read/write/mkdir/remove/stat в `SimpleFS`;
- отсутствие рассинхронизации между dispatcher, scheduler и kernel facade;
- корректная сборка `KernelBuilder` только при наличии всех обязательных зависимостей.

Иллюстрация для документа:

- Скриншот дерева сервисов ядра или успешного тестового прогона, где видны IPC, FS и kernel integration tests.

## Источники кода, использованные в отчете

- `src/include/contur/ipc/i_ipc_channel.h`
- `src/include/contur/ipc/pipe.h`
- `src/include/contur/ipc/shared_memory.h`
- `src/include/contur/ipc/message_queue.h`
- `src/contur/ipc/ipc_manager.cpp`
- `src/include/contur/syscall/syscall_ids.h`
- `src/include/contur/syscall/syscall_table.h`
- `src/contur/syscall/syscall_table.cpp`
- `src/include/contur/fs/i_filesystem.h`
- `src/contur/fs/simple_fs.cpp`
- `src/include/contur/kernel/i_kernel.h`
- `src/contur/kernel/kernel_builder.cpp`

## Критерии готовности

- В тексте есть связка «сервисный механизм -> контракт -> наблюдаемое поведение».
- Описаны три IPC-механизма и роль IpcManager.
- Показана граница user-kernel через SyscallTable и набор SyscallId.
- Объяснена структура `SimpleFS` и работа inode/block/FD слоев.
- Дана роль `IKernel` как фасада и `KernelBuilder` как DI-композиции.
- Диаграммы встроены в соответствующие смысловые разделы.

## Краткие выводы

Стадии 8-10 собирают сервисный слой ядра в единую прикладную оболочку. IPC дает процессам способ обмениваться данными, syscall layer формализует переход из user space в kernel space, файловая подсистема предоставляет привычную модель каталогов и дескрипторов, а `KernelBuilder` завершает композицию системы в один объект, пригодный для запуска и тестирования.

В результате Contur 2 получает не просто вычислительное ядро, а полноценную среду с сервисами, через которые можно строить более сложные сценарии работы ОС, не нарушая архитектурных границ.
