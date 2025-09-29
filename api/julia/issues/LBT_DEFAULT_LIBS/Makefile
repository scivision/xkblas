all:
	clang -fPIC -shared -o libopenblas_custom.so wrapper.c
	LBT_FORCE_RETSTYLE="normal" LBT_FORCE_F2C="plain" LBT_DEFAULT_LIBS=./libopenblas_custom.so julia main.jl
