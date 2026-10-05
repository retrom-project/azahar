# Extend the upstream linker recipe, keeping the core and bridge compilation owned by this fork.
LDFLAGS := $(filter-out --no-heap-copy,$(LDFLAGS))
LDFLAGS += -s ENVIRONMENT=web,worker,node -s ERROR_ON_UNDEFINED_SYMBOLS=1 -Wl,--wrap=retro_load_game -Wl,--wrap=retro_unserialize
RARCH_OBJ += /work/.cache/content-load.o
