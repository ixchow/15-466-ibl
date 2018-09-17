.PHONY : all

HOSTNAME := $(shell hostname -s)

ifeq ($(HOSTNAME), incepchow)
	BLENDER = ~/blender-2.79b-linux-glibc219-x86_64/blender
else ifeq ($(HOSTNAME), retchow)
	BLENDER=/Applications/Blender/blender.app/Contents/MacOS/blender
else
	BLENDER = blender
endif

DIST=../dist

all : \
	$(DIST)/menu.p \
	$(DIST)/paddle-ball.pnc \

$(DIST)/%.p : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.pnc : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/title.pc : phone-bank.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<':6 '$@'

$(DIST)/sky.pc : phone-bank.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<':7 '$@'

$(DIST)/phone-bank.pnc : phone-bank.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<':1 '$@'

$(DIST)/phone-bank.scene : phone-bank.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- '$<':1 '$@'

$(DIST)/phone-bank.w : phone-bank.blend export-walkmeshes.py
	$(BLENDER) --background --python export-walkmeshes.py -- '$<':3 '$@'

$(DIST)/%.scene : %.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- '$<' '$@'
