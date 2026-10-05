#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
struct retro_game_info;
static _Atomic int result;
extern bool __real_retro_load_game(const struct retro_game_info *);
void retrom_content_load_set(int value) { atomic_store(&result, value); }
EMSCRIPTEN_KEEPALIVE int retrom_content_load_result(void) { return atomic_load(&result); }
bool __wrap_retro_load_game(const struct retro_game_info *game) {
    atomic_store(&result, 0);
    bool prepared = __real_retro_load_game(game);
    // Hardware loading completes later, after context_reset -> System::Load.
    // A successful preparation must never replace its explicit completion.
    if (!prepared) {
        int pending = 0;
        atomic_compare_exchange_strong(&result, &pending, -1);
    }
    return prepared;
}

// A state load is asynchronous in the frontend, but this callback owns the
// actual deserialization result. Loading logs are not completion receipts.
static _Atomic int state_result;
extern bool __real_retro_unserialize(const void *, size_t);
EMSCRIPTEN_KEEPALIVE void retrom_state_load_begin(void) { atomic_store(&state_result, 0); }
EMSCRIPTEN_KEEPALIVE int retrom_state_load_result(void) { return atomic_load(&state_result); }
bool __wrap_retro_unserialize(const void *data, size_t size) {
    bool loaded = __real_retro_unserialize(data, size);
    atomic_store(&state_result, loaded ? 1 : -1);
    return loaded;
}
