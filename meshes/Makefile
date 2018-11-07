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
	$(DIST)/bridge.scene \
	$(DIST)/bridge.pnc \
	$(DIST)/plant.pnc \
	$(DIST)/bridge-deploy.tanim \
	$(DIST)/plant.banims \

$(DIST)/bridge-deploy.tanim : bridge.blend export-transform-animation.py
	$(BLENDER) --background --python export-transform-animation.py -- '$<' 'Seg1,Seg2,Camera' 1 60 '$@'

$(DIST)/plant.banims : plant.blend export-bone-animations.py
	$(BLENDER) --background --python export-bone-animations.py -- '$<' 'Plant' '[0,30]Wind;[100,140]Walk' '$@'

$(DIST)/%.p : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.pnc : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.pnct : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.scene : %.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- '$<' '$@'

$(DIST)/phone-bank.w : phone-bank.blend export-walkmeshes.py
	$(BLENDER) --background --python export-walkmeshes.py -- '$<':3 '$@'
