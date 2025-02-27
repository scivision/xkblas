all:
	clang -cc1 -triple spir-unknown-unknown -cl-std=CL3.0 -emit-llvm -o diffusion.ll diffusion.cl -I ../../include
	llvm-as diffusion.ll -o diffusion.bc
	llvm-spirv diffusion.bc -o diffusion.spv
	cat diffusion.spv | xxd -p -c 12 | sed 's/../0x&, /g' > diffusion.spv.bytes

