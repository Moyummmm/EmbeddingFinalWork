FROM alpine:3.21 AS builder
RUN apk add --no-cache cmake make g++ linux-headers git

WORKDIR /src
COPY CMakeLists.txt .
COPY main.cpp protocol.h protocol.cpp registry.h registry.cpp server.h server.cpp .
RUN mkdir build && cd build && cmake .. && make -j

FROM alpine:3.21
RUN apk add --no-cache libstdc++
COPY --from=builder /src/build/server /usr/local/bin/peer-registry-server

EXPOSE 8888
ENTRYPOINT ["peer-registry-server"]
CMD ["8888"]
