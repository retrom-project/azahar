#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
struct retro_game_info;
extern bool __wrap_retro_load_game(const struct retro_game_info *);
extern int retrom_content_load_result(void);
extern void retrom_content_load_set(int);
static int outcome;
bool __real_retro_load_game(const struct retro_game_info *game) {
    (void)game;
    if (outcome == 2) retrom_content_load_set(-2);
    return outcome != 1;
}
extern void retrom_state_load_begin(void);
extern int retrom_state_load_result(void);
extern bool __wrap_retro_unserialize(const void *, size_t);
bool __real_retro_unserialize(const void *data, size_t size) { (void)data; return size == 1; }
int main(void) {
    retrom_state_load_begin();
    assert(retrom_state_load_result() == 0);
    assert(__wrap_retro_unserialize(0, 1));
    assert(retrom_state_load_result() == 1);
    retrom_state_load_begin();
    assert(retrom_state_load_result() == 0);
    assert(!__wrap_retro_unserialize(0, 2));
    assert(retrom_state_load_result() == -1);
    assert(retrom_content_load_result() == 0);
    assert(__wrap_retro_load_game(0));
    assert(retrom_content_load_result() == 0);
    retrom_content_load_set(1);
    assert(retrom_content_load_result() == 1);
    outcome = 1;
    assert(!__wrap_retro_load_game(0));
    assert(retrom_content_load_result() == -1);
    outcome = 2;
    assert(__wrap_retro_load_game(0));
    assert(retrom_content_load_result() == -2);
}
