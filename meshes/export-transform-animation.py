#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

import sys
import bpy
import struct
import bmesh
import mathutils


# File format:
# str0 len < char > * [strings chunk]
# idx0 len < uint uint > * [object names]
# xff0 len < trs trs ... > * [transform animation frames, one per object]

strings_data = b''
index_data = b''
frames_data = b''

args = []
for i in range(0,len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]

if len(args) != 5:
	print("\n\nUsage:\nblender --background --python export-animation.py -- <infile.blend> <object name,object name,...> <minFrame> <maxFrame> <outfile.manim>\nExports transforms for range [minFrame,maxFrame] animation to a binary blob. Exported transforms are relative to the parent transform but otherwise absolute.\n")
	exit(1)

infile = args[0]
object_names = args[1].split(",")
min_frame = int(args[2])
max_frame = int(args[3])
outfile = args[4]

print("Will read '" + infile + "' and dump frame [" + str(min_frame) + "," + str(max_frame) + "] for '" + "', '".join(object_names) + "' to '" + outfile + "'.")

bpy.ops.wm.open_mainfile(filepath=infile)

objs = []
for object_name in object_names:
	assert(object_name in bpy.data.objects)
	obj = bpy.data.objects[object_name]
	objs.append(obj)
	#write index entry for object:
	begin = len(strings_data)
	strings_data += bytes(obj.name, 'utf8')
	end = len(strings_data)
	index_data += struct.pack('II', begin, end)

#-----------------------------
#write out Translation-Rotation-Scale for each frame:

#write frame:
def write_frame():
	global frames_data
	for obj in objs:
		mat = obj.matrix_world
		#express transformation relative to parent:
		if obj.parent:
			world_to_parent = obj.parent.matrix_world.copy()
			world_to_parent.invert()
			mat = world_to_parent * mat
		trs = mat.decompose() #turn into (translation, rotation, scale)
		frames_data += struct.pack('3f', trs[0].x, trs[0].y, trs[0].z)
		frames_data += struct.pack('4f', trs[1].x, trs[1].y, trs[1].z, trs[1].w)
		frames_data += struct.pack('3f', trs[2].x, trs[2].y, trs[2].z)


for frame in range(min_frame, max_frame+1):
	bpy.context.scene.frame_set(frame, 0.0) #note: second param is sub-frame
	write_frame()

print("Wrote frames [" + str(min_frame) + ", " + str(max_frame) + "]")

#----------------------------
#Write data to file

#write the strings chunk and scene chunk to an output blob:
blob = open(outfile, 'wb')
def write_chunk(magic, data):
	blob.write(struct.pack('4s',magic)) #type
	blob.write(struct.pack('I', len(data))) #length
	blob.write(data)

write_chunk(b'str0', strings_data)
write_chunk(b'idx0', index_data)
write_chunk(b'xff0', frames_data)

print("Wrote " + str(blob.tell()) + " bytes [== "
	+ str(len(strings_data)) + " bytes of strings + "
	+ str(len(index_data)) + " bytes of index + "
	+ str(len(frames_data)) + " bytes of frames]"
	+ " to '" + outfile + "'")
blob.close()
