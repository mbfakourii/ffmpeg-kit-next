#!/bin/bash

# SET BUILD OPTIONS
ASM_OPTIONS=""
case ${ARCH} in
arm64)
  ASM_OPTIONS="linux-aarch64 enable-ec_nistp_64_gcc_128"
  ;;
x86-64)
  ASM_OPTIONS="linux-x86_64 enable-ec_nistp_64_gcc_128"
  ;;
esac
ZLIB_INCLUDE_DIRECTORY=$(pkg-config --variable=includedir zlib) || return 1
ZLIB_LIBRARY_DIRECTORY=$(pkg-config --variable=libdir zlib) || return 1
if [[ -z ${ZLIB_INCLUDE_DIRECTORY} ]] || [[ ! -f "${ZLIB_INCLUDE_DIRECTORY}/zlib.h" ]]; then
  echo "ERROR: zlib.h not found in the target zlib include directory: ${ZLIB_INCLUDE_DIRECTORY}" >&2
  return 1
fi
if [[ -z ${ZLIB_LIBRARY_DIRECTORY} ]] || [[ ! -d "${ZLIB_LIBRARY_DIRECTORY}" ]]; then
  echo "ERROR: Target zlib library directory not found: ${ZLIB_LIBRARY_DIRECTORY}" >&2
  return 1
fi

# ALWAYS CLEAN THE PREVIOUS BUILD
make distclean 2>/dev/null 1>/dev/null

# REGENERATE BUILD FILES IF NECESSARY OR REQUESTED
if [[ ! -f "${BASEDIR}"/src/"${LIB_NAME}"/configure ]] || [[ ${RECONF_openssl} -eq 1 ]]; then
  autoreconf_library "${LIB_NAME}" 1>>"${BASEDIR}"/build.log 2>&1 || return 1
fi

INT128_AVAILABLE=$($CC -dM -E - </dev/null 2>>"${BASEDIR}"/build.log | grep __SIZEOF_INT128__)

echo -e "INFO: __uint128_t detection output: $INT128_AVAILABLE\n" 1>>"${BASEDIR}"/build.log 2>&1

./Configure \
  --prefix="${LIB_INSTALL_PREFIX}" \
  --with-zlib-include="${ZLIB_INCLUDE_DIRECTORY}" \
  --with-zlib-lib="${ZLIB_LIBRARY_DIRECTORY}" \
  zlib \
  no-shared \
  no-engine \
  no-dso \
  no-legacy \
  ${ASM_OPTIONS} \
  no-tests || return 1

make -j$(get_cpu_count) build_sw || return 1

make install_sw install_ssldirs || return 1

# MANUALLY COPY PKG-CONFIG FILES
cp ./exporters/*.pc "${INSTALL_PKG_CONFIG_DIR}" || return 1
