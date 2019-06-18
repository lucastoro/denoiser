#!/bin/sh

YAML_CPP_OPTIONS='-DYAML_CPP_BUILD_TESTS=OFF -DYAML_CPP_BUILD_TOOLS=OFF -DYAML_CPP_INSTALL=OFF -DMSVC_SHARED_RT=OFF'
GTETST_OPTIONS='-DINSTALL_GTEST=OFF'
GLOBAL_OPTIONS='-DCMAKE_BUILD_TYPE=Release'
CMAKE_OPTIONS="$YAML_CPP_OPTIONS $GTETST_OPTIONS $GLOBAL_OPTIONS"
MAKE_OPTIONS="-j \$(grep processor /proc/cpuinfo | wc -l)"
DOCKERIFY_SCRIPT='https://raw.githubusercontent.com/lucastoro/dockerifier/master/dockerify.sh'
DOCKERIFIER=$(date +%N).sh
ENTRYPOINT=$(date +%N).sh
DOCKERFILE=Dockerfile.$(date +%N)
CONTAINER=denoiser-dockerifier-$(date +%N)

command -v docker || {
  echo "docker not found"
  exit 1
}

(docker images | grep '^docker ' | grep latest) && DOCKER_IMAGE_EXISTS=true || DOCKER_IMAGE_EXISTS=false

[ -f CMakeLists.txt ] && (grep 'project(artifact-denoiser)' CMakeLists.txt) && LOCAL_REPO=true || LOCAL_REPO=false

$LOCAL_REPO && DOCKER_EXTRA="-v $PWD:/denoiser:ro"

atexit() {
  rm -f $ENTRYPOINT $DOCKERIFIER $DOCKERFILE
  docker image rm $CONTAINER
  $DOCKER_IMAGE_EXISTS || docker image rm docker:latest
}

set -e

wget $DOCKERIFY_SCRIPT -O $DOCKERIFIER

cat > $ENTRYPOINT << EOF
#!/bin/sh
set -ex
$LOCAL_REPO || {
  git clone --recurse-submodules --depth 1 https://github.com/lucastoro/denoiser.git
}
mkdir build && cd build
cmake $CMAKE_OPTIONS -DDENOISER_TESTS=ON ../denoiser
make $MAKE_OPTIONS
./artifact-denoiser -d ../denoiser --test
cmake $CMAKE_OPTIONS -DDENOISER_TESTS=OFF ../denoiser
make $MAKE_OPTIONS
strip ./artifact-denoiser
/$DOCKERIFIER ./artifact-denoiser
EOF

cat > $DOCKERFILE << EOF
FROM docker:latest
RUN apk add --no-cache gcc g++ make cmake git curl-dev
COPY $ENTRYPOINT /
COPY $DOCKERIFIER /
RUN chmod +x /$DOCKERIFIER
ENTRYPOINT ["/bin/sh", "/$ENTRYPOINT"]
EOF

trap "atexit" EXIT
docker build -t $CONTAINER -f $DOCKERFILE .
docker run --rm -it $DOCKER_EXTRA -v /var/run/docker.sock:/var/run/docker.sock $CONTAINER
