SHACC := shacc

OUTPUTS := sphere_f.gxp sphere_v.gxp

all: $(OUTPUTS)

%_f.gxp: %_f.cg
	$(SHACC) --fragment $< $@

%_v.gxp: %_v.cg
	$(SHACC) --vertex $< $@

clean:
	$(RM) $(OUTPUTS)
