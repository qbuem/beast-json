#!/bin/bash
# gen_coverage.sh – Generate HTML coverage report for libFuzzer targets.

set -e

LLVM_PATH="/opt/homebrew/opt/llvm@21/bin"
CLANG_PP="$LLVM_PATH/clang++"
LLVM_PROFDATA="$LLVM_PATH/llvm-profdata"
LLVM_COV="$LLVM_PATH/llvm-cov"

REPORT_DIR="fuzz/coverage_report"
MERGED_PROF="fuzz/fuzzer.profdata"

echo "🧹 Clearing previous build cache..."
rm -rf build-coverage

echo "🔨 Building with coverage instrumentation (using Homebrew LLVM@21)..."
cmake -B build-coverage \
      -DQBUEM_JSON_BUILD_FUZZ=ON \
      -DQBUEM_JSON_BUILD_TESTS=OFF \
      -DQBUEM_JSON_BUILD_BENCHMARKS=OFF \
      -DCMAKE_CXX_COMPILER="$CLANG_PP" \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
      -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate"
cmake --build build-coverage -j$(sysctl -n hw.ncpu)

echo "🏃 Running fuzzers to collect profiles..."
# Run each target for a few seconds to generate raw profiles
# Note: macOS find needs -maxdepth immediately after the path
TARGETS=$(find build-coverage/fuzz -maxdepth 1 -type f -name "fuzz_*" -perm +111)

for t in $TARGETS; do
    echo "  Running $(basename $t)..."
    # Create target-specific corpus dir if needed
    CORPUS_DIR="fuzz/corpus_$(basename $t)"
    mkdir -p "$CORPUS_DIR"
    cp fuzz/corpus/*.json "$CORPUS_DIR/" 2>/dev/null || true
    
    # Run for longer to get deeper coverage
    LLVM_PROFILE_FILE="fuzz/$(basename $t).profraw" $t "$CORPUS_DIR" -max_total_time=30 -dict=fuzz/json.dict || true
    if [ ! -f "fuzz/$(basename $t).profraw" ]; then
        echo "  ❌ ERROR: No profile generated for $(basename $t)"
    fi
done

echo "🔗 Merging profiles..."
"$LLVM_PROFDATA" merge -sparse fuzz/*.profraw -o $MERGED_PROF

echo "📊 Generating HTML report..."
mkdir -p $REPORT_DIR
# Use ALL binaries for coverage calculation to see the union of coverage
"$LLVM_COV" show build-coverage/fuzz/fuzz_parse \
    $(find build-coverage/fuzz -maxdepth 1 -type f -name "fuzz_*" -perm +111 | sed 's/^/-object /' | tr '\n' ' ') \
    -instr-profile=$MERGED_PROF \
    -format=html \
    -output-dir=$REPORT_DIR \
    -show-line-counts-or-regions \
    -show-instantiations \
    -ignore-filename-regex="(_deps|test|fuzz)"

echo "✅ Done! Report available at: $REPORT_DIR/index.html"
