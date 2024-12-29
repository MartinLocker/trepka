#include "str.h"

int strpos(const char* haystack, const char* needle)
{
   char *p = strstr(haystack, needle);
   if (p)
      return p - haystack;
   return NOT_FOUND;
}

char* strreplace(char* s, const char* find, const char* replaceWith) {
  int i = strpos(s, find);
  if (i != NOT_FOUND) {
    int l = strlen(replaceWith);
    int v = strlen(find);
    int w = strlen(s) - i - v + 1;
    memmove(s + i + l, s + i + v, w);
    memcpy(s + i, replaceWith, l);
  }
  return s;
}

char* strreplace(char* s, const char* find, uint32_t x) {
  char d[16];
  sprintf(d, "%d", x);
  return strreplace(s, find, d);
}

char* strsplit(char *s, const char* d) {
  char* r;  
//  Serial.printf("s: %s, d: %s\r\n", s, d);
  r = strstr(s, d);
  if (r) 
    *r++ = 0;
  else 
    r = s;      
//  Serial.printf("s: %s, r: %s\r\n", s, r);
  return r;
}

char html5[256] = {0};

void url_encoder_rfc_tables_init(){
  for (int i = 0; i < 256; i++){
    html5[i] = isalnum(i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : (i == ' ') ? '+' : 0;
  }
}

char* urlencode(char* enc, char* s) {
  for (; *s; s++){
    if (html5[(unsigned char)*s]) {
      *enc = html5[(unsigned char)*s];
      enc++;
    } else  {
      sprintf(enc, "%%%02X", *s);
      enc += 3;
    }
  }
  *enc = 0;
  return(enc);
}
