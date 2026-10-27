
#include <ctype.h>
#include <curl/curl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HREQ_CONNECT_TIMEOUT 10L
#define HREQ_TOTAL_TIMEOUT 30L
#define HREQ_USER_AGENT "HReq/1.0"

typedef struct {
  char *buffer;
  size_t capacity;
  size_t length;
  int overflow;
} HttpResponse;

static size_t http_write_callback(char *data, size_t size, size_t count,
                                  void *userdata) {
  HttpResponse *response = (HttpResponse *)userdata;

  if (size != 0 && count > SIZE_MAX / size) {
    response->overflow = 1;
    return 0;
  }

  size_t bytes = size * count;

  if (bytes > response->capacity - 1 - response->length) {
    response->overflow = 1;
    return 0;
  }

  memcpy(response->buffer + response->length, data, bytes);

  response->length += bytes;
  response->buffer[response->length] = '\0';

  return bytes;
}

static int method_is_valid(const char *method) {
  if (!method)
    return 0;

  return strcmp(method, "GET") == 0 || strcmp(method, "POST") == 0 ||
         strcmp(method, "PUT") == 0 || strcmp(method, "PATCH") == 0 ||
         strcmp(method, "DELETE") == 0;
}

static int contains_case_insensitive(const char *text, const char *search) {
  if (!text || !search || !*search)
    return 0;

  size_t search_length = strlen(search);

  while (*text) {
    size_t i = 0;

    while (i < search_length && text[i] &&
           tolower((unsigned char)text[i]) ==
               tolower((unsigned char)search[i])) {
      i++;
    }

    if (i == search_length)
      return 1;

    text++;
  }

  return 0;
}

static struct curl_slist *build_headers(const char *headers, int has_body) {
  struct curl_slist *list = NULL;
  const char *cursor = headers;
  int has_content_type = 0;

  while (cursor && *cursor) {
    const char *end = strchr(cursor, '\n');

    size_t length = end ? (size_t)(end - cursor) : strlen(cursor);

    while (length > 0 &&
           (cursor[length - 1] == '\r' || cursor[length - 1] == ' ' ||
            cursor[length - 1] == '\t')) {
      length--;
    }

    while (length > 0 && (*cursor == ' ' || *cursor == '\t')) {
      cursor++;
      length--;
    }

    if (length > 0) {
      char *line = (char *)malloc(length + 1);

      if (!line) {
        curl_slist_free_all(list);
        return NULL;
      }

      memcpy(line, cursor, length);
      line[length] = '\0';

      if (contains_case_insensitive(line, "Content-Type:")) {
        has_content_type = 1;
      }

      struct curl_slist *updated = curl_slist_append(list, line);

      free(line);

      if (!updated) {
        curl_slist_free_all(list);
        return NULL;
      }

      list = updated;
    }

    if (!end)
      break;
    cursor = end + 1;
  }

  if (has_body && !has_content_type) {
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

static int perform_request(char *url, char *method, char *request_body,
                           char *headers, char *response_buffer,
                           int response_capacity, int *status_code) {
  if (!url || !method || !response_buffer || response_capacity <= 0 ||
      !method_is_valid(method)) {
    return -1;
  }

  response_buffer[0] = '\0';

  if (status_code) {
    *status_code = 0;
  }

  CURL *curl = curl_easy_init();

  if (!curl) {
    return -1;
  }

  HttpResponse response = {response_buffer, (size_t)response_capacity, 0, 0};

  struct curl_slist *header_list = NULL;

  if (headers || request_body) {
    header_list = build_headers(headers, request_body != NULL);

    if (!header_list && (headers || request_body)) {
      curl_easy_cleanup(curl);
      return -1;
    }
  }

  CURLcode result = CURLE_OK;

#define HREQ_SETOPT(option, value)                                             \
  do {                                                                         \
    result = curl_easy_setopt(curl, option, value);                            \
    if (result != CURLE_OK)                                                    \
      goto cleanup;                                                            \
  } while (0)

  HREQ_SETOPT(CURLOPT_URL, url);
  HREQ_SETOPT(CURLOPT_USERAGENT, HREQ_USER_AGENT);
  HREQ_SETOPT(CURLOPT_WRITEFUNCTION, http_write_callback);
  HREQ_SETOPT(CURLOPT_WRITEDATA, &response);
  HREQ_SETOPT(CURLOPT_CONNECTTIMEOUT, HREQ_CONNECT_TIMEOUT);
  HREQ_SETOPT(CURLOPT_TIMEOUT, HREQ_TOTAL_TIMEOUT);
  HREQ_SETOPT(CURLOPT_FOLLOWLOCATION, 1L);
  HREQ_SETOPT(CURLOPT_MAXREDIRS, 10L);
  HREQ_SETOPT(CURLOPT_SSL_VERIFYPEER, 1L);
  HREQ_SETOPT(CURLOPT_SSL_VERIFYHOST, 2L);

  /* Solo permitir HTTP y HTTPS, también en redirecciones. */
  HREQ_SETOPT(CURLOPT_PROTOCOLS_STR, "http,https");
  HREQ_SETOPT(CURLOPT_REDIR_PROTOCOLS_STR, "http,https");

  if (header_list) {
    HREQ_SETOPT(CURLOPT_HTTPHEADER, header_list);
  }

  if (strcmp(method, "GET") != 0) {
    HREQ_SETOPT(CURLOPT_CUSTOMREQUEST, method);
  }

  if (request_body && strcmp(method, "GET") != 0) {
    size_t body_length = strlen(request_body);

    if (body_length > (size_t)LLONG_MAX) {
      result = CURLE_BAD_FUNCTION_ARGUMENT;
      goto cleanup;
    }

    HREQ_SETOPT(CURLOPT_POSTFIELDS, request_body);
    HREQ_SETOPT(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_length);
  }

  result = curl_easy_perform(curl);

  if (status_code && (result == CURLE_OK || response.overflow)) {
    long http_status = 0;

    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status) ==
        CURLE_OK) {
      *status_code = (int)http_status;
    }
  }

cleanup:
  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);

#undef HREQ_SETOPT

  if (response.overflow) {
    response_buffer[response.length] = '\0';
    return -2;
  }

  if (result != CURLE_OK) {
    return -1;
  }

  return (int)response.length;
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
  if (!json_body)
    return -1;

  return perform_request(url, "POST", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_put_json(char *url, char *json_body, char *body_buffer,
                  int body_capacity, int *status_code) {
  if (!json_body)
    return -1;

  return perform_request(url, "PUT", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_patch_json(char *url, char *json_body, char *body_buffer,
                    int body_capacity, int *status_code) {
  if (!json_body)
    return -1;

  return perform_request(url, "PATCH", json_body, NULL, body_buffer,
                         body_capacity, status_code);
}

int http_delete(char *url, char *body_buffer, int body_capacity,
                int *status_code) {
  return perform_request(url, "DELETE", NULL, NULL, body_buffer, body_capacity,
                         status_code);
}
