# Отчет 6. Системные сервисы ядра

## Охват этапов

- Этап 8: IPC & Syscalls
- Этап 9: File System
- Этап 10: Kernel

## Контекст и цель

После того как процессы могут исполняться и планироваться, ядру нужны «сервисы»:
взаимодействие процессов (IPC), вход в ядро через системные вызовы, постоянное хранилище
(файловая система) и единая точка входа в API ядра. Этот отчёт описывает все четыре слоя и
их сборку через `KernelBuilder`.

Цели:

- описать `IIpcChannel` и три реализации: `Pipe`, `SharedMemory`, `MessageQueue`;
- описать `IpcManager` как реестр именованных каналов;
- зафиксировать набор `SyscallId` и устройство `SyscallTable`;
- объяснить `SimpleFS` — инод-таблицу, аллокатор блоков, FD-таблицу и `IFileSystem`;
- показать, что `KernelBuilder` собирает Kernel чисто через DI и проверяет полноту состава.

## IPC: каналы и реестр

Все каналы реализуют единый интерфейс `IIpcChannel`. Это позволяет писать клиентский код,
независимый от транспорта.

Три реализации:

- `Pipe` — bounded circular buffer с блокирующей семантикой read/write при заполнении;
- `SharedMemory` — именованная область с attach/detach на нескольких процессах;
- `MessageQueue` — типизированная FIFO с опциональным приоритетом.

`IpcManager` — это реестр: создание по имени, поиск, удаление; повторный `create` возвращает
существующий канал (а не создаёт дубль). За счёт этого процессы синхронизируются по «имени»,
а не по сырым указателям, что важно для будущего syscall-уровня.

### Схема: модели IPC

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="120" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Process A" vertex="1"><mxGeometry x="40" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="121" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Process B" vertex="1"><mxGeometry x="660" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="122" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IpcManager (registry)" vertex="1"><mxGeometry x="340" y="60" width="200" height="50" as="geometry" /></mxCell>
    <mxCell id="123" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Pipe (bounded FIFO)" vertex="1"><mxGeometry x="40" y="170" width="200" height="40" as="geometry" /></mxCell>
    <mxCell id="124" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="SharedMemory (attach/detach)" vertex="1"><mxGeometry x="290" y="170" width="260" height="40" as="geometry" /></mxCell>
    <mxCell id="125" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="MessageQueue (typed, priority)" vertex="1"><mxGeometry x="580" y="170" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="130" edge="1" parent="1" source="120" target="122" style="endArrow=classic;html=1;" value="lookup/create"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="131" edge="1" parent="1" source="121" target="122" style="endArrow=classic;html=1;" value="lookup/create"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="132" edge="1" parent="1" source="122" target="123" style="endArrow=block;html=1;dashed=1;" value="implements IIpcChannel"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="133" edge="1" parent="1" source="122" target="124" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="134" edge="1" parent="1" source="122" target="125" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Syscall: граница user-kernel

`SyscallId` — фиксированный перечень числовых идентификаторов. В коде он сгруппирован по
семантике (управление процессом, файлы, IPC, синхронизация, время/PID):

| Группа | Идентификаторы |
|---|---|
| Управление процессом | `Exit`, `Fork`, `Exec`, `Wait` |
| Базовый I/O | `Write`, `Read` |
| Файлы | `Open`, `Close` |
| IPC | `CreatePipe`, `SendMessage`, `ReceiveMessage`, `ShmCreate`, `ShmAttach` |
| Синхронизация | `MutexLock`, `MutexUnlock`, `SemWait`, `SemSignal` |
| Утилиты | `GetPid`, `GetTime`, `Yield` |

`SyscallTable` хранит `SyscallId → SyscallHandlerFn` и диспетчеризует вызовы. Регистрация
обработчика возможна и из `IKernel::registerSyscallHandler(...)`, что делает граничный слой
расширяемым без правок ядра.

### Схема: путь системного вызова

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="140" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="User code (Block::Interrupt)" vertex="1"><mxGeometry x="40" y="60" width="200" height="50" as="geometry" /></mxCell>
    <mxCell id="141" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="CPU: Interrupt::SystemCall" vertex="1"><mxGeometry x="280" y="60" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="142" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Dispatcher: StopReason::Interrupted" vertex="1"><mxGeometry x="540" y="60" width="260" height="50" as="geometry" /></mxCell>
    <mxCell id="143" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="Kernel::syscall(pid, id, args)" vertex="1"><mxGeometry x="280" y="160" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="144" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="SyscallTable.dispatch(id)" vertex="1"><mxGeometry x="540" y="160" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="145" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="SyscallHandlerFn (handler body)" vertex="1"><mxGeometry x="540" y="240" width="260" height="50" as="geometry" /></mxCell>
    <mxCell id="150" edge="1" parent="1" source="140" target="141" style="endArrow=classic;html=1;" value="Int"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="151" edge="1" parent="1" source="141" target="142" style="endArrow=classic;html=1;" value="ExecutionResult::interrupted"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="152" edge="1" parent="1" source="142" target="143" style="endArrow=classic;html=1;" value="handle"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="153" edge="1" parent="1" source="143" target="144" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="154" edge="1" parent="1" source="144" target="145" style="endArrow=classic;html=1;" value="invoke"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Файловая система `SimpleFS`

`IFileSystem` определяет восемь операций: `open`, `read`, `write`, `close`, `mkdir`, `remove`,
`listDir`, `stat`. Возвращаемые типы — `Result<...>`; `stat` возвращает `InodeInfo` (id, тип,
размер, число блоков, временные метки в тиках).

`SimpleFS` строится на трёх артефактах:

- `Inode` — узел: тип (`File`/`Directory`), размер, указатели на блоки данных, временные метки;
- `BlockAllocator` — bitmap, `allocate/free/isFree`, поддерживающий целостность;
- `FileDescriptor` + per-process FD-таблица — пользовательский handle на открытый файл.

Над этим — простая иерархия директорий с `DirectoryEntry` (имя → inode).

### Схема: внутреннее устройство `SimpleFS`

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="160" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="IFileSystem (SimpleFS)" vertex="1"><mxGeometry x="40" y="40" width="200" height="50" as="geometry" /></mxCell>
    <mxCell id="161" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Inode table" vertex="1"><mxGeometry x="280" y="0" width="170" height="40" as="geometry" /></mxCell>
    <mxCell id="162" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="DirectoryEntry tree" vertex="1"><mxGeometry x="280" y="50" width="170" height="40" as="geometry" /></mxCell>
    <mxCell id="163" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="BlockAllocator (bitmap)" vertex="1"><mxGeometry x="280" y="100" width="200" height="40" as="geometry" /></mxCell>
    <mxCell id="164" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="Data blocks (simulated disk)" vertex="1"><mxGeometry x="500" y="100" width="220" height="40" as="geometry" /></mxCell>
    <mxCell id="165" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="Per-process FD table" vertex="1"><mxGeometry x="280" y="150" width="200" height="40" as="geometry" /></mxCell>
    <mxCell id="170" edge="1" parent="1" source="160" target="161" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="171" edge="1" parent="1" source="160" target="162" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="172" edge="1" parent="1" source="160" target="163" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="173" edge="1" parent="1" source="163" target="164" style="endArrow=classic;html=1;" value="manages"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="174" edge="1" parent="1" source="160" target="165" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## `KernelBuilder` и фасад `IKernel`

`IKernel` — это фасад, через который снаружи доступно всё ядро: создание/завершение процессов,
тики (`tick`, `runForTicks`), системные вызовы (`syscall`, `registerSyscallHandler`),
именованные синхронизационные примитивы (`registerSyncPrimitive`, `enterCritical`,
`leaveCritical`), а также неизменяемый снимок состояния `snapshot()`.

`KernelBuilder` — fluent-API для DI-сборки. Реальный набор методов:

```cpp
KernelBuilder &withClock(...);
KernelBuilder &withMemory(...);
KernelBuilder &withMmu(...);
KernelBuilder &withVirtualMemory(...);
KernelBuilder &withCpu(...);
KernelBuilder &withExecutionEngine(...);
KernelBuilder &withScheduler(...);
KernelBuilder &withDispatcher(...);
KernelBuilder &withTracer(...);
KernelBuilder &withRuntime(...);
KernelBuilder &withFileSystem(...);
KernelBuilder &withIpcManager(...);
KernelBuilder &withSyscallTable(...);
KernelBuilder &withDefaultTickBudget(...);

Result<std::unique_ptr<IKernel>> build();
```

Ключевая инвариант DI-сборки: `KernelBuilder` инжектирует именно компонент `IDispatchRuntime`
в сборку диспетчера; никаких числовых параметров «host thread count» в самой `KernelSnapshot`
или `KernelDependencies` нет — рантайм-конфиг остаётся внутри dispatch-слоя.

### Схема: композиция Kernel через KernelBuilder

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="200" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="KernelBuilder (composition root)" vertex="1"><mxGeometry x="320" y="20" width="260" height="50" as="geometry" /></mxCell>
    <mxCell id="201" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IClock" vertex="1"><mxGeometry x="40" y="110" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="202" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IMemory" vertex="1"><mxGeometry x="180" y="110" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="203" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IMMU" vertex="1"><mxGeometry x="320" y="110" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="204" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IVirtualMemory" vertex="1"><mxGeometry x="460" y="110" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="205" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="ICPU" vertex="1"><mxGeometry x="620" y="110" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="206" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IExecutionEngine" vertex="1"><mxGeometry x="40" y="170" width="160" height="40" as="geometry" /></mxCell>
    <mxCell id="207" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IScheduler" vertex="1"><mxGeometry x="220" y="170" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="208" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IDispatcher" vertex="1"><mxGeometry x="380" y="170" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="209" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IDispatchRuntime" vertex="1"><mxGeometry x="540" y="170" width="160" height="40" as="geometry" /></mxCell>
    <mxCell id="210" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="ITracer" vertex="1"><mxGeometry x="40" y="230" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="211" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="IFileSystem" vertex="1"><mxGeometry x="180" y="230" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="212" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="IpcManager" vertex="1"><mxGeometry x="340" y="230" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="213" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="SyscallTable" vertex="1"><mxGeometry x="500" y="230" width="140" height="40" as="geometry" /></mxCell>
    <mxCell id="214" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="IKernel (final)" vertex="1"><mxGeometry x="340" y="300" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="220" edge="1" parent="1" source="200" target="201" style="endArrow=classic;html=1;" value="withClock"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="221" edge="1" parent="1" source="200" target="202" style="endArrow=classic;html=1;" value="withMemory"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="222" edge="1" parent="1" source="200" target="203" style="endArrow=classic;html=1;" value="withMmu"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="223" edge="1" parent="1" source="200" target="204" style="endArrow=classic;html=1;" value="withVirtualMemory"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="224" edge="1" parent="1" source="200" target="205" style="endArrow=classic;html=1;" value="withCpu"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="225" edge="1" parent="1" source="200" target="206" style="endArrow=classic;html=1;" value="withExecutionEngine"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="226" edge="1" parent="1" source="200" target="207" style="endArrow=classic;html=1;" value="withScheduler"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="227" edge="1" parent="1" source="200" target="208" style="endArrow=classic;html=1;" value="withDispatcher"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="228" edge="1" parent="1" source="200" target="209" style="endArrow=classic;html=1;" value="withRuntime"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="229" edge="1" parent="1" source="200" target="210" style="endArrow=classic;html=1;" value="withTracer"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="230" edge="1" parent="1" source="200" target="211" style="endArrow=classic;html=1;" value="withFileSystem"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="231" edge="1" parent="1" source="200" target="212" style="endArrow=classic;html=1;" value="withIpcManager"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="232" edge="1" parent="1" source="200" target="213" style="endArrow=classic;html=1;" value="withSyscallTable"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="233" edge="1" parent="1" source="200" target="214" style="endArrow=block;html=1;" value="build() -> Result&lt;unique_ptr&lt;IKernel&gt;&gt;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Источники кода, использованные в отчёте

- `src/include/contur/ipc/i_ipc_channel.h`
- `src/include/contur/ipc/pipe.h` (+ `.cpp`)
- `src/include/contur/ipc/shared_memory.h` (+ `.cpp`)
- `src/include/contur/ipc/message_queue.h` (+ `.cpp`)
- `src/include/contur/ipc/ipc_manager.h` (+ `.cpp`)
- `src/include/contur/syscall/syscall_ids.h`
- `src/include/contur/syscall/syscall_handler.h`
- `src/include/contur/syscall/syscall_table.h` (+ `.cpp`)
- `src/include/contur/fs/inode.h`
- `src/include/contur/fs/directory_entry.h`
- `src/include/contur/fs/block_allocator.h` (+ `.cpp`)
- `src/include/contur/fs/file_descriptor.h` (+ `.cpp`)
- `src/include/contur/fs/i_filesystem.h`
- `src/include/contur/fs/simple_fs.h` (+ `.cpp`)
- `src/include/contur/kernel/i_kernel.h`
- `src/include/contur/kernel/kernel.h` (+ `.cpp`)
- `src/include/contur/kernel/kernel_builder.h` (+ `.cpp`)

## Критерии готовности

- IPC: показаны три модели (Pipe / SharedMemory / MessageQueue) и единый реестр `IpcManager`.
- Syscall: перечислены все `SyscallId`, описан путь от `Interrupt::SystemCall` до handler-а.
- FS: описаны Inode, BlockAllocator, FD-таблица, директории; зафиксирован контракт `IFileSystem`.
- Kernel: показан полный набор `with*`-методов `KernelBuilder` и инжектирование рантайма как
    обязательное.

## Краткие выводы

IPC, syscalls, файловая система и `KernelBuilder` собирают набор подсистем в полноценный
«фасад ядра» с предсказуемым DI-входом и наблюдаемым снимком состояния. Любая подсистема
может быть подменена для тестов или демонстраций без правок в самом ядре, что и обеспечивает
работу TUI, нативного исполнения, многопроцессорного рантайма и CI поверх одного и того же
ядра.
