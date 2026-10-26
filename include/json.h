#ifndef JSON_H
#define JSON_H

int json_get_string(char *json, char *key, char *output, int output_capacity);

int json_get_int(char *json, char *key, int *output);
int json_get_bool(char *json, char *key, int *output);
int json_is_null(char *json, char *key);

#endif