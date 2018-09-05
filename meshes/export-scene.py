#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

#Note: Script meant to be executed from within blender, as per:
#blender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.scene>

import sys,re

args = []
for i in range(0,len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]

if len(args) != 2:
	print("\n\nUsage:\nblender --background --python export-meshes.py -- <infile.blend>[:layer] <outfile.scene>\nExports the transforms of objects in layer layer (default 1) to a binary blob, indexed by the names of the objects that reference them.\n")
	exit(1)

infile = args[0]
layer = 1
m = re.match(r'^(.*):(\d+)$', infile)
if m:
	infile = m.group(1)
	layer = int(m.group(2))
outfile = args[1]

assert layer >= 1 and layer <= 20

print("Will export transforms from layer " + str(layer) + " of '" + infile + "' to '" + outfile + "'.")

import bpy
import struct

#---------------------------------------------------------------------
#Export scene (object positions for every object on layer one)

bpy.ops.wm.open_mainfile(filepath=infile)

to_write = []
for obj in bpy.data.objects:
	if obj.layers[layer-1] == False: continue
	if obj.type != 'MESH': continue
	to_write.append(obj.name)

#strings chunk will have names
strings = b''
name_begin = dict()
name_end = dict()
for name in to_write:
	name_begin[name] = len(strings)
	strings += bytes(name, 'utf8')
	name_end[name] = len(strings)

#scene chunk will have transforms + indices into strings for name
scene = b''
for name in to_write:
	obj = bpy.data.objects[name]
	assert(obj.name in name_begin)
	scene += struct.pack('I', name_begin[obj.name])
	scene += struct.pack('I', name_end[obj.name])
	transform = obj.matrix_world.decompose()
	scene += struct.pack('3f', transform[0].x, transform[0].y, transform[0].z)
	scene += struct.pack('4f', transform[1].x, transform[1].y, transform[1].z, transform[1].w)
	scene += struct.pack('3f', transform[2].x, transform[2].y, transform[2].z)

#write the strings chunk and scene chunk to an output blob:
blob = open(outfile, 'wb')
#first chunk: the strings
blob.write(struct.pack('4s',b'str0')) #type
blob.write(struct.pack('I', len(strings))) #length
blob.write(strings)
#second chunk: the scene
blob.write(struct.pack('4s',b'scn0')) #type
blob.write(struct.pack('I', len(scene))) #length
blob.write(scene)
wrote = blob.tell()
blob.close()

print("Wrote " + str(wrote) + " bytes to '" + outfile + "'")
