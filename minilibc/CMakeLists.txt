add_library(minilibc STATIC bcopy.S bzero.S memcmp.S memcpy.S memmove.S 
    memset.S strcmp.S strlen.S strncmp.S memchr.c aeabi_uldivmod.S
    udivmoddi4.c)
target_compile_options(minilibc PRIVATE -fno-builtin)
