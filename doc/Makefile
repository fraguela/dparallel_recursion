all: info

info:
	@echo \'make doc\' for user-level documentation, or
	@echo \'make internal-doc\' for developer documentation

doc: Doxyfile
	doxygen $<

internal-doc : Doxyfile.internal
	doxygen $<

clean:
	-@rm -rf html latex man
