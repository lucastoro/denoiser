#!/bin/sh
cd $(dirname $(realpath $0))/..
docker build -t denoiser-test test/docker
docker run --rm -v $PWD:/shared:ro denoiser-test

