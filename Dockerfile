FROM emscripten/emsdk:latest AS builder

WORKDIR /src

COPY . .

RUN emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build-web --target MinecraftWeb \
    && cp build-web/MinecraftWeb.html build-web/index.html

FROM nginx:1.27-alpine

COPY docker/nginx/default.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /src/build-web/ /usr/share/nginx/html/

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
