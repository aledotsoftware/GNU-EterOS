#ifndef ETEROS_USER_MODE_H
#define ETEROS_USER_MODE_H

/**
 * Jump to Ring 3.
 * @param entry_point User mode instruction pointer.
 * @param user_stack  User mode stack pointer.
 */
void enter_user_mode(void* entry_point, void* user_stack);

#endif /* ETEROS_USER_MODE_H */
