global syscall_entry_asm
extern syscall_handler

syscall_entry_asm:
    ; 1. Процессор уже переключился в Ring 0, но нам нужно сохранить регистры пользователя,
    ; чтобы Си-код их не затер. Сохраняем RCX (там адрес возврата) и R11 (там флаги)
    push rcx
    push r11
    push rbp
    mov rbp, rsp

    ; 2. Вызываем нашу безопасную Си-функцию для обработки логики
    call syscall_handler

    ; 3. Восстанавливаем всё обратно после выполнения Си-кода
    mov rsp, rbp
    pop rbp
    pop r11
    pop rcx

    ; 4. Возвращаемся обратно в kernel_main по адресу из RCX через обычный ассемблерный прыжок!
    jmp rcx

