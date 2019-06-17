#!/bin/bash

#!/bin/sh

command -v docker &>/dev/null || {
  echo "docker not found" && exit 1
}

(docker images | grep '^docker ' | grep latest &>/dev/null) && {
  DOCKER_IMAGE_EXISTS=true
} || {
  DOCKER_IMAGE_EXISTS=false
}

echo 'YAML_CPP_OPTIONS="-DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_CPP_INSTALL=OFF -DMSVC_SHARED_RT=OFF"
GTETST_OPTIONS="-DINSTALL_GTEST=OFF"
DENOISER_OPTIONS="-DDENOISER_TESTS=ON"
GLOBAL_OPTIONS="-DCMAKE_BUILD_TYPE=Release"
CMAKE_OPTIONS="$YAML_CPP_OPTIONS $GTETST_OPTIONS $DENOISER_OPTIONS $GLOBAL_OPTIONS"
MAKE_OPTIONS="-j $(grep processor /proc/cpuinfo | wc -l)"

set -ex
git clone --recurse-submodules --depth 1 https://github.com/lucastoro/denoiser.git
cd denoiser
mkdir build
cd build
cmake $CMAKE_OPTIONS ..
make $MAKE_OPTIONS
./artifact-denoiser -d .. --test
wget https://raw.githubusercontent.com/lucastoro/dockerifier/b68da4ac2930e98fd52448303933e2759e29ac90/dockerify.sh
chmod +x ./dockerify.sh
./dockerify.sh ./artifact-denoiser' > entrypoint.sh

echo 'FROM docker:latest
RUN apk add --no-cache gcc g++ make cmake git curl-dev
COPY entrypoint.sh /
ENTRYPOINT ["/bin/sh", "/entrypoint.sh"]' > Dockerfile

docker build -t denoiser-dockerifier .
result=$?
rm entrypoint.sh Dockerfile
[ $result -ne 0 ] && exit $result
docker run --rm -it -v /var/run/docker.sock:/var/run/docker.sock denoiser-dockerifier
result=$?
docker image rm denoiser-dockerifier
$DOCKER_IMAGE_EXISTS || docker image rm docker:latest
exit $result
