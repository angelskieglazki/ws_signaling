# ============================================
# Этап 1: Сборка libdatachannel
# ============================================
FROM ubuntu:22.04 AS libdatachannel-builder

ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем зависимости для сборки libdatachannel
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    make \
    pkg-config \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

# Клонируем и собираем libdatachannel
WORKDIR /tmp
RUN git clone --recursive https://github.com/paullouisageneau/libdatachannel.git \
    && cd libdatachannel \
    && git submodule update --init \
    && mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_GNUTLS=0 \
    && make -j$(nproc) \
    && make install \
    && ldconfig

# ============================================
# Этап 2: Сборка signaling-server
# ============================================
FROM ubuntu:22.04 AS server-builder

ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем зависимости
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    make \
    pkg-config \
    libssl-dev \
    libasio-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Копируем собранную libdatachannel из первого этапа
COPY --from=libdatachannel-builder /usr/local/lib /usr/local/lib
COPY --from=libdatachannel-builder /usr/local/include /usr/local/include
COPY --from=libdatachannel-builder /usr/local/lib/cmake /usr/local/lib/cmake
RUN ldconfig

# Копируем исходники проекта
WORKDIR /app
COPY . .

# Собираем только сервер (без клиента)
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_CLIENT=OFF \
    && make signaling-server -j$(nproc)

# ============================================
# Этап 3: Финальный образ
# ============================================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем только runtime-зависимости
RUN apt-get update && apt-get install -y \
    libssl3 \
    libasio-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Копируем библиотеку libdatachannel
COPY --from=libdatachannel-builder /usr/local/lib /usr/local/lib
RUN ldconfig

# Копируем собранный сервер
COPY --from=server-builder /app/build/server/signaling-server /usr/local/bin/signaling-server

# Порт по умолчанию (измените при необходимости)
EXPOSE 8080

# Запускаем сервер
ENTRYPOINT ["signaling-server"]
