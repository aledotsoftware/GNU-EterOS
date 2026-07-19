<<<<<<< SEARCH
    char           cwd[256];                /* Current Working Directory */
    struct fs_node* cwd_node;               /* Current Working Directory Node */
    uint32_t       signal_mask;             /* Mask of blocked signals */
=======
    char           cwd[256];                /* Current Working Directory */
    struct fs_node* cwd_node;               /* Current Working Directory Node */
    int            nice;                    /* Base nice value (-20 to 19) */
    uint32_t       signal_mask;             /* Mask of blocked signals */
>>>>>>> REPLACE
