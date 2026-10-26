ITSUKI ?= itsuki

INCLUDES = -I. -I/usr/include/x86_64-linux-gnu
CURL_LIBS = -L. -lcurl

.PHONY: all check build-client build-api build-methods run-client run-api run-methods clean

all: build-client build-api build-methods

check:
	@command -v $(ITSUKI) >/dev/null || (echo "No se encontró Itsuki."; exit 1)
	@test -f /usr/lib/i386-linux-gnu/libcurl.so.4 || (echo "No se encontró libcurl de 32 bits."; exit 1)
	@test -f http.c || (echo "No se encontró http.c."; exit 1)
	@test -f http.huki || (echo "No se encontró http.huki."; exit 1)
	@echo "Itsuki, libcurl y archivos de la biblioteca detectados."

build-client: check
	$(ITSUKI) build examples/client.suki http.c $(INCLUDES) $(CURL_LIBS) -o examples/client

build-api: check
	$(ITSUKI) build examples/api.suki http.c json.c $(INCLUDES) $(CURL_LIBS) -o examples/api

build-methods: check
	$(ITSUKI) build examples/methods.suki http.c $(INCLUDES) $(CURL_LIBS) -o examples/methods

run-client: build-client
	./examples/client

run-api: build-api
	./examples/api

run-methods: build-methods
	./examples/methods

clean:
	rm -f examples/client examples/api examples/methods
