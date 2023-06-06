FROM fedora:38 as builder

WORKDIR /build
RUN dnf install -y git zip cmake g++
RUN git clone https://github.com/Microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh

COPY ArcDPS-Timer-Server .
RUN cmake . -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN make

FROM fedora:38

WORKDIR /app
COPY --from=builder /build/arcdps-timer-server .

EXPOSE 5000

CMD ["/app/arcdps-timer-server"]