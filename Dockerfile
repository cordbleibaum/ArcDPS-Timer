FROM fedora:38 as builder

WORKDIR /build
RUN dnf install -y git zip cmake g++
RUN git clone https://github.com/Microsoft/vcpkg.git
RUN ./vcpkg/bootstrap-vcpkg.sh

COPY ArcDPS-Timer-Server .
COPY CMakeLists.txt .
RUN cmake . -DCMAKE_TOOLCHAIN_FILE=/build/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN ls