#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 1,
  discipline: discipline,
  title: "Архитектурный фундамент симулятора Contur 2",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Сформировать архитектурный фундамент симулятора операционной системы Contur 2:
воспроизводимую сборочную инфраструктуру, единые доменные типы, предсказуемую модель ошибок,
интерфейсные контракты и базовую модель центрального процессора. Полученная база служит
основой для последующих подсистем — памяти, процессов, планирования и ядра.

Задачи работы:

+ настроить кросс-платформенную сборку на CMake с пресетами для двух компиляторов;
+ зафиксировать сквозные доменные типы и стиль обработки ошибок через `Result<T>`;
+ описать контракты на уровне интерфейсов (`IClock`, `IMemory`, `IExecutionEngine` и т. п.);
+ определить набор команд, регистровый файл и блочное представление инструкции;
+ обеспечить разделение слоёв на уровне CMake-таргетов и подтвердить инварианту «ядро без UI».

= Реализация

== Сборочный каркас

Корневой файл `src/CMakeLists.txt` фиксирует стандарт C++20, экспортирует
`compile_commands.json` для статических анализаторов и описывает три семейства
CMake-таргетов: библиотеку ядра `contur2_lib` (без зависимостей от UI), слой `contur2_tui`
(подключает FTXUI через Conan) и исполняемое приложение `contur2`. Структура зависимостей
изображена на рисунке @fig:cmake-deps.

#figure(
  diagram(
    spacing: (3.4cm, 1.4cm),
    node-inset: 8pt,
    node((0, 0), [contur2 (app)], fill: rgb("#dae8fc")),
    node((1, 0), [contur2_demos], fill: rgb("#d5e8d4")),
    node((0, 1), [contur2_tui], fill: rgb("#e1d5e7")),
    node((1, 1), [ftxui::ftxui], fill: rgb("#fff2cc")),
    node((0, 2), [contur2_lib], fill: rgb("#ffe6cc")),
    node((1, 2), [tests (GTest)], fill: rgb("#f8cecc")),
    edge((0, 0), (1, 0), "->", [PRIVATE]),
    edge((0, 0), (0, 1), "->", [PRIVATE], label-side: left),
    edge((0, 1), (1, 1), "->", [PUBLIC]),
    edge((0, 1), (0, 2), "->", [PUBLIC], label-side: left),
    edge((1, 2), (0, 2), "->", [test]),
  ),
  caption: [Граф CMake-зависимостей проекта Contur 2],
) <fig:cmake-deps>

Полный цикл сборки и прогона тестов вызывается одним скриптом:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Скрипт `build.sh` совмещает установку Conan-зависимостей и конфигурацию CMake так, чтобы один
и тот же набор флагов работал и локально, и в CI. Это обеспечивает единое окружение у всех
разработчиков и упрощает воспроизведение результатов.

== Доменные типы и модель ошибок

Каркас прикладного типажа Contur 2 определён в `core/types.h` и фиксирует семантику ключевых
сущностей: `ProcessId`, `MemoryAddress`, `Tick`, `RegisterValue`, `FrameId`, `InodeId`,
`DeviceId`, `SubscriptionId`. Дополнительно объявлены константы-сентинелы `INVALID_PID`,
`NULL_ADDRESS`, `INVALID_FRAME`, `INVALID_INODE_ID`, размер регистрового файла
`REGISTER_COUNT = 16` и квант планирования `DEFAULT_TIME_SLICE = 4`.

Все операции, способные завершиться ошибкой, возвращают `Result<T>` либо `Result<void>`:

```cpp
enum class ErrorCode : std::int32_t {
    Ok = 0,
    OutOfMemory, InvalidPid, InvalidAddress, DivisionByZero,
    InvalidInstruction, SegmentationFault, DeviceError,
    DeadlockDetected, InvalidState, NotFound, AlreadyExists,
    BufferFull, BufferEmpty, EndOfFile, NotImplemented,
    /* ... */
};

template <typename T> class [[nodiscard]] Result {
    public:
    [[nodiscard]] static Result ok(T value);
    [[nodiscard]] static Result error(ErrorCode code);
    [[nodiscard]] bool isOk() const noexcept;
    [[nodiscard]] const T &value() const &;
    [[nodiscard]] ErrorCode errorCode() const noexcept;
};
```

Маркер `[[nodiscard]]` предотвращает «потерю» результата, а отсутствие исключений в горячих
путях упрощает анализ потока выполнения и интеграцию с многопоточным рантаймом. Обработка
результата операции схематически показана на рисунке @fig:result-flow.

#figure(
  diagram(
    spacing: (3cm, 1.2cm),
    node-inset: 8pt,
    node((0, 0), [Операция], shape: fletcher.shapes.rect, fill: rgb("#dae8fc")),
    node((1, 0), [isOk?], shape: fletcher.shapes.diamond, fill: rgb("#fff2cc")),
    node((2, -1), [use value()], fill: rgb("#d5e8d4")),
    node((2, 1), [handle errorCode()], fill: rgb("#f8cecc")),
    edge((0, 0), (1, 0), "->", [Result<T>], label-pos: 0.6),
    edge((1, 0), (2, -1), "->", [да], label-side: left),
    edge((1, 0), (2, 1), "->", [нет], label-side: right),
  ),
  caption: [Поток обработки `Result<T>` у потребителя],
) <fig:result-flow>

== Инверсия зависимостей и PIMPL

Интерфейсы вынесены в публичные заголовки, конкретные реализации скрыты идиомой PIMPL
(`struct Impl; std::unique_ptr<Impl> impl_;`). Базовый пример — часы симуляции `IClock` и
их реализация `SimulationClock`:

```cpp
class IClock {
    public:
    virtual ~IClock() = default;
    [[nodiscard]] virtual Tick now() const noexcept = 0;
    virtual void tick() = 0;
    virtual void reset() = 0;
};

class SimulationClock final : public IClock {
    public:
    SimulationClock();
    ~SimulationClock() override;
    [[nodiscard]] Tick now() const noexcept override;
    void tick() override;
    void reset() override;

    private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

Такой подход упрощает модульное тестирование (часы можно подменить mock-реализацией) и
снижает связность по заголовкам: изменение приватной структуры `Impl` не требует
перекомпиляции потребителей.

== Базовая модель архитектуры

Перечисление `Instruction` в `arch/instruction.h` задаёт инструкции симулируемого процессора:
арифметика и логика (`Add`, `Sub`, `Mul`, `Div`, `And`, `Or`, `Xor`, `ShiftLeft`,
`ShiftRight`, `Compare`), управление потоком (`JumpEqual`, `JumpLess`, `Call`, `Return`),
работа со стеком (`Push`, `Pop`), доступ к памяти (`ReadMemory`, `WriteMemory`),
прерывания (`Interrupt`) и завершение (`Halt`).

Регистровый файл `RegisterFile` содержит 16 регистров: универсальные `R0`–`R13`, программный
счётчик `ProgramCounter` (`R14`) и указатель стека `StackPointer` (`R15`). Каждая
инструкция кодируется значением `Block` с полями `code` (опкод), `reg` (целевой регистр),
`operand` (значение либо адрес) и `state` (режим адресации).

== Шинa событий

Шаблон `Event<Args...>` в `core/event.h` реализует паттерн «Observer» и предоставляет
подписку с уникальным идентификатором, отписку и широковещательную рассылку:

```cpp
template <typename... Args> class Event {
    public:
    using Callback = std::function<void(Args...)>;
    [[nodiscard]] SubscriptionId subscribe(Callback callback);
    bool unsubscribe(SubscriptionId id);
    void emit(Args... args) const;
};
```

Эта примитивная шина далее используется для развязки ядра, статистики и трассировщика —
наблюдатель подписывается на событие, не создавая жёсткой зависимости с источником.

= Заключение

Архитектурный фундамент Contur 2 содержит воспроизводимую сборку, единые доменные типы,
предсказуемую модель ошибок без исключений, интерфейсные контракты с PIMPL-реализациями и
базовую модель центрального процессора. Слои отделены на уровне CMake-таргетов: ядро не
зависит от UI, что подтверждает архитектурную инварианту проекта. Перечисленные решения
формируют основу, поверх которой строятся подсистемы памяти, процессов и ядра, описанные в
последующих отчётах.

= Список использованных источников

+ `src/CMakeLists.txt`
+ `src/CMakePresets.json`
+ `src/build.sh`
+ `src/include/contur/core/types.h`
+ `src/include/contur/core/error.h`
+ `src/include/contur/core/clock.h`
+ `src/include/contur/core/event.h`
+ `src/include/contur/arch/instruction.h`
+ `src/include/contur/arch/register_file.h`
+ `src/include/contur/arch/block.h`
+ `src/include/contur/arch/interrupt.h`
