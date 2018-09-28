//
// Created by trippli on 9/6/18.
//

#pragma once

#include <unistd.h>

#ifndef assert_or_throw
#define assert_or_throw(state, desc) do{ \
        if (!(state)) { throw std::runtime_error(desc); } \
    } while(0);
#endif

#ifdef __x86_64
void patch_function(void *target_func, void *override_func, char *backup, size_t backup_len);
#else
#error "Only x86_64 is currently supported, sorry."
#endif
