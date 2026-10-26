# itsuki-http

Biblioteca HTTP para el lenguaje de programación Itsuki, basada en
[libcurl](https://curl.se/libcurl/).

Permite realizar peticiones HTTP, enviar JSON, configurar cabeceras
personalizadas y procesar respuestas JSON mediante el parser incluido.

## Características

- Peticiones GET, POST, PUT, PATCH y DELETE.
- Cabeceras HTTP personalizadas.
- Envío de cuerpos JSON.
- Recepción de respuestas en buffers administrados por el usuario.
- Código de estado HTTP.
- Detección de errores de conexión y respuestas demasiado grandes.
- Parser JSON incluido.
- Ejemplos escritos en Itsuki.

## Estructura del proyecto

```text
itsuki-http/
├── examples/
│   ├── client.suki
│   ├── api.suki
│   └── methods.suki
├── img/
│   └── itsuki_logo.png
├── include/
│   ├── http.huki
│   ├── json.h
│   └── json.huki
├── src/
│   ├── http.c
│   └── json.c
├── LICENSE
├── Makefile
└── README.md
```

## Requisitos

- Compilador Itsuki disponible mediante el comando `itsuki`.
- GCC o un compilador C compatible.
- libcurl con las bibliotecas de desarrollo necesarias.
- Make.

El Makefile de este proyecto comprueba la presencia de libcurl de
32 bits en `/usr/lib/i386-linux-gnu/libcurl.so.4` y utiliza las
cabeceras de `/usr/include/x86_64-linux-gnu`. Es posible que debas
adaptar esas rutas a tu distribución y a la arquitectura de tu
compilador.

## Compilación

Desde la carpeta del proyecto:

```bash
make
```

Esto compila los tres ejemplos disponibles.

Para compilar un ejemplo individual:

```bash
make build-client
make build-api
make build-methods
```

Para ejecutar los ejemplos:

```bash
make run-client
make run-api
make run-methods
```

Para eliminar los ejecutables generados:

```bash
make clean
```

## Uso de la biblioteca HTTP

Incluye la interfaz HTTP en tu archivo Itsuki:

```c
#include "../include/http.huki"
```

Las funciones reciben buffers que administra el programa que realiza
la petición. Asegúrate de proporcionar suficiente espacio para la
respuesta.

### GET

```c
char response[4096];
int status = 0;

int received = http_get(
    "https://jsonplaceholder.typicode.com/posts/1",
    response,
    4096,
    &status
);

if (received >= 0) {
    printf("HTTP: %d\n", status);
    printf("%s\n", response);
}
```

### POST con JSON

```c
char response[4096];
int status = 0;

int received = http_post_json(
    "https://jsonplaceholder.typicode.com/posts",
    "{\"title\":\"Hola\",\"body\":\"Prueba\",\"userId\":1}",
    response,
    4096,
    &status
);

if (received >= 0) {
    printf("HTTP: %d\n", status);
    printf("%s\n", response);
}
```

### PUT, PATCH y DELETE

Las funciones disponibles son:

```c
http_put_json(url, json_body, response, capacity, &status);
http_patch_json(url, json_body, response, capacity, &status);
http_delete(url, response, capacity, &status);
```

Las firmas completas están declaradas en `include/http.huki`.

## Peticiones personalizadas

Usa `http_request()` para seleccionar el método y enviar cabeceras
personalizadas.

```c
char response[4096];
char* headers = "Accept: application/json\nX-App: Itsuki";
int status = 0;

int received = http_request(
    "https://jsonplaceholder.typicode.com/posts/1",
    "GET",
    NULL,
    headers,
    response,
    4096,
    &status
);

if (received >= 0) {
    printf("HTTP: %d\n", status);
    printf("%s\n", response);
}
```

Las cabeceras se separan mediante saltos de línea (`\n`).

Para enviar un token de autenticación, puedes añadir una cabecera
como `Authorization: Bearer TU_TOKEN`. No publiques tokens reales
en repositorios públicos.

## Interpretación de resultados

Las funciones HTTP devuelven:

| Resultado | Significado |
|---|---|
| `0` o superior | Cantidad de bytes recibidos |
| `-1` | Error de petición o conexión |
| `-2` | La respuesta supera la capacidad del buffer |

El código HTTP se devuelve mediante `status_code`.

Un código HTTP como `404` o `500` no implica necesariamente un error
de conexión: la petición puede haber terminado correctamente y la
función puede devolver la cantidad de bytes recibidos.

Si la conexión falla antes de obtener una respuesta HTTP, el código
de estado normalmente permanece en `0`.

## Parser JSON

El proyecto incluye un parser JSON en:

- `include/json.h`
- `include/json.huki`
- `src/json.c`

El ejemplo `examples/api.suki` combina una petición HTTP con el
parser para leer datos de una API.

Consulta `examples/api.suki` para ver el uso concreto de la interfaz
JSON incluida en este proyecto.

## Ejemplos

| Archivo | Propósito |
|---|---|
| `examples/client.suki` | Cliente HTTP básico |
| `examples/api.suki` | Petición HTTP y lectura de JSON |
| `examples/methods.suki` | Pruebas de los métodos HTTP |

Los ejemplos utilizan JSONPlaceholder, un servicio público de
pruebas. Sus operaciones de escritura son simuladas y no guardan
cambios permanentes en una base de datos.

## Licencia

Consulta el archivo `LICENSE` para conocer los términos de uso,
modificación y distribución.

<p align="center"> <img src="img/itsuki_logo.png" alt="Itsuki Programming Language" width="160"> </p>

<h1 align="center">Itsuki Programming Language</h1>
