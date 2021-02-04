#!/bin/bash

SUDO=
DOCKER=$( which docker )
PODMAN=$( which podman )

function freeport( ) {
    local start=${1:-5432}
    
    sudo netstat -tanep | awk -v port=${start} '
      /tcp/&&/LISTEN/ { 
        ports[ gensub( /.*:([0-9]+)/, "\\1", "g", $4 ) ] = 1 
      }
      END { 
        for( p=port;; ++p ) {
          if ( ! ( p in ports ) ) { 
            print p
            exit 0 
          } 
        }
      }'
}

function docker( ) {
    if test -z "${SUDO}"; then
        if test -z "${DOCKER}"; then
            DOCKER=${PODMAN}
        fi
        
        ${DOCKER} ps >& /dev/null
        
        if test $? -ne 0; then
            SUDO=$(which sudo)
        else
            SUDO=" "
        fi
    fi

    ${SUDO} ${DOCKER} $@
}

function pgport( ) {
    local port=$( docker ps -f name=${POSTGRESQL_CONTAINER} --format '{{.Ports}}' | sed 's,.*:\([0-9]\+\)->.*,\1,g' )
    echo ${port:-$( freeport 5432 )}
}

export POSTGRESQL_USERNAME=testdb
export POSTGRESQL_DATABASE=testdb
export POSTGRESQL_PASSWORD=123456
export POSTGRESQL_POSTGRES_PASSWORD=${POSTGRESQL_PASSWORD} 
export POSTGRESQL_CONTAINER=$( basename $PWD )-postgres
export POSTGRESQL_PORT=$( pgport )
export POSTGRESQL_HOSTNAME=localhost:${POSTGRESQL_PORT}

function is_running( ) {
    # Wait for it to become available
    psql <<< "SELECT 1;" >& /dev/null
    return $?
}

function postgres( ) {
    local FLAGS
    test -t 0 && FLAGS=-t || FLAGS=""
    docker exec ${FLAGS} -i -e PGPASSWORD=${POSTGRESQL_POSTGRES_PASSWORD} ${POSTGRESQL_CONTAINER} \
        psql -U postgres -d ${POSTGRESQL_DATABASE}
}

function psql( ) {
    local FLAGS
    test -t 0 && FLAGS=-t || FLAGS=""
    docker exec ${FLAGS} -i -e PGPASSWORD=${POSTGRESQL_PASSWORD} ${POSTGRESQL_CONTAINER} \
        psql -U ${POSTGRESQL_USERNAME} -d ${POSTGRESQL_DATABASE}
}

function bash( ) {
    local FLAGS
    test -t 0 && FLAGS=-t || FLAGS=""
    docker exec ${FLAGS} -i -e PGPASSWORD=${POSTGRESQL_PASSWORD} ${POSTGRESQL_CONTAINER} \
        /bin/bash
}

function start( ) {
    
    # Expand the postgres environment variables
    eval DOCKER_ENV=\"$( echo ${!POSTGRESQL*} | sed 's,\([^ ]\+\),-e \1=${\1},g' )\"
        
    if test -z "$( docker ps -f name="${POSTGRESQL_CONTAINER}" --format "{{.Names}}" )"; then
        # Execute the test database environment 
        docker run --rm -d --name ${POSTGRESQL_CONTAINER} -p ${POSTGRESQL_PORT}:5432 \
            ${DOCKER_ENV} docker.io/bitnami/postgresql:latest || exit $?
        
        # Wait for it to become available
        until is_running; do sleep 1; done

        postgres <<EOF || exit $?
CREATE EXTENSION pgcrypto;
EOF
        
        cat <<EOF > restsrv.ini
[database]
url = psql://${POSTGRESQL_USERNAME}:${POSTGRESQL_PASSWORD}@${POSTGRESQL_HOSTNAME}/${POSTGRESQL_DATABASE} 
pool_size = 10
[log]
level=trace
EOF
    fi
}

function stop( ) {
    docker kill ${POSTGRESQL_CONTAINER} >& /dev/null
}

function restart( ) {
    stop
    start
}

function dbinit_main( ) {
    case "$1" in
        start|stop|restart|psql|bash)
            $1
            ;;
    esac
}

(return 0 2>/dev/null) && sourced=1 || sourced=0

if test ${sourced} -eq 0; then
    dbinit_main "$@"
fi
