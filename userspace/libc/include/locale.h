#ifndef _LOCALE_H
#define _LOCALE_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

static inline char *setlocale(int category, const char *locale) {
    return "C";
}

static inline struct lconv *localeconv(void) {
    static struct lconv l = {
        .decimal_point = ".",
        .thousands_sep = "",
        .grouping = "",
        .int_curr_symbol = "",
        .currency_symbol = "",
        .mon_decimal_point = "",
        .mon_thousands_sep = "",
        .mon_grouping = "",
        .positive_sign = "",
        .negative_sign = "",
        .int_frac_digits = 127,
        .frac_digits = 127,
        .p_cs_precedes = 127,
        .p_sep_by_space = 127,
        .n_cs_precedes = 127,
        .n_sep_by_space = 127,
        .p_sign_posn = 127,
        .n_sign_posn = 127,
        .int_p_cs_precedes = 127,
        .int_p_sep_by_space = 127,
        .int_n_cs_precedes = 127,
        .int_n_sep_by_space = 127,
        .int_p_sign_posn = 127,
        .int_n_sign_posn = 127
    };
    return &l;
}

#endif /* _LOCALE_H */
