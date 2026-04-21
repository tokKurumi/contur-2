# Отчет 6. Системные сервисы ядра

## Покрываемые стадии

- Стадия 8: IPC & Syscalls
- Стадия 9: File System
- Стадия 10: Kernel

## Цель отчета

Описать сервисную часть ядра: межпроцессное взаимодействие, системные вызовы, файловую подсистему и финальную композицию через `KernelBuilder`.

## Что обязательно описать

1. `IIpcChannel` и реализации: Pipe, SharedMemory, MessageQueue.
2. `SyscallTable`: граница user-kernel.
3. `SimpleFS`: inode table, block allocator, FD table.
4. `IKernel` как фасад и DI-сборка в `KernelBuilder`.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/ipc/i_ipc_channel.h`
- `src/contur/ipc/ipc_manager.cpp`
- `src/include/contur/syscall/syscall_ids.h`
- `src/contur/syscall/syscall_table.cpp`
- `src/include/contur/fs/i_filesystem.h`
- `src/contur/fs/simple_fs.cpp`
- `src/include/contur/kernel/i_kernel.h`
- `src/contur/kernel/kernel_builder.cpp`

## Место под рисунки

- Рисунок 1. Пример взаимодействия двух процессов через IPC.
- Рисунок 2. Структура inode и отображение блоков файла.

## Диаграмма 1 (Syscall dispatch, mxGraphModel)

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

## Диаграмма 2 (KernelBuilder composition, mxGraphModel)

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
