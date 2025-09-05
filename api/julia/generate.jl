using Clang.Generators
using Clang.LibClang.Clang_jll
using ArgParse

function main()
    s = ArgParseSettings()
    @add_arg_table s begin
        "--input", "-i"
            help = "Input directory containing xkblas headers"
            arg_type = String
            required = true
    end

    parsed_args = parse_args(ARGS, s)
    xkblas_dir = parsed_args["input"]

    cd(@__DIR__)

    # Load generator options (must include type_map and rename_functions)
    options = load_options(joinpath(@__DIR__, "generator.toml"))

    # Collect all headers
    headers = [joinpath(xkblas_dir, f) for f in readdir(xkblas_dir) if endswith(f, ".h")]

    # Default compiler flags
    args = get_default_args()
    push!(args, "-I$xkblas_dir")

    # Create context: only two positional arguments are supported in this version
    ctx = create_context(headers, args, options)

    # Generate bindings
    build!(ctx)

    println("bindings.jl generated successfully in src/")
end

main()

