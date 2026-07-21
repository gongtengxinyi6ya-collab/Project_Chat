ARG ROCKY_VERSION=10

FROM rockylinux/rockylinux:${ROCKY_VERSION} AS builder

ARG HIREDIS_VERSION=1.3.0
ARG REDIS_PLUS_PLUS_VERSION=1.3.15

RUN dnf install -y dnf-plugins-core \
    && dnf config-manager --set-enabled crb \
    && dnf install -y \
        gcc gcc-c++ cmake ninja-build make \
        git curl tar gzip \
        openssl-devel zlib-devel \
        ca-certificates \
    && dnf clean all \
    && rm -rf /var/cache/dnf

# 使用当前 MySQL EL10 仓库配置，避免旧的 2023 过期密钥。
RUN dnf install -y \
        https://repo.mysql.com/mysql84-community-release-el10-3.noarch.rpm \
    && dnf install -y \
        mysql-connector-c++ \
        mysql-connector-c++-jdbc \
        mysql-connector-c++-devel \
    && dnf clean all

# 编译 hiredis。
RUN curl -fsSL \
        -o /tmp/hiredis.tar.gz \
        https://github.com/redis/hiredis/archive/refs/tags/v${HIREDIS_VERSION}.tar.gz \
    && tar -xzf /tmp/hiredis.tar.gz -C /tmp \
    && cmake \
        -S /tmp/hiredis-${HIREDIS_VERSION} \
        -B /tmp/hiredis-build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_INSTALL_LIBDIR=lib64 \
        -DDISABLE_TESTS=ON \
    && cmake --build /tmp/hiredis-build \
    && cmake --install /tmp/hiredis-build

# 编译 redis-plus-plus。
RUN curl -fsSL \
        -o /tmp/redis-plus-plus.tar.gz \
        https://github.com/sewenew/redis-plus-plus/archive/refs/tags/${REDIS_PLUS_PLUS_VERSION}.tar.gz \
    && tar -xzf /tmp/redis-plus-plus.tar.gz -C /tmp \
    && cmake \
        -S /tmp/redis-plus-plus-${REDIS_PLUS_PLUS_VERSION} \
        -B /tmp/redis-plus-plus-build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DCMAKE_INSTALL_LIBDIR=lib64 \
        -DCMAKE_PREFIX_PATH=/usr/local \
        -DREDIS_PLUS_PLUS_CXX_STANDARD=20 \
        -DREDIS_PLUS_PLUS_BUILD_TEST=OFF \
        -DREDIS_PLUS_PLUS_BUILD_STATIC=OFF \
    && cmake --build /tmp/redis-plus-plus-build \
    && cmake --install /tmp/redis-plus-plus-build \
    && ldconfig

WORKDIR /src
COPY . /src

RUN cmake \
        -S /src \
        -B /src/build-release \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_SQL=ON \
        -DENABLE_REDIS=ON \
        -DENABLE_ASAN=OFF \
        -Dmysql-concpp_DIR=/usr/lib64/cmake/mysql-concpp \
        -DCMAKE_PREFIX_PATH=/usr/local \
    && cmake --build /src/build-release \
        --target server \
        --parallel

FROM rockylinux/rockylinux:${ROCKY_VERSION} AS runtime

RUN dnf install -y \
        libstdc++ openssl-libs zlib \
        ca-certificates shadow-utils bash grep \
    && dnf clean all

COPY --from=builder /usr/lib64/libmysqlcppconn*.so* /usr/lib64/
COPY --from=builder /usr/local/lib64/libhiredis.so* /usr/local/lib64/
COPY --from=builder /usr/local/lib64/libredis++.so* /usr/local/lib64/

RUN echo "/usr/local/lib64" > /etc/ld.so.conf.d/usr-local-lib64.conf \
    && ldconfig \
    && useradd \
        --system \
        --uid 10001 \
        --home-dir /app \
        --shell /sbin/nologin \
        chat

WORKDIR /app

COPY --from=builder --chown=chat:chat \
    /src/build-release/server /app/server

COPY --chown=chat:chat \
    config/config.json /app/config/config.json

USER chat

EXPOSE 8080

STOPSIGNAL SIGTERM

HEALTHCHECK --interval=30s --timeout=3s --start-period=15s --retries=3 \
    CMD bash -c 'exec 3<>/dev/tcp/127.0.0.1/8080'

ENTRYPOINT ["/app/server"]