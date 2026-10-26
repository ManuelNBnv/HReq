#include <ctype.h>
#include <curl/curl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct http_buffer {
  char *data;
  size_t capacity;
  size_t stored;
  int overflow;
};

static size_t http_write_callback(char *incoming, size_t size, size_t count,
                                  void *userdata) {
  struct http_buffer *buffer = (struct http_buffer *)userdata;
  size_t bytes = size * count;

  if (!buffer || !buffer->data || buffer->capacity == 0)
    return 0;

  size_t available = buffer->capacity - 1 - buffer->stored;

  if (bytes > available) {
    if (available > 0) {
      memcpy(buffer->data + buffer->stored, incoming, available);
      buffer->stored += available;
    }

    buffer->data[buffer->stored] = '\0';
    buffer->overflow = 1;

    // Detiene la transferencia si no cabe la respuesta.
    return 0;
  }

  if (bytes > 0) {
    memcpy(buffer->data + buffer->stored, incoming, bytes);

    buffer->stored += bytes;
    buffer->data[buffer->stored] = '\0';
  }

  return bytes;
}

static int method_is_valid(const char *method) {
  return method &&
         (strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0 ||
          strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0 ||
          strcmp(method, "DELETE") == 0);
}

static struct curl_slist *build_headers(const char *headers,
                                        int add_json_content_type) {
  struct curl_slist *list = NULL;

  if (headers && *headers) {
    size_t length = strlen(headers);
    char *copy = (char *)malloc(length + 1);

    if (!copy)
      return NULL;

    memcpy(copy, headers, length + 1);

    char *line = copy;

    while (line && *line) {
      char *next = strchr(line, '\n');

      if (next) {
        *next = '\0';
        next++;
      }

      size_t line_length = strlen(line);

      while (line_length > 0 &&
             (line[line_length - 1] == '\r' ||
              isspace((unsigned char)line[line_length - 1]))) {
        line[--line_length] = '\0';
      }

      while (*line && isspace((unsigned char)*line))
        line++;

      if (*line) {
        struct curl_slist *updated = curl_slist_append(list, line);

        if (!updated) {
          curl_slist_free_all(list);
          free(copy);
          return NULL;
        }

        list = updated;
      }

      line = next;
    }

    free(copy);
  }

  if (add_json_content_type &&
      (!headers || !strstr(headers, "Content-Type:"))) {
    struct curl_slist *updated =
        curl_slist_append(list, "Content-Type: application/json");

    if (!updated) {
      curl_slist_free_all(list);
      return NULL;
    }

    list = updated;
  }

  return list;
}

static int perform_request(const char *url, const char *method,
                           const char *request_body, const char *headers,
                           char *response_buffer, int response_capacity,
                           int *status_code) {
  if (!url || !*url || !method_is_valid(method) || !response_buffer ||
      response_capacity <= 0) {
    return -1;
  }

  response_buffer[0] = '\0';

  if (status_code)
    *status_code = 0;

  CURL *curl = curl_easy_init();

  if (!curl)
    return -1;

  struct http_buffer buffer;
  buffer.data = response_buffer;
  buffer.capacity = (size_t)response_capacity;
  buffer.stored = 0;
  buffer.overflow = 0;

  int has_body = request_body != NULL &&
                 (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 ||
                  strcmp(method, "PATCH") == 0);

  struct curl_slist *header_list = build_headers(headers, has_body);

  if (headers && *headers && !header_list) {
    curl_easy_cleanup(curl);
    return -1;
  }

  if (has_body && !header_list) {
    curl_easy_cleanup(curl);
    return -1;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "itsuki-http/0.2.0");

  // Mantiene la verificación TLS activada.
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  if (header_list)
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

  if (strcmp(method, "GET") != 0)
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

  if (has_body) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                     (curl_off_t)strlen(request_body));
  }

  CURLcode result = curl_easy_perform(curl);
  long response_status = 0;

  if (result == CURLE_OK || result == CURLE_WRITE_ERROR) {
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_status);

    if (status_code)
      *status_code = (int)response_status;
  }

  if (header_list)
    curl_slist_free_all(header_list);

  curl_easy_cleanup(curl);

  if (buffer.overflow)
    return -2;

  if (result != CURLE_OK)
    return -1;

  return (int)buffer.stored;
}

int http_request(char *url, char *method, char *request_body, char *headers,
                 char *response_buffer, int response_capacity,
                 int *status_code) {
  return perform_request(url, method, request_body, headers, response_buffer,
                         response_capacity, status_code);
}

int http_get(char *url, char *body_buffer, int body_capacity,
             int *status_code) {
  return perform_request(url, "GET", NULL, NULL, body_buffer, body_capacity,
                         status_code);
}

int http_post_json(char *url, char *json_body, char *body_buffer,
                   int body_capacity, int *status_code) {
  return perform_request(url, "POST", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_put_json(char *url, char *json_body, char *body_buffer,
                  int body_capacity, int *status_code) {
  return perform_request(url, "PUT", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_patch_json(char *url, char *json_body, char *body_buffer,
                    int body_capacity, int *status_code) {
  return perform_request(url, "PATCH", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_delete(char *url, char *body_buffer, int body_capacity,
                int *status_code) {
  return perform_request(url, "DELETE", NULL, NULL, body_buffer, body_capacity,
                         status_code);
}