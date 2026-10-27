# HReq

HReq es una biblioteca HTTP para programas escritos en **Itsuki**. Usa **libcurl** para realizar peticiones HTTP/HTTPS e incluye funciones sencillas para trabajar con respuestas JSON.

## Funciones disponibles

### Peticiones HTTP

| Función | Uso |
|---|---|
| `http_get` | Realiza una petición GET. |
| `http_post_json` | Envía un cuerpo JSON mediante POST. |
| `http_put_json` | Envía un cuerpo JSON mediante PUT. |
| `http_patch_json` | Envía un cuerpo JSON mediante PATCH. |
| `http_delete` | Realiza una petición DELETE. |
| `http_request` | Permite elegir el método, el cuerpo y las cabeceras. |

Las respuestas se escriben en un buffer que proporciona el programa que hace la petición. Las funciones no reservan ese buffer por ti: asegúrate de que tenga capacidad suficiente.

### Interpretación del resultado

Las funciones HTTP devuelven:

| Retorno | Significado |
|---:|---|
| `0` o un número positivo | Cantidad de bytes recibidos. |
| `-1` | Error de petición o conexión. |
| `-2` | La respuesta supera la capacidad del buffer. |

El código HTTP se recibe por separado mediante el parámetro `status_code`. Por eso, un estado HTTP `404` o `500` no significa necesariamente que la conexión haya fallado: la petición puede haber terminado y devuelto un código HTTP de error.

### Parser JSON

HReq incluye estas funciones para consultar valores en un objeto JSON:

- `json_get_string(json, key, output, output_capacity)`
- `json_get_int(json, key, output)`
- `json_get_bool(json, key, output)`
- `json_is_null(json, key)`

Las búsquedas admiten claves anidadas con puntos, por ejemplo `usuario.perfil.activo`.

## Archivos del proyecto

La versión actual mantiene estos archivos en la raíz:

```text
HReq/
├── hreq.huki
├── http.c
├── json.huki
├── json.h
├── json.c
├── libcurl.so
├── img/
└── README.md
```

- `hreq.huki`: declaraciones de las funciones HTTP para Itsuki.
- `http.c`: implementación de las peticiones HTTP mediante libcurl.
- `json.huki`: declaraciones del parser JSON para Itsuki.
- `json.h` y `json.c`: interfaz e implementación del parser JSON.
- `libcurl.so`: biblioteca compartida de libcurl incluida en el repositorio.
- `img/`: recursos gráficos del proyecto.

## Requisitos

- El compilador de Itsuki (`itsuki`).
- Un compilador C compatible, como GCC.
- Las cabeceras de desarrollo de libcurl, incluida `curl/curl.h`.
- Una biblioteca libcurl compatible con la arquitectura y el entorno de enlace que use tu compilador de Itsuki.

La presencia de `libcurl.so` en el repositorio no garantiza por sí sola que todas sus dependencias del sistema estén instaladas. Si la compilación no encuentra `curl/curl.h` o falla al enlazar libcurl, instala las cabeceras y dependencias adecuadas para tu distribución y arquitectura.

## Uso desde Itsuki

Incluye las interfaces que vayas a utilizar:

```c
#include <hreq.huki>
#include <json.huki>
```

### Ejemplo: petición GET

```c
#include <fundamental.huki>
#include <hreq.huki>

fn main() -> int {
    var respuesta: char[4096];
    var estado: int = 0;

    var recibidos: int = http_get(
        "https://jsonplaceholder.typicode.com/posts/1",
        respuesta,
        4096,
        &estado
    );

    if (recibidos < 0) {
        printf("Error en la petición: %d\n", recibidos);
        return 1;
    }

    printf("Estado HTTP: %d\n", estado);
    printf("Respuesta: %s\n", respuesta);
    return 0;
}
```

### Ejemplo: POST con JSON

```c
#include <fundamental.huki>
#include <hreq.huki>

fn main() -> int {
    var respuesta: char[4096];
    var estado: int = 0;

    var recibidos: int = http_post_json(
        "https://jsonplaceholder.typicode.com/posts",
        "{\"title\":\"Hola\",\"body\":\"Prueba\",\"userId\":1}",
        respuesta,
        4096,
        &estado
    );

    if (recibidos < 0) {
        printf("Error en la petición: %d\n", recibidos);
        return 1;
    }

    printf("Estado HTTP: %d\n", estado);
    printf("Respuesta: %s\n", respuesta);
    return 0;
}
```

Estos ejemplos muestran el uso de las declaraciones de la API; para compilar, también hay que enlazar la implementación nativa de HReq y libcurl como se indica a continuación.

## Compilación

El compilador necesita encontrar `http.c` para resolver las funciones HTTP, y el enlazador necesita libcurl. Desde una instalación donde HReq esté en `~/.itsuki/packages/hreq`, una forma explícita de compilar es:

```bash
itsuki build client.suki \
  "$HOME/.itsuki/packages/hreq/http.c" \
  -I"$HOME/.itsuki/packages/hreq" \
  -I/usr/include/x86_64-linux-gnu \
  -L"$HOME/.itsuki/packages/hreq" \
  -lcurl \
  -o client
```

Adapta las rutas de las cabeceras y de la biblioteca a tu sistema. La ruta `/usr/include/x86_64-linux-gnu` es específica de algunas instalaciones Debian/Ubuntu y puede ser distinta en otras distribuciones.

**Nota sobre la compilación automática:** `hreq.huki` y `http.c` tienen nombres base diferentes. Si tu versión del compilador descubre automáticamente el archivo de implementación a partir del nombre del encabezado, no debes asumir que `hreq.huki` encontrará `http.c` por sí solo. Además, descubrir `http.c` no añade automáticamente la dependencia externa `-lcurl`. La forma de declarar dependencias automáticamente depende de las funciones que admita tu versión del gestor de paquetes de Itsuki.

## Cabeceras HTTP personalizadas

Usa `http_request` cuando necesites indicar el método o añadir cabeceras propias:

```c
var respuesta: char[4096];
var estado: int = 0;

var recibidos: int = http_request(
    "https://jsonplaceholder.typicode.com/posts/1",
    "GET",
    null,
    "Accept: application/json\nX-App: Itsuki",
    respuesta,
    4096,
    &estado
);
```

Las cabeceras se separan mediante saltos de línea (`\n`). En las peticiones con cuerpo, HReq agrega `Content-Type: application/json` si no se proporcionó una cabecera `Content-Type`.

No publiques tokens de acceso ni otros secretos en repositorios públicos.

## Seguridad y límites

- HReq configura tiempos máximos para la conexión y la petición.
- Sigue redirecciones HTTP/HTTPS, con un límite de redirecciones.
- Limita los protocolos permitidos a HTTP y HTTPS.
- Verifica los certificados TLS mediante las opciones de libcurl.
- Si la respuesta no cabe en el buffer, devuelve `-2`; aumenta el buffer o gestiona ese resultado.
- El código de estado HTTP se informa por separado del resultado de la operación.

## Licencia

Consulta `LICENSE.txt` para ver los términos de licencia incluidos con esta versión del proyecto.

<p align="center"> <img src="img/itsuki_logo.png" alt="Itsuki Programming Language" width="160"> </p>

<h1 align="center">Itsuki Programming Language</h1>
