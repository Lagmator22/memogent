#!/usr/bin/env bash
# Bootstrap the Python reference runtime + build the C++ core.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "${SCRIPT_DIR}")"

echo "==> installing Python reference runtime"
cd "${ROOT}/python"
make install

echo "==> building C++ core"
cd "${ROOT}"
mkdir -p build && cd build
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

echo "==> done. try:"
echo "    ./build/examples/cpp_cli/mem demo"
echo "    cd python && make bench"
