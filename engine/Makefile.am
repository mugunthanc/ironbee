ACLOCAL_AMFLAGS = -I ../../acinclude

include $(top_srcdir)/build/common.mk

EXTRA_DIST =  config-parser.rl run-ragel.py \
              engine_private.h  \
              state_notify_private.h \
              rule_engine_private.h \
              managed_collection_private.h \
              core_private.h \
              core_audit_private.h

lib_LTLIBRARIES = libironbee.la
libironbee_la_SOURCES = engine.c provider.c parser.c data.c \
                        context_selection.c site.c \
                        managed_collection.c \
                        config.c config-parser.c config-parser.h \
                        matcher.c filter.c \
                        operator.c action.c transformation.c \
                        module.c \
                        parsed_content.c \
                        capture.c \
                        core.c \
                        core_context_selection.c \
                        core_collection_managers.c \
                        core_fields.c \
                        core_tfns.c \
                        core_operators.c \
                        core_actions.c \
                        core_audit.c \
                        log.c \
                        logevent.c \
                        rule_logger.c \
                        rule_engine.c \
                        state_notify.c \
                        config-parser.h \
                        $(top_builddir)/lua/ironbee.h
libironbee_la_CFLAGS = $(AM_CFLAGS) $(HTP_CFLAGS) $(PCRE_CFLAGS)
libironbee_la_LIBADD = $(top_builddir)/util/libibutil.la $(PCRE_LDADD)
libironbee_la_LDFLAGS = $(AM_LDFLAGS) $(HTP_LDFLAGS) \
                        -version-info @LIBRARY_VERSION@ \
                        -release @MAJOR_VERSION@.@MINOR_VERSION@

if DARWIN
install-exec-hook: $(lib_LTLIBRARIES)
	@for l in $(lib_LTLIBRARIES); do \
	  base=`echo $$l | sed 's/\..*//'`; \
	  echo "rebasing $$base..."; \
	  rebase -v $(DESTDIR)$(libdir)/$$base.dylib; \
	done
endif

# Ragel generated files
RUNRAGEL=$(srcdir)/run-ragel.py
$(srcdir)/config-parser.c: $(srcdir)/config-parser.rl
	$(RUNRAGEL) "$(RAGEL)" $*

config-parser.dot: $(srcdir)/config-parser.rl
	$(RAGEL) -V $< > $@

config-parser.svg: config-parser.dot
	dot -Tsvg $<  > $@
