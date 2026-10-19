#!/usr/bin/env bash
set -euo pipefail

# Match the working Windows toolchain. MSYS2 builds the compiler packages and
# C++ runtime packages separately, so their package release numbers differ.
llvm_version=22.1.8
case "${MSYSTEM:-}" in
  CLANG64)
    repository=clang64
    package_prefix=mingw-w64-clang-x86_64
    ;;
  CLANGARM64)
    repository=clangarm64
    package_prefix=mingw-w64-clang-aarch64
    ;;
  *)
    echo "LLVM ${llvm_version} requires MSYS2 CLANG64 or CLANGARM64" >&2
    exit 1
    ;;
esac

package_specs=(
  clang:2 clang-libs:2 compiler-rt:2 lld:2
  llvm:2 llvm-libs:2 llvm-tools:2
  libc++:1 libunwind:1
)

# These optional packages require the same compiler version if already present
# in the runner image or setup-msys2 cache.
for package in clang-analyzer clang-tools-extra gcc-compat; do
  if pacman -Q "${package_prefix}-${package}" >/dev/null 2>&1; then
    package_specs+=("${package}:2")
  fi
done

package_urls=()
expected_packages=()
for spec in "${package_specs[@]}"; do
  package="${package_prefix}-${spec%:*}"
  version="${llvm_version}-${spec##*:}"
  package_urls+=("https://repo.msys2.org/mingw/${repository}/${package}-${version}-any.pkg.tar.zst")
  expected_packages+=("${package} ${version}")
done

# Install the complete set in one transaction, with pacman's normal dependency
# and signature checks. No package updates should run after this step.
pacman --noconfirm -U "${package_urls[@]}"

for expected in "${expected_packages[@]}"; do
  installed=$(pacman -Q "${expected%% *}")
  echo "${installed}"
  if [[ "${installed}" != "${expected}" ]]; then
    echo "Expected ${expected}, found ${installed}" >&2
    exit 1
  fi
done

clang --version
ld.lld --version
llvm-ar --version

# Check that the native build tools still run against the selected C++ runtime.
cmake --version
meson --version
ninja --version
pkg-config --version

# Exercise the compiler, headers, linker, and exception runtime before spending
# time on the full dependency build.
check_dir=$(mktemp -d)
trap 'rm -rf "${check_dir}"' EXIT
cat >"${check_dir}/check.cpp" <<'CPP'
#include <exception>
#include <stdexcept>

#if __clang_major__ != 22 || !defined(_LIBCPP_VERSION) || _LIBCPP_VERSION / 10000 != 22
#error Expected Clang 22 and libc++ 22
#endif

int main() {
  std::exception_ptr error;
  try {
    throw std::runtime_error("LLVM 22 exception check");
  } catch (...) {
    error = std::current_exception();
  }
  try {
    std::rethrow_exception(error);
  } catch (const std::runtime_error &) {
    return 0;
  }
  return 1;
}
CPP
clang++ -std=c++11 "${check_dir}/check.cpp" -o "${check_dir}/check.exe"
"${check_dir}/check.exe"
