1. **Move `task_irq_restore(irq_flags)` to after `schedule()` in `futex_wait`:** This fixes a critical race condition where a timer interrupt can wake up a blocked task before it voluntarily yields the CPU, causing it to be scheduled immediately but bypassing the `schedule()` call entirely, and then when `schedule()` is eventually called, it's not marked for wake up anymore, causing it to hang.

2. **Move `task_irq_restore(irq_flags)` to after `schedule()` in `sem_wait`:** Same race condition as `futex_wait`. By restoring interrupts after `schedule()`, we ensure that the task correctly switches out while interrupts are disabled, and timer interrupts are queued until it resumes.

3. **Move `task_irq_restore(irq_flags)` to after `schedule()` in `task_exit_internal`:** This aligns with the rest of the scheduler design.

4. **Run `pre_commit_instructions`** to complete verification and testing.
5. **Submit changes** with `submit`.
