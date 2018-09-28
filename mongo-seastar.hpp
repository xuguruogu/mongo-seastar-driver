//
// Created by trippli on 9/6/18.
//

#pragma once

void mongoc_seastar_hack();
void mongoc_seastar_unhack();

void __attribute__((constructor)) on_load_hack() {
    mongoc_seastar_hack();
}

void __attribute__((destructor)) on_unload_hack() {
    mongoc_seastar_unhack();
}