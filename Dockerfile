FROM conanio/gcc8:latest as builder

ARG CONAN_LOCAL_REPO= 
ARG CONAN_REPO_USER= 
ARG CONAN_REPO_PASS=  
ARG VERSION=

USER root

RUN apt-get update && apt-get install chrpath;

USER conan
ADD --chown=conan:1001 . /home/conan/src

RUN set -ex; cd /home/conan/src; ls -la;                                                                                  \
  LOCAL_REPO=$CONAN_LOCAL_REPO;                                                                                           \
  LOCAL_REPO_USER=$CONAN_REPO_USER;                                                                                       \
  LOCAL_REPO_PASS=$CONAN_REPO_PASS;                                                                                       \
  VERSION=$VERSION;                                                                                                       \
  PKGDIR=/tmp/token-rest;                                                                                                 \
  mkdir -p ${PKGDIR}/lib ${PKGDIR}/bin                                                                                    \
  && conan profile new --detect --force default                                                                           \
  && conan profile update settings.compiler.libcxx=libstdc++11 default                                                    \
  && conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan                                  \
  && conan remote add nlohmann-json https://api.bintray.com/conan/vthiery/conan-packages                                  \
  ; if test -n "${LOCAL_REPO}"; then                                                                                      \
    conan remote add local ${LOCAL_REPO};                                                                                 \
    if test -n $LOCAL_REPO_PASS; then                                                                                     \
      conan user -p $LOCAL_REPO_PASS -r local $LOCAL_REPO_USER;                                                           \
    fi;                                                                                                                   \
  fi                                                                                                                      \
  && conan install --build=missing .                                                                                      \
  && cmake . -DCMAKE_INSTALL_PREFIX=${PKGDIR} -DVERSION=$VERSION                                                          \
  && cmake --build .                                                                                                      \
  && cmake --install .                                                                                                    \
  && sed -ne '/\[libdirs\]/,/^$/ { /^\//p }' conanbuildinfo.txt | while read line; do                                     \
    ( cd "${line}"; ls . >/dev/stderr; tar cf - ./*.so* ) | ( cd "${PKGDIR}/lib"; tar xf - );                             \
  done                                                                                                                    \
  && find ${PKGDIR}                                                                                                       \
  ; chrpath -r \$ORIGIN:\$ORIGIN/../lib ${PKGDIR}/*/* || true  

FROM centos:8

RUN  mkdir -p /opt/token
COPY --from=builder /tmp/token-rest/ /opt/token
ENV  LC_ALL=C
ENV  LANGUAGE=
CMD  [ "/opt/token/bin/restsrv", "-m", "start", "-F" ]
