#!/bin/bash

SELF=$0
SDIR=$( cd $( dirname "$SELF" ); pwd )
VERSION=$( git tag -l --points-at $( git rev-parse HEAD ) | tr '-' '.' )

source dbinit.sh

set -eu

function build( ) {
  local profile=${CONAN_USERNAME:-$USER}-$(
    case "$( git branch )" in
      *master*) echo "stable"
                ;;
      *) echo "testing"
         ;;
    esac )
  local commit=$( git rev-parse HEAD | cut -c 1-6 )$( git diff --quiet || echo "-dirty" )

  conan profile new --detect --force ${profile} || exit $?
  conan profile update settings.compiler.libcxx=libstdc++11 ${profile} || exit $?
  GCC_VER=$( gcc --version | awk '/^gcc/ { print $NF }' )
  if test "${GCC_VER//.*}" == "10"; then
    conan profile update settings.compiler.version=${GCC_VER%.*} ${profile} || exit $?
  fi

  test ! -d build && mkdir build

  (
    cd build

    conan install .. --build=missing -pr ${profile} -s build_type=${1:-Release}
    cmake .. -DVERSION=${VERSION} -DCOMMIT=${commit} -DCMAKE_BUILD_TYPE=${1:-Release}
    VERBOSE=1 cmake --build   .
    cmake --install .
  )

  start
  
  cp -v restsrv.ini build/bin/

  ( cd "${SDIR}/build";
    export LD_LIBRARY_PATH=${PWD}:${PWD}/lib:${PWD}/bin:${PWD}/src
    make test
    
    RET=$?
    
    if test ${RET} -ne 0; then
      cat Testing/Temporary/LastTest.log
    fi
    
    exit $RET
  ); RET=$?
  
  return $RET
}

function container( ) {
  docker build .                                                           \
    -t ${1:-restsrv:latest}                                                \
    ${CONAN_LOCAL_REPO:+--build-arg CONAN_LOCAL_REPO=${CONAN_LOCAL_REPO}}  \
    ${CONAN_REPO_USER:+--build-arg CONAN_REPO_USER=${CONAN_REPO_USER}}     \
    ${CONAN_REPO_PASS:+--build-arg CONAN_REPO_PASS=${CONAN_REPO_PASS}}     \
    --build-arg VERSION=${VERSION} "${@:2}"
}

case "${1:-local}" in
  local)
    build "${@:2}"
    ;;
  container)
    $1 "${@:2}"
    ;;
esac

