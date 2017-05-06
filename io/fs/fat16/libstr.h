#ifndef LIBSTR_H
#define LIBSTR_H

char * strcat(char *dest, const char *src);
bool streq(const char *str1, const char *str2);
void strcpy(char *dest, const char *source, int char_count=-1);
void substring(char *result, const char *source, int first_index, int latest_index);
bool extract_sublevel_filename(char *result, const char *source, natb level);

#endif