#ifndef STR_H
#define STR_H

#include <Arduino.h>
#include <string.h>

#define NOT_FOUND -1

int strpos(const char* haystack, const char* needle);
char* strreplace(char* s, const char* find, const char* replaceWith);
char* strreplace(char* s, const char* find, uint32_t x);
char* strsplit(char* s, const char* d);
char* urlencode(char* s, char* d);
void  url_encoder_rfc_tables_init();

#endif