OOFATSRCS := ff.c ffsystem.c ffunicode.c
OOFATOBJS := $(patsubst %.c, .o/%.o, $(OOFATSRCS))

mkfatimg: .o/main.o $(OOFATOBJS)
	gcc -g -Og -o $@ $^

.o/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	gcc -g -Og -c -o $@ $< -MD -MF ".o/$*.d" -MT "$@"

-include $(patsubst %.c, .o/%.d, $(OOFATSRCS) main.c)

.PHONY: clean
clean:
	rm -rf .o mkfatimg
