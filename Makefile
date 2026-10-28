# SalaGiochi - games for children on Haiku/RenkuOS
#
# Native:  make
# Cross:   make CXX=arm-unknown-haiku-g++ ARCHFLAGS="..." RC=rc XRES=xres

NAME := SalaGiochi
CXX ?= g++
RC ?= rc
COLLECTCATKEYS ?= collectcatkeys
LINKCATKEYS ?= linkcatkeys
SIGNATURE := application/x-vnd.atomozero-SalaGiochi
LANGUAGES := $(basename $(notdir $(filter-out locales/en.catkeys,$(wildcard locales/*.catkeys))))
XRES ?= xres
MIMESET ?= mimeset
ARCHFLAGS ?=

SOURCES := $(wildcard src/*.cpp)
OBJDIR := objects
OBJECTS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

# "Add games": the parents drop game files on it
INSTALLER := AddGames
INSTALLER_SIGNATURE := application/x-vnd.atomozero-SalaGiochi-Installer
INSTALLER_SOURCES := installer/Installer.cpp src/Systems.cpp
INSTALLER_OBJECTS := $(OBJDIR)/installer/Installer.o $(OBJDIR)/Systems.o
INSTALLER_LANGUAGES := $(basename $(notdir $(filter-out installer/locales/en.catkeys,$(wildcard installer/locales/*.catkeys))))

CXXFLAGS += -O2 -Wall -Wno-multichar -std=gnu++17 $(ARCHFLAGS)
LIBS := -lbe -lmedia -lgame -ltranslation -ldevice -llocalestub

all: $(OBJDIR)/$(NAME) $(OBJDIR)/$(INSTALLER) catalogs

$(OBJDIR)/%.o: src/%.cpp src/*.h
	@mkdir -p $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/$(NAME).rsrc: $(NAME).rdef
	@mkdir -p $(OBJDIR)
	$(RC) -o $@ $<

$(OBJDIR)/$(NAME): $(OBJECTS) $(OBJDIR)/$(NAME).rsrc
	$(CXX) $(ARCHFLAGS) -s -o $@ $(OBJECTS) $(LIBS)
	$(XRES) -o $@ $(OBJDIR)/$(NAME).rsrc
	-$(MIMESET) -f $@

$(OBJDIR)/installer/%.o: installer/%.cpp src/*.h
	@mkdir -p $(OBJDIR)/installer
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/$(INSTALLER).rsrc: installer/$(INSTALLER).rdef
	@mkdir -p $(OBJDIR)
	$(RC) -o $@ $<

$(OBJDIR)/$(INSTALLER): $(INSTALLER_OBJECTS) $(OBJDIR)/$(INSTALLER).rsrc
	$(CXX) $(ARCHFLAGS) -s -o $@ $(INSTALLER_OBJECTS) -lbe -llocalestub
	$(XRES) -o $@ $(OBJDIR)/$(INSTALLER).rsrc
	-$(MIMESET) -f $@

# installed in data/locale/catalogs/x-vnd.atomozero-SalaGiochi/ and
# .../x-vnd.atomozero-SalaGiochi-Installer/
catalogs:
	@mkdir -p $(OBJDIR)/catalogs $(OBJDIR)/catalogs-installer
	for language in $(LANGUAGES); do \
		$(LINKCATKEYS) -o $(OBJDIR)/catalogs/$$language.catalog \
			-s $(SIGNATURE) -l $$language locales/$$language.catkeys \
			|| exit 1; \
	done
	for language in $(INSTALLER_LANGUAGES); do \
		$(LINKCATKEYS) -o $(OBJDIR)/catalogs-installer/$$language.catalog \
			-s $(INSTALLER_SIGNATURE) -l $$language \
			installer/locales/$$language.catkeys || exit 1; \
	done

# the English strings, to translate from: make catkeys
catkeys:
	@mkdir -p $(OBJDIR)
	cat $(SOURCES) | $(CXX) -E -P -x c++ -DB_COLLECTING_CATKEYS $(CXXFLAGS) \
		-Isrc - > $(OBJDIR)/strings.pre
	$(COLLECTCATKEYS) -s $(SIGNATURE) -o locales/en.catkeys \
		$(OBJDIR)/strings.pre
	cat $(INSTALLER_SOURCES) | $(CXX) -E -P -x c++ -DB_COLLECTING_CATKEYS \
		$(CXXFLAGS) -Isrc - > $(OBJDIR)/installer-strings.pre
	$(COLLECTCATKEYS) -s $(INSTALLER_SIGNATURE) \
		-o installer/locales/en.catkeys $(OBJDIR)/installer-strings.pre

clean:
	rm -rf $(OBJDIR)

.PHONY: all clean catkeys catalogs
