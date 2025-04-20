# This file contains definitions of various custom GDB commands
# to aid in debugging our kernel. Information on how to write files
# like these can be found in the GDB manual
# (https://sourceware.org/gdb/current/onlinedocs/gdb).

# To use this file, either source it using the command "source gdb_scripts"
# or source it when running gdb by passing the "-x gdb_scipts" option to gdb
# when running it. (E.g. "gdb -x gdb_scipts bootimg")

define save_old_state
    if $save_old_state
        set $old_ds = $ds
        set $old_es = $es
        set $old_fs = $fs
        set $old_gs = $gs
        set $old_cs = $cs
        set $old_ss = $ss
        set $old_eax = $eax
        set $old_ebx = $ebx
        set $old_ecx = $ecx
        set $old_edx = $edx
        set $old_ebp = $ebp
        set $old_esp = $esp
        set $old_esi = $esi
        set $old_edi = $edi
        set $old_eflags = $eflags
        set $old_eip = $eip
        set $save_old_state = 0
    end
end
document save_old_state
Saves the current CPU state to be restored by restore_old_state.

Command for saving the old CPU state if $save_old_state is true.
This is used to save the actual state of the CPU while we go off and inspect
things in the stack etc.
Invoke as follows: save_old_state
end

define restore_old_state
    if !$save_old_state
        set $ds = $old_ds
        set $es = $old_es
        set $fs = $old_fs
        set $gs = $old_gs
        set $cs = $old_cs
        set $ss = $old_ss
        set $eax = $old_eax
        set $ebx = $old_ebx
        set $ecx = $old_ecx
        set $edx = $old_edx
        set $esp = $old_esp
        set $ebp = $old_ebp
        set $esi = $old_esi
        set $edi = $old_edi
        set $eflags = $old_eflags
        set $eip = $old_eip
        set $save_old_state = 1
        printf "restored the old state from before popping\n"
    else
        printf "no state to restore to!\n"
    end
end
document restore_old_state
Restores the original process state from before popping contexts.

Command for restoring the original state from before we started popping contexts.
Useful to return back to the original execution context from before we inspected
things, to then resume execution.
Invoke as follows: restore_old_state
end

define pop_iret_context
    dont-repeat
    save_old_state
    set $context = (iret_context_base_t*)($arg0)
    set $ds = $context->ds
    set $es = $context->es
    set $fs = $context->fs
    set $gs = $context->gs
    set $eax = $context->eax
    set $ebx = $context->ebx
    set $ecx = $context->ecx
    set $edx = $context->edx
    set $ebp = $context->ebp
    set $esi = $context->esi
    set $edi = $context->edi
    set $eip = $context->eip
    set $eflags = $context->eflags
    if ($context->cs & 3) != ($cs & 3)
        # If we were in user mode, get esp from context
        set $esp = ((iret_context_user_t*)$context)->esp
        set $ss = ((iret_context_user_t*)$context)->ss
        set $cs = $context->cs
        printf "popped iret_context_user_t at address 0x%x\n", $context
    else
        # If we were in kernel mode, set esp to after context
        set $esp = $context + 1
        set $cs = $context->cs
        printf "popped iret_context_base_t at address 0x%x\n", $context
    end
end
document pop_iret_context
Pops the given iret_context pointer.

Command for popping off an iret_context, useful when wanting to inspect
what caused an exception.
Invoke as follows: pop_iret_context context
where context is an expression that evaluates to a pointer to either
an iret_context_base_t or an iret_context_user_t
end

define pop_stack_context
    dont-repeat
    save_old_state
    set $context = (context_t*)($arg0)
    set $esp = $context->esp
    set $eip = $context->eip
    if $eip == {uint32_t}&swap_context_return_landing_addr
        # pop off the registers pushed by swap_context too if the context points there
        set $stack_ptr = (uint32_t*)$esp
        set $edi = $stack_ptr[0]
        set $esi = $stack_ptr[1]
        set $ebx = $stack_ptr[2]
        set $eflags = $stack_ptr[3]
        set $ebp = $stack_ptr[4]
        # set $eip = &swap_context_pre_jump
    end
    printf "popped context_t at address 0x%x\n", $context
end
document pop_stack_context
Pops off the given context_t pointer.

Command for popping off a context_t (as defined in swtch.h),
useful for switching over and inspecting a different process's
current stack trace.
Invoke as follows: pop_stack_context context
where context is a pointer to a context_t struct
end

define jump_to_pcb
    dont-repeat
    set $pcb = (pcb_t*)($arg0)
    pop_stack_context &$pcb->context
end
document jump_to_pcb
Jumps to the kernel stack of the given PCB.

Command for jumping to the given PCB pointer.
Invoke as follows: jump_to_pcb <PCB>
where PCB is a pointer to a pcb_t
end

define jump_to_pid
    dont-repeat
    # Same math as the pid_to_pcb function
    set $pcb = &(kernel_stacks_start - (($arg0) + 1))->pcb
    # use a variable instead of directly putting the above as the arg
    # to the jump_to_pcb command, since command args do direct substitution
    jump_to_pcb $pcb
end
document jump_to_pid
Jumps to the kernel stack of the given PID.

Command for jumping to the given process ID.
Invoke as follows: jump_to_process <PID>
end

# Variable to keep track of whether we should save the old state
# when running commands like pop_iret_context. Normally, we only
# save the state if we have not ran such a pop command yet since
# the last time we ran restore_context
init-if-undefined $save_old_state = 1

printf "loaded in custom commands from gdb_scripts.gdb!\n\n"
