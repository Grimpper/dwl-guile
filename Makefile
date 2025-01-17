include config.mk

CFLAGS += -I. -DWLR_USE_UNSTABLE -D_POSIX_C_SOURCE=200809L -Wno-declaration-after-statement -Wno-unused-label -Wno-error=implicit-function-declaration $(XWAYLAND) $(DEBUGFLAGS) -DPREFIX=\"$(PREFIX)\"

WAYLAND_PROTOCOLS=$(shell pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_SCANNER=$(shell pkg-config --variable=wayland_scanner wayland-scanner)

PKGS = wlroots wayland-server wayland-client xkbcommon libinput guile-3.0 pixman-1 $(XLIBS)
CFLAGS += $(foreach p,$(PKGS),$(shell pkg-config --cflags $(p)))
LDLIBS += $(foreach p,$(PKGS),$(shell pkg-config --libs $(p)))

all: dwl

clean:
	rm -f dwl *.o *-protocol.h *-protocol.c

install: dwl
	mv dwl dwl-guile
	install -Dm755 dwl-guile $(PREFIX)/bin/dwl-guile
	install -Dm644 doc/dwl-guile.1.man $(MANDIR)/man1/dwl-guile.1
	cp -r share $(PREFIX)/share/dwl-guile

.PHONY: all clean install

# wayland-scanner is a tool which generates C headers and rigging for Wayland
# protocols, which are specified in XML. wlroots requires you to rig these up
# to your build system yourself and provide them in the include path.
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		$(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@

xdg-shell-protocol.o: xdg-shell-protocol.h

wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/wlr-layer-shell-unstable-v1.xml $@

wlr-layer-shell-unstable-v1-protocol.o: wlr-layer-shell-unstable-v1-protocol.h

dscm-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) server-header \
		protocols/dscm-unstable-v1.xml $@

dscm-unstable-v1-protocol-client.h:
	$(WAYLAND_SCANNER) client-header \
		protocols/dscm-unstable-v1.xml $@

dscm-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code \
		protocols/dscm-unstable-v1.xml $@

dscm-unstable-v1-protocol.o: dscm-unstable-v1-protocol.h
dscm-unstable-v1-protocol-client.o: dscm-unstable-v1-protocol-client.h

dscm/ipc.o: dscm-unstable-v1-protocol-client.h util.h

dwl.o: dscm/shared.h dscm/utils.h dscm/bindings.h dscm/config.h dscm/ipc.h client.h xdg-shell-protocol.h wlr-layer-shell-unstable-v1-protocol.h dscm-unstable-v1-protocol.h util.h

dwl: xdg-shell-protocol.o wlr-layer-shell-unstable-v1-protocol.o util.o dscm-unstable-v1-protocol.o dscm/ipc.o
