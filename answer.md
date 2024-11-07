# Inspect a user-process's page table
1. page at va 0x0, valid, contains text(code), permission is read | execute in user mode
2. page at va 0x1000, valid, contains data(initialized), permission is read | write in user mode
3. page at va 0x2000, valid, guard page, permission is read | write in supervisor mode but none in user mode
4. page at va 0x3000, valid, user stack, permission is read | write in user mode
5. page at va 0xFFFFE000, the last but one page, valid, trapframe page, permission is read | write in user mode
6. page at va 0xFFFFF000, the last page, valid, trampoline page, permission is read | execute in supervisor mode but none in user mode

