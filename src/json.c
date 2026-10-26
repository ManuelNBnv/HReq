
#include "json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **p) {
  while (**p && isspace((unsigned char)**p)) {
    (*p)++;
  }
}

static int parse_string(const char **p, char *out, int capacity) {
  if (**p != '"')
    return -1;

  (*p)++;
  int length = 0;

  while (**p && **p != '"') {
    unsigned char c = (unsigned char)*(*p)++;

    if (c == '\\') {
      c = (unsigned char)*(*p)++;

      if (!c)
        return -1;

      switch (c) {
      case '"':
        c = '"';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
      default:
        return -1;
      }
    }

    if (out) {
      if (length + 1 >= capacity)
        return -1;

      out[length] = (char)c;
    }

    length++;
  }

  if (**p != '"')
    return -1;

  (*p)++;

  if (out)
    out[length] = '\0';

  return length;
}

static int skip_value(const char **p);

static int skip_container(const char **p, char close) {
  (*p)++;
  skip_ws(p);

  if (**p == close) {
    (*p)++;
    return 1;
  }

  while (**p) {
    if (close == '}') {
      if (parse_string(p, NULL, 0) < 0)
        return 0;

      skip_ws(p);

      if (**p != ':')
        return 0;

      (*p)++;
      skip_ws(p);
    }

    if (!skip_value(p))
      return 0;

    skip_ws(p);

    if (**p == close) {
      (*p)++;
      return 1;
    }

    if (**p != ',')
      return 0;

    (*p)++;
    skip_ws(p);
  }

  return 0;
}

static int skip_value(const char **p) {
  skip_ws(p);

  if (**p == '"')
    return parse_string(p, NULL, 0) >= 0;

  if (**p == '{')
    return skip_container(p, '}');

  if (**p == '[')
    return skip_container(p, ']');

  if (!**p)
    return 0;

  while (**p && **p != ',' && **p != '}' && **p != ']' &&
         !isspace((unsigned char)**p)) {
    (*p)++;
  }

  return 1;
}

/* Busca una clave directamente dentro de un objeto JSON. */
static const char *find_in_object(const char *json, const char *key) {
  if (!json || !key || *json != '{')
    return NULL;

  const char *p = json + 1;
  skip_ws(&p);

  while (*p && *p != '}') {
    char current_key[256];

    if (parse_string(&p, current_key, sizeof(current_key)) < 0)
      return NULL;

    skip_ws(&p);

    if (*p != ':')
      return NULL;

    p++;
    skip_ws(&p);

    if (strcmp(current_key, key) == 0)
      return p;

    if (!skip_value(&p))
      return NULL;

    skip_ws(&p);

    if (*p == ',') {
      p++;
      skip_ws(&p);
    } else if (*p != '}') {
      return NULL;
    } else {
      break;
    }
  }

  return NULL;
}

/*
 * Busca claves normales y rutas anidadas:
 * "nombre", "usuario.nombre", "usuario.perfil.activo".
 */
static const char *find_value(const char *json, const char *key) {
  if (!json || !key || !*key)
    return NULL;

  const char *dot = strchr(key, '.');

  if (!dot)
    return find_in_object(json, key);

  size_t length = (size_t)(dot - key);

  if (length == 0 || length >= 256 || !dot[1])
    return NULL;

  char first_key[256];
  memcpy(first_key, key, length);
  first_key[length] = '\0';

  const char *value = find_in_object(json, first_key);

  if (!value || *value != '{')
    return NULL;

  return find_value(value, dot + 1);
}

int json_get_string(char *json, char *key, char *output, int output_capacity) {
  if (!output || output_capacity <= 0)
    return -1;

  const char *p = find_value(json, key);

  if (!p || *p != '"')
    return -1;

  return parse_string(&p, output, output_capacity);
}

int json_get_int(char *json, char *key, int *output) {
  if (!output)
    return 0;

  const char *p = find_value(json, key);

  if (!p)
    return 0;

  char *end;
  long value = strtol(p, &end, 10);

  if (end == p)
    return 0;

  while (*end && isspace((unsigned char)*end))
    end++;

  if (*end != ',' && *end != '}' && *end != ']' && *end != '\0')
    return 0;

  *output = (int)value;
  return 1;
}

int json_get_bool(char *json, char *key, int *output) {
  if (!output)
    return 0;

  const char *p = find_value(json, key);

  if (!p)
    return 0;

  if (strncmp(p, "true", 4) == 0 &&
      (p[4] == ',' || p[4] == '}' || p[4] == ']' || p[4] == '\0' ||
       isspace((unsigned char)p[4]))) {
    *output = 1;
    return 1;
  }

  if (strncmp(p, "false", 5) == 0 &&
      (p[5] == ',' || p[5] == '}' || p[5] == ']' || p[5] == '\0' ||
       isspace((unsigned char)p[5]))) {
    *output = 0;
    return 1;
  }

  return 0;
}

int json_is_null(char *json, char *key) {
  const char *p = find_value(json, key);

  if (!p || strncmp(p, "null", 4) != 0)
    return 0;

  char next = p[4];

  return next == ',' || next == '}' || next == ']' || next == '\0' ||
         isspace((unsigned char)next);
}
