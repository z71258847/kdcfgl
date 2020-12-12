PATH2LIB=~/kdcfgl/build/kdcfgl/LLVMPJT.so        # Specify your build directory in the project
PASS=-kdcfgl                   # Choose either -fplicm-correctness or -fplicm-performance

# Delete outputs from previous run.
rm -f *.bc *_output

SOURCE=${1}.c
if [[ -f ${1}.cpp ]]; then
    SOURCE=${1}.cpp
fi

# Convert source code to bitcode (IR)
clang -emit-llvm -O0 -Xclang -disable-O0-optnone -c $SOURCE -o ${1}.bc
# Get rid of switch table
opt -lowerswitch ${1}.bc -o ${1}.nosw.bc
# Canonicalize natural loops
opt -loop-simplify ${1}.nosw.bc -o ${1}.ls.bc
# Mem2reg pass
opt -mem2reg ${1}.ls.bc -o ${1}.m2r.bc
# Instrument profiler
#opt -pgo-instr-gen -instrprof ${1}.m2r.bc -o ${1}.m2r.prof.bc
# Generate binary executable with profiler embedded
#clang -fprofile-instr-generate ${1}.m2r.prof.bc -o ${1}_prof

# Generate profiled data
#./${1}_prof > correct_output
#llvm-profdata merge -o ${1}.profdata default.profraw

# Apply kdcfgl
#opt -o ${1}.kdcfgl.bc -pgo-instr-use -pgo-test-profile-file=${1}.profdata -load ${PATH2LIB} ${PASS} < ${1}.m2r.bc > /dev/null
opt -o ${1}.kdcfgl.bc -load ${PATH2LIB} ${PASS} < ${1}.m2r.bc > /dev/null
opt -simplifycfg ${1}.kdcfgl.bc -o ${1}.merged.bc

# Generate binary excutable before FPLICM: Unoptimzied code
clang ${1}.m2r.bc -o ${1}_no_kdcfgl
# Generate binary executable after FPLICM: Optimized code
clang ${1}.merged.bc -o ${1}_kdcfgl

# Produce output from binary to check correctness
./${1}_no_kdcfgl > correct_output
./${1}_kdcfgl > kdcfgl_output

echo -e "\n=== Correctness Check ==="
if [ "$(diff correct_output kdcfgl_output)" != "" ]; then
    echo -e ">> FAIL\n"
else
    echo -e ">> PASS\n"
    # Measure performance
    echo -e "1. Performance of original code"
    time ./${1}_no_kdcfgl > /dev/null
    echo -e "\n\n"
    echo -e "2. Performance of transformed code"
    time ./${1}_kdcfgl > /dev/null
    echo -e "\n\n"
fi

#rm -rf *_*
