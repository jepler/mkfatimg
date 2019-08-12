OOFATSRCS := ff.c ffsystem.c ffunicode.c
OOFATOBJS := $(patsubst %.c, .o/%.o, $(OOFATSRCS))

mkfatimg: .o/main.o $(OOFATOBJS)
	gcc -g -Og -o $@ $^

CFLAGS := -g -Og -Wall
.o/ffsystem.o: CFLAGS += -include stdlib.h
.o/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	gcc $(CFLAGS) -c -o $@ $< -MD -MF ".o/$*.d" -MT "$@"

-include $(patsubst %.c, .o/%.d, $(OOFATSRCS) main.c)

.PHONY: clean
clean:
	rm -rf .o mkfatimg
