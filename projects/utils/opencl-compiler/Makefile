all:
	clang -cc1 -triple spir-unknown-unknown -emit-llvm -o empty-kernel.ll empty-kernel.cl
	llvm-as empty-kernel.ll -o empty-kernel.bc
	llvm-spirv empty-kernel.bc -o empty-kernel.spv
	cat empty-kernel.spv | xxd -p -c 12 | sed 's/../0x&, /g' > empty-kernel.spv.bytes
