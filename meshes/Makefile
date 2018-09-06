.PHONY : all

HOSTNAME := $(shell hostname)

ifeq ($(HOSTNAME), incepchow)
	BLENDER = ~/blender-2.79b-linux-glibc219-x86_64/blender
else
	BLENDER = blender
endif

DIST=../dist

all : \
	$(DIST)/menu.p \
	$(DIST)/meshes.pnc \
	$(DIST)/crates.pnc \
	$(DIST)/crates.scene \

$(DIST)/%.p : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.pnc : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.scene : %.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- '$<' '$@'
