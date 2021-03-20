/*
 * this macro is used to silence nan warnings added with cast-function-type and introduced in GCC 8 (https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)
 * macro from Alba Mendez (@mildsunrise) https://github.com/nodejs/nan/issues/807#issuecomment-581536991
 * see also https://github.com/meldron/node-alsa-capture/issues/5
 */

#if defined(__GNUC__) && __GNUC__ >= 8
#define DISABLE_WCAST_FUNCTION_TYPE _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wcast-function-type\"")
#define DISABLE_WCAST_FUNCTION_TYPE_END _Pragma("GCC diagnostic pop")
#else
#define DISABLE_WCAST_FUNCTION_TYPE
#define DISABLE_WCAST_FUNCTION_TYPE_END
#endif