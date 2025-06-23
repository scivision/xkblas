# include <xkomp.h>

template <typename T>
T
xkomp_env_init_parse_convert(const char * const value)
{
    static_assert(sizeof(T) == 0, "Unsupported type for xkomp_env_init_parse");
}

template <>
int
xkomp_env_init_parse_convert<int>(const char * const value)
{
    return atoi(value);
}

template <>
char
xkomp_env_init_parse_convert<char>(const char * const value)
{
    return *value;
}

template <>
char *
xkomp_env_init_parse_convert<char *>(const char * const value)
{
    return strdup(value);
}

template <typename T>
void
xkomp_env_init_parse(
    T * var,
    const char * const name,
    const T default_value
) {
    const char * value = getenv(name);
    if (value && *value)
        *var = xkomp_env_init_parse_convert<T>(value);
    else
        *var = default_value;
}

/**
 *  Spec 6.0 says
 *  Modifications to the environment variables after the program has started,
 *  even if modified by the program itself, are ignored by the OpenMP
 *  implementation. This routine load at the program starts.
 */
void
xkomp_env_init(xkomp_env_t * env)
{
    // parse env variables
    # define F(T, S, D) xkomp_env_init_parse<T>(&env->S, #S, D)
    F(char,     OMP_DISPLAY_ENV,    'f');   // true, false or verbose ('t', 'f' or 'v')
    F(int,      OMP_NUM_THREADS,    0);
    F(int,      OMP_THREAD_LIMIT,   INT_MAX);
    F(char *,   OMP_PLACES,         "cores");
    F(char *,   OMP_PROC_BIND,      "close");
    # undef F

    // maybe display
    switch (env->OMP_DISPLAY_ENV)
    {
        case ('f'):
            break ;

        case ('t'):
        case ('v'):
        {
            LOGGER_IMPL("-- XKOMP environment variables --");
            break ;
        }

        default:
            LOGGER_ERROR("Invalid `OMP_DISPLAY_ENV`");
    }
}
