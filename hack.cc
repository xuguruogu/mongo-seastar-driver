//
// Created by trippli on 9/6/18.
//

#include <cstdlib>
#include <sys/mman.h>
#include <cstring>
#include <system_error>
#include <iostream>
#include "hack.h"

void patch_function(void *target_func, void *override_func, char *backup, size_t backup_len) {
    auto page_size = static_cast<size_t>(sysconf(_SC_PAGE_SIZE));

    const unsigned char jmp_instr[] = {
            0xff, 0x25, 0x00, 0x00, 0x00, 0x00,                 /* jmpq *0x0(%rip) */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* <addr> placeholder */
    };

    if (backup_len < sizeof(jmp_instr)) {
        throw std::runtime_error("backup_len < sizeof(jmp_instr)");
    }

    auto page_aligned_start = (size_t) target_func & ~(page_size - 1);
    auto page_aligned_end = (size_t)((char*)target_func + sizeof(jmp_instr)) & ~(page_size  - 1);

    for(auto i = page_aligned_start; i <= page_aligned_end; i += page_size) {
        /* Unprotect target page */
        if (mprotect((void *)i, page_size, PROT_READ|PROT_WRITE|PROT_EXEC) < 0) {
            throw std::system_error(errno, std::system_category());
        }
    }

    if (!override_func) {
        memcpy(target_func, backup, sizeof(jmp_instr));
    } else {
        memcpy(backup, target_func, sizeof(jmp_instr));
        memcpy(target_func, jmp_instr, sizeof(jmp_instr));
        memcpy((char *)target_func + 6, &override_func, sizeof(void *));
    }

    for(auto i = page_aligned_start; i <= page_aligned_end; i += page_size) {
        /* Unprotect target page */
        if (mprotect((void *)i, page_size, PROT_READ|PROT_EXEC) < 0) {
            throw std::system_error(errno, std::system_category());
        }
    }
}