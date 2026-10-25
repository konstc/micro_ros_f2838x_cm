# FreeRTOS port for the F2838x CM (tiarmclang)

`port.c` and `portmacro.h` are the unmodified upstream FreeRTOS-Kernel files
`portable/GCC/ARM_CM3/` at tag **V11.3.0**.

Why not C2000Ware's own port: C2000Ware provides FreeRTOS V11.3.0 (identical
to upstream) with only a `CCS/ARM_CM4` port. That port uses legacy TI intrinsics
(`_set_interrupt_priority`, `__clz`) and a TI-syntax `portasm.asm`, neither of
which tiarmclang accepts. The CM is a Cortex-M4 without FPU, for which
`GCC/ARM_CM3` is FreeRTOS's port; it uses GNU inline assembly only, which
tiarmclang compiles.

The kernel sources (`tasks.c`, `queue.c`, `list.c`, `timers.c`,
`stream_buffer.c`, `event_groups.c`, `portable/MemMang/heap_4.c`) are taken from
C2000Ware through the `C2000WARE_ROOT`. When C2000Ware updates FreeRTOS, update
these two files to the matching tag.

The vector table must point SVCall, PendSV and SysTick directly at
`vPortSVCHandler`, `xPortPendSVHandler` and `xPortSysTickHandler`
(`configCHECK_HANDLER_INSTALLATION` verifies it at scheduler start).
