#!/bin/bash
# Usage: ./build-wheels.sh <workdir> <pyminorversion1> <pyminorversion2> ...
set -e -x

workdir=$1
shift

echo "Changing cwd to ${workdir}"
cd ${workdir}

yum install -y zlib-devel bzip2 bzip2-devel xz-devel curl-devel openssl-devel ncurses-devel

# downgrade autoconf to work more nicely with htslib
curl -L -O http://ftp.gnu.org/gnu/autoconf/autoconf-2.69.tar.gz
tar zxf autoconf-2.69.tar.gz
cd autoconf-2.69
yum install -y openssl-devel
./configure
make && make install
cd ..

make htslib/libhts.a pymod.a
mkdir -p wheelhouse

echo "PYTHON VERSIONS AVAILABLE"
ls /opt/python/

# Compile wheels
for minor in $@; do
    if [[ "${minor}" == "8" ]]  || [[ "${minor}" == "9" ]]; then
        PYBIN="/opt/python/cp3${minor}-cp3${minor}/bin"
    else
        PYBIN="/opt/python/cp3${minor}-cp3${minor}m/bin"
    fi
    # auditwheel/issues/102
    "${PYBIN}"/pip install --upgrade cffi setuptools pip wheel==0.31.1
    "${PYBIN}"/pip wheel --no-dependencies . -w ./wheelhouse/
done


# Bundle external shared libraries into the wheels
for whl in "wheelhouse/${PACKAGE_FILE_NAME}"*.whl; do
    auditwheel repair "${whl}" -w ./wheelhouse/
done


## Install packages
if [[ "${DO_COUNT_TEST}" == "1" ]]; then
    for minor in $@; do
        if [[ "${minor}" == "8" || "${minor}" == "9" ]]; then
            PYBIN="/opt/python/cp3${minor}-cp3${minor}/bin"
        else
            PYBIN="/opt/python/cp3${minor}-cp3${minor}m/bin"
        fi
        "${PYBIN}"/pip install -r requirements.txt 
        "${PYBIN}"/pip install "${PACKAGE_NAME}" --no-index -f ./wheelhouse
        "${PYBIN}"/medaka_counts --print medaka/test/data/test_reads.bam utg000001l:10000-10010
    done
fi

cd wheelhouse && ls | grep -v "${PACKAGE_FILE_NAME}.*manylinux" | xargs rm
