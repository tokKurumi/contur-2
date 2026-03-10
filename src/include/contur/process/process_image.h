/// @file process_image.h
/// @brief ProcessImage — the full in-memory representation of a process.
///
/// A ProcessImage bundles together:
/// - A PCB (metadata / bookkeeping)
/// - A code segment (vector of Blocks — the program)
/// - A RegisterFile (the process's CPU register context)
///
/// This is the object that the execution engine works with directly.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "contur/core/types.h"

#include "contur/arch/block.h"
#include "contur/arch/register_file.h"
#include "contur/process/pcb.h"
#include "contur/process/priority.h"

namespace contur {

    /// @brief Full in-memory representation of a process.
    ///
    /// Contains all data needed to execute a process:
    /// - PCB for lifecycle management
    /// - Code segment (vector<Block>) for the interpreter
    /// - RegisterFile for CPU context save/restore
    ///
    /// Ownership: ProcessImage owns all three components.
    class ProcessImage
    {
        public:
        /// @brief Constructs a ProcessImage with the given parameters.
        /// @param id        Unique process identifier.
        /// @param name      Human-readable process name.
        /// @param code      The program code (vector of Blocks).
        /// @param priority  Process priority (defaults to Normal).
        /// @param arrivalTime Simulation tick at creation.
        ProcessImage(
            ProcessId id,
            std::string name,
            std::vector<Block> code,
            Priority priority = Priority{},
            Tick arrivalTime = 0
        );

        ~ProcessImage();

        // Non-copyable
        ProcessImage(const ProcessImage &) = delete;
        ProcessImage &operator=(const ProcessImage &) = delete;

        // Movable
        ProcessImage(ProcessImage &&) noexcept;
        ProcessImage &operator=(ProcessImage &&) noexcept;

        // --- Component access ---

        /// @brief Returns a const reference to the PCB.
        [[nodiscard]] const PCB &pcb() const noexcept;

        /// @brief Returns a mutable reference to the PCB.
        [[nodiscard]] PCB &pcb() noexcept;

        /// @brief Returns a const reference to the register file.
        [[nodiscard]] const RegisterFile &registers() const noexcept;

        /// @brief Returns a mutable reference to the register file.
        [[nodiscard]] RegisterFile &registers() noexcept;

        // --- Code segment ---

        /// @brief Returns a const reference to the code segment.
        [[nodiscard]] const std::vector<Block> &code() const noexcept;

        /// @brief Returns the size of the code segment (number of Blocks).
        [[nodiscard]] std::size_t codeSize() const noexcept;

        /// @brief Reads a single Block from the code segment.
        /// @param offset Index into the code segment.
        /// @return The Block at the given offset.
        /// @pre offset < codeSize()
        [[nodiscard]] const Block &readCode(std::size_t offset) const;

        /// @brief Replaces the code segment with new code.
        /// @param newCode The replacement program.
        void setCode(std::vector<Block> newCode);

        // --- Convenience accessors (delegate to PCB) ---

        /// @brief Returns the process ID.
        [[nodiscard]] ProcessId id() const noexcept;

        /// @brief Returns the process name.
        [[nodiscard]] std::string_view name() const noexcept;

        /// @brief Returns the current process state.
        [[nodiscard]] ProcessState state() const noexcept;

        /// @brief Returns the process priority.
        [[nodiscard]] const Priority &priority() const noexcept;

        private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace contur
