#ifdef DEBUG

#define FASSERT(cond, retval)  do {                      \
    if (!(cond)) {                                       \
            pr_err("Assertion failed: %s at %s:%d\n",    \
                   #cond, __FILE__, __LINE__);           \
            return (retval);                             \
    }                                                    \
} while (0);


#else  // DEBUG

#define FASSERT(cond, retval) do {} while(0)

// Disable `pr_debug` when not DEBUG
#undef pr_debug
#define pr_debug(...) do {} while(0)

#endif  // DEBUG
