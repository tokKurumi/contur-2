# Отчет 3. Ядро исполнения

## Покрываемые стадии

- Стадия 4: CPU + I/O
- Стадия 5: Interpreter Engine

## Контекст и цель

После формирования памяти и модели процесса в предыдущих этапах необходимо реализовать вычислительный контур: компоненты, которые на самом деле исполняют инструкции. На этом этапе проект получает:

- классический процессор с ALU (арифметико-логическое устройство) и регистровым файлом;
- fetch-decode-execute цикл, управляемый бюджетом тиков;
- систему обработки прерываний и ошибок;
- контроллер устройств ввода-вывода, позволяющий ядру обращаться к консоли, сети и другим периферийным устройствам.

Суть этого этапа в том, чтобы замкнуть контур «интерпретатор → CPU → ALU → память/I/O» и показать, как за один исполняемый цикл происходит выборка инструкции, её декодирование, выполнение операции и обновление состояния процессора.

## Что обязательно описать

1. Fetch-decode-execute цикл и контракты `ICPU`.
2. Контракт `IExecutionEngine` и его роль в управлении исполнением.
3. ALU: набор поддерживаемых операций и обработка ошибок.
4. Прерывания и исключения (`DivisionByZero`, `Exit`, `IllegalInstruction`).
5. Архитектура устройств: `IDevice`, `DeviceManager`, типовые реализации (`ConsoleDevice`, `NetworkDevice`).
6. Механизм работы по тик-бюджету: как Interpreter Engine управляет временем исполнения.

## Процессор (CPU) и контракт ICPU

Центральный процессор в симуляторе реализует классический набор ответственностей: управление программным счётчиком, выборка инструкций из памяти, декодирование команды, делегирование операций ALU и обновление состояния регистров.

Контракт `ICPU` определяет минимальный интерфейс:

```cpp
class ICPU
{
  public:
  virtual ~ICPU() = default;

  /// Выполнить один цикл fetch-decode-execute
  [[nodiscard]] virtual Result<CpuExecutionResult> step(ProcessId processId) = 0;

  /// Получить текущее значение регистра
  [[nodiscard]] virtual Result<RegisterValue> getRegister(ProcessId processId, Register reg) const = 0;

  /// Установить значение регистра
  [[nodiscard]] virtual Result<void> setRegister(ProcessId processId, Register reg, RegisterValue value) = 0;

  /// Получить текущее значение флагов
  [[nodiscard]] virtual Flags getFlags(ProcessId processId) const = 0;
};
```

Ключевой метод `step(processId)` инкапсулирует всю логику одной инструкции:

1. **Fetch** — прочитать слово из памяти по адресу `PC` (Program Counter);
2. **Decode** — разбить слово на опкод и операнды;
3. **Execute** — выполнить операцию (ALU или управление потоком);
4. **Write back** — обновить `PC` и целевые регистры.

В результате вызова `step(...)` возвращается объект `CpuExecutionResult`, который содержит информацию о выполненной инструкции, возможной ошибке или прерывании:

```cpp
struct CpuExecutionResult
{
  Instruction instruction;
  bool hasError = false;
  ErrorCode errorCode = ErrorCode::Ok;
  bool hasInterrupt = false;
  InterruptType interruptType;
};
```

Такой дизайн позволяет интерпретатору на высшем уровне корректно обработать результат и решить, как реагировать на ошибку (например, остановить процесс или логировать событие).

### Схема: fetch-decode-execute цикл с результатом

## Арифметико-логическое устройство (ALU)

`ALU` отвечает за выполнение всех арифметических и логических операций. Он получает два операнда, операцию и возвращает результат или ошибку:

```cpp
class ALU
{
  public:
  [[nodiscard]] Result<RegisterValue> execute(Instruction op, RegisterValue lhs, RegisterValue rhs) const;
};
```

Поддерживаемый набор операций охватывает базовую ISA:

- **Арифметика**: `Add`, `Sub`, `Mul`, `Div`, `Mod`
- **Логика**: `And`, `Or`, `Xor`, `Not`, `Shl`, `Shr`
- **Сравнение**: `Cmp`, `Eq`, `Ne`, `Lt`, `Le`, `Gt`, `Ge`
- **Ветвления**: `Jmp`, `Jz`, `Jnz`

Критический момент — обработка ошибок:

```cpp
case Instruction::Div:
{
  if (rhs == 0)
    return Result<RegisterValue>::error(ErrorCode::DivisionByZero);
  return Result<RegisterValue>::ok(lhs / rhs);
}
```

Все арифметические операции защищены: деление на ноль возвращает ошибку, а не прерывание процесса. Это важно для корректного управления потоком на уровне интерпретатора.

## Обработка прерываний и исключений

Система обработки ошибок в CPU чётко разделена между двумя уровнями:

1. **Локальные ошибки операций** (`DivisionByZero`, `InvalidInstruction`) — возвращаются через `Result<T>` из `step(...)`.
2. **Программные прерывания** (`Interrupt`, `Exit`, `Halt`) — сигнализируют о запросе на смену режима ядром.

При обработке в `step(...)`:

```cpp
auto decodeResult = decode(fetchedWord);
if (!decodeResult.isOk())
{
  return CpuExecutionResult{
    .instruction = Instruction::Nop,
    .hasError = true,
    .errorCode = ErrorCode::InvalidInstruction,
  };
}

auto instruction = decodeResult.value();
auto execResult = executeInstruction(instruction);

if (execResult.hasError)
{
  return CpuExecutionResult{
    .instruction = instruction,
    .hasError = true,
    .errorCode = execResult.errorCode,
  };
}

if (instruction == Instruction::Exit || instruction == Instruction::Halt)
{
  return CpuExecutionResult{
    .instruction = instruction,
    .hasInterrupt = true,
    .interruptType = InterruptType::Exit,
  };
}
```

Интерпретатор на уровне выше проверяет оба флага (`hasError` и `hasInterrupt`) и принимает решение: остановить процесс, перейти на handler, или записать событие в трейс.

## Устройства ввода-вывода и DeviceManager

Система ввода-вывода построена на двухуровневой архитектуре: универсальный контракт `IDevice` и управляющий компонент `DeviceManager`, маршрутизирующий запросы.

Контракт устройства минимален:

```cpp
class IDevice
{
  public:
  virtual ~IDevice() = default;

  [[nodiscard]] virtual std::string_view deviceId() const noexcept = 0;
  [[nodiscard]] virtual DeviceType type() const noexcept = 0;

  [[nodiscard]] virtual Result<Block> read(std::uint32_t address) = 0;
  [[nodiscard]] virtual Result<void> write(std::uint32_t address, const Block &data) = 0;

  [[nodiscard]] virtual Result<void> reset() = 0;
};
```

`DeviceManager` держит реестр устройств и обеспечивает маршрутизацию:

```cpp
class DeviceManager
{
  public:
  [[nodiscard]] Result<void> registerDevice(std::shared_ptr<IDevice> device);
  [[nodiscard]] Result<void> unregisterDevice(std::string_view deviceId);

  [[nodiscard]] Result<Block> readFromDevice(std::string_view deviceId, std::uint32_t address);
  [[nodiscard]] Result<void> writeToDevice(std::string_view deviceId, std::uint32_t address, const Block &data);

  [[nodiscard]] std::vector<std::string_view> listDevices() const;
};
```

### Типовые устройства

**ConsoleDevice** — базовое текстовое устройство для вывода/ввода. При записи данные накапливаются в буфере, при чтении возвращается очередной символ:

```cpp
class ConsoleDevice : public IDevice
{
  // Хранит очередь вывода и входных символов
  std::deque<Block> outputBuffer;
  std::deque<Block> inputBuffer;
};
```

**NetworkDevice** — имитирует сетевой адаптер. Позволяет процессам обмениваться данными через виртуальный канал:

```cpp
class NetworkDevice : public IDevice
{
  // Маршрутизирует пакеты между процессами
  std::unordered_map<ProcessId, std::deque<Block>> receiveQueues;
};
```

На практике такая архитектура позволяет:

- добавлять новые устройства без изменения CPU/интерпретатора;
- тестировать CPU отдельно, подменяя реальные устройства mock-объектами;
- отслеживать и логировать все I/O операции через единую точку (`DeviceManager`).

### Схема: иерархия устройств и маршрутизация

## Интерпретатор и управление исполнением по тик-бюджету

Контракт `IExecutionEngine` определяет высший уровень абстракции:

```cpp
class IExecutionEngine
{
  public:
  virtual ~IExecutionEngine() = default;

  /// Инициализировать процесс для исполнения
  [[nodiscard]] virtual Result<void> loadProcess(const ProcessImage &image) = 0;

  /// Выполнить N инструкций с бюджетом тиков
  [[nodiscard]] virtual Result<ExecutionSummary> execute(ProcessId processId, Tick timeBudget) = 0;

  /// Получить текущее состояние процесса
  [[nodiscard]] virtual ProcessState getProcessState(ProcessId processId) const = 0;
};
```

`InterpreterEngine` — конкретная реализация, которая управляет исполнением и гарантирует, что каждый процесс получает справедливую долю времени. Ключевой метод `execute(processId, timeBudget)` работает следующим образом:

1. Получить `ProcessImage` (PCB, регистры, код);
2. Инициализировать `PC` и регистры из образа;
3. Цикл исполнения:
   - Вызвать `CPU::step(...)`;
   - Проверить результат (ошибка, прерывание, успех);
   - Обновить累计時間 (accumulated time);
   - Если `timeBudget` исчерпан или процесс завершился, выйти из цикла.
4. Сохранить состояние регистров обратно в `ProcessImage`;
5. Вернуть `ExecutionSummary` с информацией о выполненных инструкциях и событиях.

Структура результата:

```cpp
struct ExecutionSummary
{
  Tick instructionsExecuted = 0;
  Tick timeBudgetUsed = 0;
  bool processCompleted = false;
  bool hadError = false;
  ErrorCode lastError = ErrorCode::Ok;
  std::vector<ExecutionEvent> events;
};
```

Такой дизайн обеспечивает:

- **Справедливость** — каждый процесс выполняется до исчерпания своего бюджета;
- **Предсказуемость** — нет скрытого управления потоком внутри CPU;
- **Наблюдаемость** — все события (ошибки, прерывания, I/O) собираются в `ExecutionSummary`;
- **Тестируемость** — можно подменять любой компонент (CPU, память, I/O) на mock.

Интерпретатор также интегрируется с системой трейсинга: каждая инструкция может быть залогирована с полным контекстом (PC, операнды, результат, состояние флагов).

## Код и артефакты (вставить фрагменты)
- `src/contur/cpu/cpu.cpp`
- `src/include/contur/io/i_device.h`
- `src/contur/io/device_manager.cpp`
- `src/include/contur/execution/i_execution_engine.h`
- `src/contur/execution/interpreter_engine.cpp`

## Место под рисунки

- Рисунок 1. Поток исполнения одной инструкции.
- Рисунок 2. Связи CPU и I/O через DeviceManager.

## Диаграмма 1 (Sequence step(), mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="60" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="InterpreterEngine" vertex="1"><mxGeometry x="40" y="60" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="61" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="CPU" vertex="1"><mxGeometry x="230" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="62" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="IMemory" vertex="1"><mxGeometry x="380" y="60" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="63" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ALU" vertex="1"><mxGeometry x="540" y="60" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="64" edge="1" parent="1" source="60" target="61" style="endArrow=classic;html=1;" value="step()"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="65" edge="1" parent="1" source="61" target="62" style="endArrow=classic;html=1;" value="fetch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="66" edge="1" parent="1" source="61" target="63" style="endArrow=classic;html=1;" value="execute op"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Component diagram CPU/IO, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="70" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="CPU" vertex="1"><mxGeometry x="70" y="100" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="71" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="DeviceManager" vertex="1"><mxGeometry x="250" y="100" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="72" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ConsoleDevice" vertex="1"><mxGeometry x="450" y="60" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="73" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="NetworkDevice" vertex="1"><mxGeometry x="450" y="140" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="74" edge="1" parent="1" source="70" target="71" style="endArrow=classic;html=1;" value="I/O req"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="75" edge="1" parent="1" source="71" target="72" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="76" edge="1" parent="1" source="71" target="73" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Проверка и воспроизводимость

Для подтверждения работы CPU и интерпретатора используется стандартный сценарий:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Проверяется:

- **Корректность fetch-decode-execute**: инструкции правильно загружаются, декодируются и выполняются;
- **Обработка ошибок**: деление на ноль, недопустимые инструкции, сегментация возвращают соответствующие коды;
- **Управление I/O**: запись в устройства и чтение из них работают без побочных эффектов;
- **Гарантия бюджета**: интерпретатор останавливается точно после израсходования выделенного времени;
- **Целостность регистров**: состояние регистров сохраняется между контекстными переключениями.

Иллюстрация для документа:

- Скрин лога тестов с успешным завершением unit-тестов CPU и executor.

## Источники кода, использованные в отчете

- `src/include/contur/cpu/alu.h`
- `src/include/contur/cpu/i_cpu.h`
- `src/contur/cpu/cpu.cpp`
- `src/include/contur/io/i_device.h`
- `src/contur/io/device_manager.cpp`
- `src/contur/io/console_device.cpp`
- `src/contur/io/network_device.cpp`
- `src/include/contur/execution/i_execution_engine.h`
- `src/contur/execution/interpreter_engine.cpp`
- `src/include/contur/arch/instruction.h`
- `src/include/contur/arch/register_file.h`
- `src/include/contur/core/error.h`
- `src/include/contur/core/types.h`

## Критерии готовности

- В тексте раскрыта последовательность fetch-decode-execute с кодовыми примерами.
- Объяснены контракты `ICPU` и `IExecutionEngine` и их практическое назначение.
- Показаны способы обработки ошибок (`Result<T>`) и прерываний (`hasError`, `hasInterrupt`).
- Описаны два уровня I/O (`IDevice` и `DeviceManager`) с примерами типовых устройств.
- Объяснен механизм управления бюджетом времени в интерпретаторе.
- Дана связь компонентов: InterpreterEngine → CPU → ALU/IMemory и CPU → DeviceManager → IDevice.
- Приведены диаграммы последовательности и компонентов в нужных местах текста.

## Краткие выводы

Стадия 4-5 завершает вычислительный контур: CPU с ALU обрабатывает инструкции, интерпретатор управляет их исполнением по тик-бюджету, а система I/O позволяет процессам взаимодействовать с внешними устройствами. 

Ключевые достигнутые свойства:

- **Предсказуемость**: каждый процесс получает гарантированное количество тиков без скрытого переполнения или зависания;
- **Безопасность**: все ошибки (деление на ноль, недопустимые инструкции) обрабатываются явно через `Result<T>`, а не через исключения;
- **Модульность**: CPU, ALU, I/O слабо связаны через интерфейсы, что позволяет тестировать каждый компонент отдельно и легко добавлять новые устройства;
- **Наблюдаемость**: интерпретатор собирает полную информацию о каждом цикле (выполненные инструкции, события, ошибки) для трейсинга и отладки.

На этом фундаменте далее строятся планировщик, диспетчер, синхронизация и ядро ОС в целом. Вычислительный контур готов к интеграции со слоями планирования и управления процессами.
