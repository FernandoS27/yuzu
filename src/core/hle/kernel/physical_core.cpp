// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/spin_lock.h"
#include "core/arm/arm_interface.h"
#ifdef ARCHITECTURE_x86_64
#include "core/arm/dynarmic/arm_dynarmic_32.h"
#include "core/arm/dynarmic/arm_dynarmic_64.h"
#endif
#include "core/arm/cpu_interrupt_handler.h"
#include "core/arm/exclusive_monitor.h"
#include "core/arm/unicorn/arm_unicorn.h"
#include "core/core.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/physical_core.h"
#include "core/hle/kernel/scheduler.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

PhysicalCore::PhysicalCore(
    Core::System& system, std::size_t id, Kernel::Scheduler& scheduler,
    std::array<Core::CPUInterruptHandler, Core::Hardware::NUM_CPU_CORES>& interrupt_handlers)
    : interrupt_handler{interrupt_handlers[id]}, core_index{id}, scheduler{scheduler} {
    auto& kernel = system.Kernel();
#ifdef ARCHITECTURE_x86_64
    arm_interface_32 = std::make_unique<Core::ARM_Dynarmic_32>(
        system, interrupt_handlers, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
    arm_interface_64 = std::make_unique<Core::ARM_Dynarmic_64>(
        system, interrupt_handlers, kernel.IsMulticore(), kernel.GetExclusiveMonitor(), core_index);
#else
    arm_interface_32 = std::make_shared<Core::ARM_Unicorn>(
        system, interrupt_handlers, kernel.IsMulticore(), ARM_Unicorn::Arch::AArch32, core_index);
    arm_interface_64 = std::make_shared<Core::ARM_Unicorn>(
        system, interrupt_handlers, kernel.IsMulticore(), ARM_Unicorn::Arch::AArch64, core_index);
    LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif

    guard = std::make_unique<Common::SpinLock>();
}

PhysicalCore::~PhysicalCore() = default;

void PhysicalCore::Run() {
    arm_interface->Run();
}

void PhysicalCore::Idle() {
    interrupt_handler.AwaitInterrupt();
}

void PhysicalCore::Shutdown() {
    scheduler.Shutdown();
}

void PhysicalCore::Interrupt() {
    guard->lock();
    interrupt_handler.SetInterrupt(true);
    guard->unlock();
}

void PhysicalCore::ClearInterrupt() {
    guard->lock();
    interrupt_handler.SetInterrupt(false);
    guard->unlock();
}

void PhysicalCore::SetIs64Bit(bool is_64_bit) {
    if (is_64_bit) {
        arm_interface = arm_interface_64.get();
    } else {
        arm_interface = arm_interface_32.get();
    }
}

} // namespace Kernel
