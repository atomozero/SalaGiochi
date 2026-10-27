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

CXXFLAGS += -O2 -Wall -Wno-multichar -std=gnu++17 $(ARCHFLAGS)
LIBS := -lbe -lmedia -lgame -ltranslation -ldevice -llocalestub

all: $(OBJDIR)/$(NAME) catalogs

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

# installed in data/locale/catalogs/x-vnd.atomozero-SalaGiochi/
catalogs:
	@mkdir -p $(OBJDIR)/catalogs
	for language in $(LANGUAGES); do \
		$(LINKCATKEYS) -o $(OBJDIR)/catalogs/$$language.catalog \
			-s $(SIGNATURE) -l $$language locales/$$language.catkeys \
			|| exit 1; \
	done

# the English strings, to translate from: make catkeys
catkeys:
	@mkdir -p $(OBJDIR)
	cat $(SOURCES) | $(CXX) -E -P -x c++ -DB_COLLECTING_CATKEYS $(CXXFLAGS) \
		-Isrc - > $(OBJDIR)/strings.pre
	$(COLLECTCATKEYS) -s $(SIGNATURE) -o locales/en.catkeys \
		$(OBJDIR)/strings.pre

clean:
	rm -rf $(OBJDIR)

.PHONY: all clean catkeys catalogs
