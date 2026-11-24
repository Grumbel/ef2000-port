#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later WITH GPL-3.0-linking-exception
"""Apply DID's 1996/97 binary updates without running the original patcher.

DID's standalone patches (``tactpat1.exe``, ``Tactpgf.exe`` and, by
layout, ``efpat24.exe`` and ``PATCHEF.EXE``) are a 16-bit DOS patcher
followed by its patch data. The GFX+ updater (``Tactpat.exe``) embeds
three of them. This tool locates them and reimplements the command
format statically. It never writes into the
source installation: results go into a separate output directory, and
unchanged files are read from the source on demand. Files already in the
output directory take precedence, so stages chain into one directory.

    didpatch.py list Tactpat.exe
    didpatch.py apply Tactpat.exe v20 SRC OUT
    didpatch.py apply Tactpat.exe 3dfx SRC OUT   # OUT already holds v20
    didpatch.py apply TACPAT1.EXE file SRC OUT   # standalone DID patch

Patch data layout (argv[0] of the DOS patcher): an 8-byte trailer holds
u32 ``ncmd`` and ``ndata``; the opcode stream is the ``ncmd`` bytes
before the trailer and the argument stream the ``ndata`` bytes before
that. Checksums are CRC-32 (0xedb88320) with zero init and no final xor.
"""

import argparse
import os
import shutil
import struct
import sys
import zlib

STAGES = {
	"v20": b"update to EF2000 V2.0",
	"3dfx": b"Tactcom 2.0 to 3DFX update",
	"rendition": b"Tactcom 2.0 to Rendition update",
}


def did_crc(data):
	return zlib.crc32(data, 0xffffffff) ^ 0xffffffff


def find_stage(image, title):
	start = image.find(title)
	if start < 0:
		raise SystemExit("stage not found: %r" % title)
	for trailer in range(start, len(image) - 7):
		ncmd, ndata = struct.unpack_from("<II", image, trailer)
		if ncmd and ndata and trailer - ncmd - ndata == start:
			return image[start:trailer + 8]
	raise SystemExit("no trailer for stage %r" % title)


class Patch:
	def __init__(self, blob, source, out, log=print):
		ncmd, ndata = struct.unpack_from("<II", blob, len(blob) - 8)
		end = len(blob) - 8
		self.cmd = blob[end - ncmd:end]
		self.dat = blob[end - ncmd - ndata:end - ncmd]
		self.ci = self.di = 0
		self.source = os.path.realpath(source)
		self.out = os.path.realpath(out)
		self.log = log
		self.target = None
		self.target_name = None
		self.src = None
		self.sp = 0

	def byte(self):
		value = self.dat[self.di]
		self.di += 1
		return value

	def u16be(self):
		value = (self.dat[self.di] << 8) | self.dat[self.di + 1]
		self.di += 2
		return value

	def u32(self):
		value = struct.unpack_from("<I", self.dat, self.di)[0]
		self.di += 4
		return value

	def string(self):
		end = self.dat.index(0, self.di)
		value = self.dat[self.di:end].decode("latin1")
		self.di = end + 1
		return value

	def count(self, kind):
		return (1, 2)[kind] if kind < 2 else (self.byte() + 3 if kind == 2 else self.u16be() + 0x103)

	def lookup(self, name, write=False):
		parts = name.replace("\\", "/").split("/")
		for base in [self.out] if write else [self.out, self.source]:
			path = base
			for part in parts:
				entries = os.listdir(path) if os.path.isdir(path) else []
				match = [e for e in entries if e.lower() == part.lower()]
				if not match:
					break
				path = os.path.join(path, match[0])
			else:
				return path
		return os.path.join(self.out, *parts) if write else None

	def in_out(self, path):
		return os.path.commonpath([os.path.realpath(path), self.out]) == self.out

	def run(self):
		os.makedirs(self.out, exist_ok=True)
		while True:
			op = self.cmd[self.ci]
			self.ci += 1
			if op == 0:
				return
			elif op == 1:
				self.log(self.string().rstrip())
			elif op == 2:
				self.target_name = self.string()
				self.u32()
				self.target = bytearray()
			elif op == 3:
				size, crc = self.u32(), self.u32()
				if len(self.target) != size or did_crc(self.target) != crc:
					raise SystemExit("checksum failure on %s" % self.target_name)
				path = self.lookup(self.target_name, True)
				os.makedirs(os.path.dirname(path), exist_ok=True)
				with open(path, "wb") as f:
					f.write(self.target)
				self.target = None
			elif op == 4:
				name = self.string()
				with open(self.lookup(name), "rb") as f:
					self.src = f.read()
				self.sp = 0
				self.log("patching %s" % name)
			elif op == 5:
				self.src = None
			elif op == 6:
				old, new = self.string(), self.string()
				old_path, new_path = self.lookup(old), self.lookup(new, True)
				os.makedirs(os.path.dirname(new_path), exist_ok=True)
				if os.path.exists(new_path):
					os.remove(new_path)
				if self.in_out(old_path):
					os.replace(old_path, new_path)
				else:
					shutil.copyfile(old_path, new_path)
			elif op == 7:
				name = self.string()
				alternatives = [self.string() for _ in range(self.byte())]
				if not all(self.lookup(a) for a in alternatives):
					raise SystemExit("couldn't find %s" % name)
			elif op == 8:
				self.u32()
			elif op == 9:
				name = self.string()
				old_size, old_crc, new_size, new_crc = (self.u32() for _ in range(4))
				path = self.lookup(name)
				data = open(path, "rb").read() if path else b""
				found = (len(data), did_crc(data))
				if found == (new_size, new_crc):
					raise SystemExit("%s is already patched" % name)
				if found != (old_size, old_crc):
					raise SystemExit("unable to patch %s: size %d crc %08x, want %d %08x"
						% (name, found[0], found[1], old_size, old_crc))
				self.log("verified %s" % name)
			elif 10 <= op <= 13:
				n = self.count(op - 10)
				self.target += self.dat[self.di:self.di + n]
				self.di += n
			elif 14 <= op <= 17:
				n = self.count(op - 14)
				self.target += self.src[self.sp:self.sp + n]
				self.sp += n
			elif 18 <= op <= 21:
				self.sp += self.count(op - 18)
			elif 22 <= op <= 25:
				self.sp -= self.count(op - 22)
			elif 26 <= op <= 29:
				n = self.count(op - 26)
				self.sp += n
				self.target += self.dat[self.di:self.di + n]
				self.di += n
			elif op == 30:
				self.sp += self.u32()
			elif op == 31:
				self.sp -= self.u32()
			else:
				raise SystemExit("unknown patch command %d" % op)


def main():
	parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
	sub = parser.add_subparsers(dest="command", required=True)
	p_list = sub.add_parser("list")
	p_list.add_argument("updater")
	p_apply = sub.add_parser("apply")
	p_apply.add_argument("updater")
	p_apply.add_argument("stage", choices=sorted(STAGES) + ["file"],
		help="an updater stage, or 'file' for a standalone patch executable")
	p_apply.add_argument("source")
	p_apply.add_argument("out")
	args = parser.parse_args()
	with open(args.updater, "rb") as f:
		image = f.read()
	if args.command == "list":
		for name, title in STAGES.items():
			blob = find_stage(image, title)
			print("%-10s offset 0x%x length 0x%x" % (name, image.find(title), len(blob)))
		return
	source, out = os.path.realpath(args.source), os.path.realpath(args.out)
	if out == source or out.startswith(source + os.sep):
		raise SystemExit("output must not be inside the source installation")
	blob = image if args.stage == "file" else find_stage(image, STAGES[args.stage])
	Patch(blob, source, out).run()


if __name__ == "__main__":
	sys.exit(main())
