#ifndef __ACCESS_H__
# define __ACCESS_H__

typedef enum    access_mode_e
{
    IN          = 0,
    OUT         = 1,
    // OUTSET   = 2 // TODO
    IRRELEVANT  = 127
}               access_mode_t;

#endif /* __ACCESS_H__ */
