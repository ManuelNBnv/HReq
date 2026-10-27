#include "json.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================
 * Utilidades internas
 * ========================================================= */

static void skip_ws(const char **p) {
  while (**p && isspace((unsigned char)**p))
    (*p)++;
}

static int is_value_delimiter(char c) {
  return c == '\0' || c == ',' || c == '}' || c == ']' ||
         isspace((unsigned char)c);
}

static int parse_hex4(const char **p, unsigned int *value) {
  unsigned int result = 0;

  for (int i = 0; i < 4; i++) {
    unsigned char c = (unsigned char)(*p)[i];
    unsigned int digit;

    if (c >= '0' && c <= '9')
      digit = c - '0';
    else if (c >= 'a' && c <= 'f')
      digit = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      digit = c - 'A' + 10;
    else
      return 0;

    result = (result << 4) | digit;
  }

  *p += 4;
  *value = result;
  return 1;
}

static int append_utf8(char *out, int capacity, int *length,
                       unsigned int codepoint) {
  unsigned char bytes[4];
  int count;

  if (codepoint <= 0x7F) {
    bytes[0] = (unsigned char)codepoint;
    count = 1;
  } else if (codepoint <= 0x7FF) {
    bytes[0] = 0xC0 | (codepoint >> 6);
    bytes[1] = 0x80 | (codepoint & 0x3F);
    count = 2;
  } else if (codepoint <= 0xFFFF) {
    bytes[0] = 0xE0 | (codepoint >> 12);
    bytes[1] = 0x80 | ((codepoint >> 6) & 0x3F);
    bytes[2] = 0x80 | (codepoint & 0x3F);
    count = 3;
  } else if (codepoint <= 0x10FFFF) {
    bytes[0] = 0xF0 | (codepoint >> 18);
    bytes[1] = 0x80 | ((codepoint >> 12) & 0x3F);
    bytes[2] = 0x80 | ((codepoint >> 6) & 0x3F);
    bytes[3] = 0x80 | (codepoint & 0x3F);
    count = 4;
  } else {
    return 0;
  }

  if (out && (*length > capacity - 1 - count))
    return 0;

  if (out)
    memcpy(out + *length, bytes, (size_t)count);

  *length += count;
  return 1;
}

/* =========================================================
 * Cadenas JSON
 *
 * Decodifica escapes y secuencias Unicode \uXXXX.
 * Devuelve la longitud en bytes UTF-8, o -1 si falla.
 * ========================================================= */

static int parse_string(const char **p, char *out, int capacity) {
  if (!p || !*p || **p != '"')
    return -1;

  if (out && capacity <= 0)
    return -1;

  int length = 0;
  (*p)++;

  while (**p && **p != '"') {
    unsigned char c = (unsigned char)**p;
    unsigned int codepoint = c;

    (*p)++;

    if (c < 0x20)
      goto error;

    if (c == '\\') {
      c = (unsigned char)**p;

      if (!c)
        goto error;

      (*p)++;

      switch (c) {
      case '"':
        codepoint = '"';
        break;
      case '\\':
        codepoint = '\\';
        break;
      case '/':
        codepoint = '/';
        break;
      case 'b':
        codepoint = '\b';
        break;
      case 'f':
        codepoint = '\f';
        break;
      case 'n':
        codepoint = '\n';
        break;
      case 'r':
        codepoint = '\r';
        break;
      case 't':
        codepoint = '\t';
        break;

      case 'u': {
        if (!parse_hex4(p, &codepoint))
          goto error;

        /* Un sustituto alto requiere un sustituto bajo. */
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
          unsigned int low;

          if ((*p)[0] != '\\' || (*p)[1] != 'u')
            goto error;

          *p += 2;

          if (!parse_hex4(p, &low))
            goto error;

          if (low < 0xDC00 || low > 0xDFFF)
            goto error;

          codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
          goto error;
        }

        break;
      }

      default:
        goto error;
      }
    }

    if (!append_utf8(out, capacity, &length, codepoint))
      goto error;
  }

  if (**p != '"')
    goto error;

  (*p)++;

  if (out)
    out[length] = '\0';

  return length;

error:
  if (out && capacity > 0)
    out[length < capacity ? length : capacity - 1] = '\0';

  return -1;
}

/* =========================================================
 * Validación de números JSON
 * ========================================================= */

static int skip_number(const char **p) {
  const char *s = *p;

  if (*s == '-')
    s++;

  if (*s == '0') {
    s++;
    if (isdigit((unsigned char)*s))
      return 0;
  } else if (*s >= '1' && *s <= '9') {
    do {
      s++;
    } while (isdigit((unsigned char)*s));
  } else {
    return 0;
  }

  if (*s == '.') {
    s++;

    if (!isdigit((unsigned char)*s))
      return 0;

    do {
      s++;
    } while (isdigit((unsigned char)*s));
  }

  if (*s == 'e' || *s == 'E') {
    s++;

    if (*s == '+' || *s == '-')
      s++;

    if (!isdigit((unsigned char)*s))
      return 0;

    do {
      s++;
    } while (isdigit((unsigned char)*s));
  }

  if (!is_value_delimiter(*s))
    return 0;

  *p = s;
  return 1;
}

/* =========================================================
 * Saltar valores JSON sin almacenarlos
 * ========================================================= */

static int skip_value(const char **p);

static int skip_literal(const char **p, const char *literal) {
  size_t length = strlen(literal);

  if (strncmp(*p, literal, length) != 0)
    return 0;

  if (!is_value_delimiter((*p)[length]))
    return 0;

  *p += length;
  return 1;
}

static int skip_container(const char **p, char close) {
  char open = close == '}' ? '{' : '[';

  if (**p != open)
    return 0;

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

    /* No se permiten comas finales. */
    if (**p == close)
      return 0;
  }

  return 0;
}

static int skip_value(const char **p) {
  skip_ws(p);

  switch (**p) {
  case '"':
    return parse_string(p, NULL, 0) >= 0;

  case '{':
    return skip_container(p, '}');

  case '[':
    return skip_container(p, ']');

  case 't':
    return skip_literal(p, "true");

  case 'f':
    return skip_literal(p, "false");

  case 'n':
    return skip_literal(p, "null");

  default:
    return skip_number(p);
  }
}

/* =========================================================
 * Búsqueda de claves
 * ========================================================= */

static const char *find_in_object(const char *json, const char *key) {
  if (!json || !key)
    return NULL;

  const char *p = json;
  skip_ws(&p);

  if (*p != '{')
    return NULL;

  p++;
  skip_ws(&p);

  while (*p && *p != '}') {
    char current_key[256];

    if (parse_string(&p, current_key, (int)sizeof(current_key)) < 0)
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

      if (*p == '}')
        return NULL;
    } else if (*p != '}') {
      return NULL;
    } else {
      break;
    }
  }

  return NULL;
}

/*
 * Admite:
 *   "nombre"
 *   "usuario.nombre"
 *   "usuario.perfil.activo"
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

/* =========================================================
 * API pública de HReq JSON
 * ========================================================= */

int json_get_string(char *json, char *key, char *output, int output_capacity) {
  if (!output || output_capacity <= 0)
    return -1;

  output[0] = '\0';

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

  const char *end = p;

  if (*end == '-')
    end++;

  if (!isdigit((unsigned char)*end))
    return 0;

  if (*end == '0' && isdigit((unsigned char)end[1]))
    return 0;

  while (isdigit((unsigned char)*end))
    end++;

  /* Esta función devuelve enteros, no decimales ni exponentes. */
  if (*end == '.' || *end == 'e' || *end == 'E')
    return 0;

  const char *delimiter = end;
  skip_ws(&delimiter);

  if (!is_value_delimiter(*delimiter))
    return 0;

  errno = 0;

  char *converted_end;
  long value = strtol(p, &converted_end, 10);

  if (errno == ERANGE || converted_end != end || value < INT_MIN ||
      value > INT_MAX)
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

  if (strncmp(p, "true", 4) == 0 && is_value_delimiter(p[4])) {
    *output = 1;
    return 1;
  }

  if (strncmp(p, "false", 5) == 0 && is_value_delimiter(p[5])) {
    *output = 0;
    return 1;
  }

  return 0;
}

int json_is_null(char *json, char *key) {
  const char *p = find_value(json, key);

  if (!p || strncmp(p, "null", 4) != 0)
    return 0;

  return is_value_delimiter(p[4]);
}
